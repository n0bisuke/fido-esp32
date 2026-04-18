#include "authenticator.h"
#include "ctap2.h"
#include "crypto_wrapper.h"
#include "key_storage.h"
#include <Arduino.h>
#include <cbor.h>
#include <string.h>

// --- CTAP2 error codes not in ctap2.h yet ---
#define CTAP2_ERR_UNSUPPORTED_ALGORITHM 0x26
#define CTAP2_ERR_NO_CREDENTIALS        0x2D

// --- Helper: find text string value in a CBOR map by integer key ---
static bool cbor_map_get_text(CborValue *map, uint8_t key, char *out, size_t out_max) {
  CborValue it;
  cbor_value_enter_container(map, &it);
  while (!cbor_value_at_end(&it)) {
    uint64_t k;
    if (cbor_value_is_unsigned_integer(&it)) {
      cbor_value_get_uint64(&it, &k);
      cbor_value_advance(&it);
      if (k == key && cbor_value_is_text_string(&it)) {
        size_t len = out_max - 1;
        cbor_value_copy_text_string(&it, out, &len, NULL);
        out[len] = '\0';
        return true;
      }
    } else {
      cbor_value_advance(&it);
    }
    cbor_value_advance(&it);
  }
  return false;
}

// --- Helper: find byte string value in a CBOR map by integer key ---
static bool cbor_map_get_bytes(CborValue *map, uint8_t key, uint8_t *out, size_t *out_len) {
  CborValue it;
  cbor_value_enter_container(map, &it);
  while (!cbor_value_at_end(&it)) {
    uint64_t k;
    if (cbor_value_is_unsigned_integer(&it)) {
      cbor_value_get_uint64(&it, &k);
      cbor_value_advance(&it);
      if (k == key && cbor_value_is_byte_string(&it)) {
        cbor_value_copy_byte_string(&it, out, out_len, NULL);
        return true;
      }
    } else {
      cbor_value_advance(&it);
    }
    cbor_value_advance(&it);
  }
  return false;
}

// --- Helper: find integer value in a CBOR map by integer key ---
static bool cbor_map_get_int(CborValue *map, uint8_t key, int64_t *out) {
  CborValue it;
  cbor_value_enter_container(map, &it);
  while (!cbor_value_at_end(&it)) {
    uint64_t k;
    if (cbor_value_is_unsigned_integer(&it)) {
      cbor_value_get_uint64(&it, &k);
      cbor_value_advance(&it);
      if (k == key && cbor_value_is_integer(&it)) {
        cbor_value_get_int64(&it, out);
        return true;
      }
    } else {
      cbor_value_advance(&it);
    }
    cbor_value_advance(&it);
  }
  return false;
}

// --- Build COSE public key map (77 bytes) ---
// {1: 2, 3: -25, -1: 1, -2: x(32B), -3: y(32B)}
static size_t build_cose_key(const uint8_t x[32], const uint8_t y[32], uint8_t *buf) {
  CborEncoder enc, map;
  cbor_encoder_init(&enc, buf, 128, 0);
  cbor_encoder_create_map(&enc, &map, 5);
  cbor_encode_uint(&map, 1);    cbor_encode_int(&map, 2);       // kty: EC2
  cbor_encode_uint(&map, 3);    cbor_encode_int(&map, -7);      // alg: ES256
  cbor_encode_int(&map, -1);    cbor_encode_uint(&map, 1);      // crv: P-256
  cbor_encode_int(&map, -2);    cbor_encode_byte_string(&map, x, 32); // x
  cbor_encode_int(&map, -3);    cbor_encode_byte_string(&map, y, 32); // y
  cbor_encoder_close_container(&enc, &map);
  return cbor_encoder_get_buffer_size(&enc, buf);
}

// ============================================================
// authenticatorGetInfo
// ============================================================
uint8_t authenticator_get_info(uint8_t *buf, size_t *out_len) {
  buf[0] = CTAP2_OK;
  CborEncoder enc, map_enc, arr_enc, opt_enc;
  cbor_encoder_init(&enc, &buf[1], CTAPHID_MAX_MSG_SIZE - 1, 0);
  cbor_encoder_create_map(&enc, &map_enc, 5);

  cbor_encode_uint(&map_enc, 0x01);
  cbor_encoder_create_array(&map_enc, &arr_enc, 2);
  cbor_encode_text_stringz(&arr_enc, "FIDO_2_0");
  cbor_encode_text_stringz(&arr_enc, "U2F_V2");
  cbor_encoder_close_container(&map_enc, &arr_enc);

  cbor_encode_uint(&map_enc, 0x02);
  cbor_encoder_create_array(&map_enc, &arr_enc, 0);
  cbor_encoder_close_container(&map_enc, &arr_enc);

  cbor_encode_uint(&map_enc, 0x03);
  cbor_encode_byte_string(&map_enc, AAGUID, 16);

  cbor_encode_uint(&map_enc, 0x04);
  cbor_encoder_create_map(&map_enc, &opt_enc, 5);
  cbor_encode_text_stringz(&opt_enc, "rk");        cbor_encode_boolean(&opt_enc, false);
  cbor_encode_text_stringz(&opt_enc, "up");        cbor_encode_boolean(&opt_enc, true);
  cbor_encode_text_stringz(&opt_enc, "uv");        cbor_encode_boolean(&opt_enc, true);
  cbor_encode_text_stringz(&opt_enc, "plat");      cbor_encode_boolean(&opt_enc, false);
  cbor_encode_text_stringz(&opt_enc, "clientPin"); cbor_encode_boolean(&opt_enc, false);
  cbor_encoder_close_container(&map_enc, &opt_enc);

  cbor_encode_uint(&map_enc, 0x05);
  cbor_encode_uint(&map_enc, CTAPHID_MAX_MSG_SIZE);

  cbor_encoder_close_container(&enc, &map_enc);
  *out_len = 1 + cbor_encoder_get_buffer_size(&enc, &buf[1]);
  return CTAP2_OK;
}

// ============================================================
// authenticatorMakeCredential
// ============================================================
uint8_t authenticator_make_credential(const uint8_t *data, size_t data_len, uint8_t *resp, size_t *resp_len) {
  Serial.println("[MC] start");
  if (data_len < 2) { resp[0] = CTAP2_ERR_INVALID_PARAM; *resp_len = 1; return CTAP2_ERR_INVALID_PARAM; }

  CborParser parser;
  CborValue root, map;
  CborError err = cbor_parser_init(&data[1], data_len - 1, 0, &parser, &root);
  if (err != CborNoError) {
    Serial.printf("[MC] CBOR parse error: %d\n", err);
    resp[0] = CTAP2_ERR_INVALID_PARAM; *resp_len = 1; return CTAP2_ERR_INVALID_PARAM;
  }
  if (cbor_value_is_map(&root)) {
    map = root;
  } else {
    Serial.println("[MC] root is not a map");
    resp[0] = CTAP2_ERR_INVALID_PARAM; *resp_len = 1; return CTAP2_ERR_INVALID_PARAM;
  }

  // Extract clientDataHash (key 0x01)
  uint8_t client_data_hash[64];
  size_t cdh_len = sizeof(client_data_hash);
  if (!cbor_map_get_bytes(&map, 0x01, client_data_hash, &cdh_len)) {
    resp[0] = CTAP2_ERR_INVALID_PARAM; *resp_len = 1; return CTAP2_ERR_INVALID_PARAM;
  }

  // Extract rp (key 0x02) → rp.id
  char rp_id[256] = {0};
  CborValue rp_val;
  // We need to iterate the map manually for nested maps
  cbor_value_enter_container(&map, &rp_val);
  char temp_str[256];
  while (!cbor_value_at_end(&rp_val)) {
    uint64_t key;
    if (!cbor_value_is_unsigned_integer(&rp_val)) { cbor_value_advance(&rp_val); cbor_value_advance(&rp_val); continue; }
    cbor_value_get_uint64(&rp_val, &key);
    cbor_value_advance(&rp_val);

    if (key == 0x02 && cbor_value_is_map(&rp_val)) {
      CborValue rp_map;
      cbor_value_enter_container(&rp_val, &rp_map);
      while (!cbor_value_at_end(&rp_map)) {
        if (cbor_value_is_text_string(&rp_map)) {
          size_t tlen = sizeof(temp_str) - 1;
          cbor_value_copy_text_string(&rp_map, temp_str, &tlen, NULL);
          temp_str[tlen] = '\0';
          cbor_value_advance(&rp_map);
          if (strcmp(temp_str, "id") == 0 && cbor_value_is_text_string(&rp_map)) {
            size_t id_len = sizeof(rp_id) - 1;
            cbor_value_copy_text_string(&rp_map, rp_id, &id_len, NULL);
            rp_id[id_len] = '\0';
          }
        }
        cbor_value_advance(&rp_map);
      }
      cbor_value_leave_container(&rp_val, &rp_map);
    } else if (key == 0x04 && cbor_value_is_map(&rp_val)) {
      // options map — skip for now
      cbor_value_advance(&rp_val);
    } else {
      cbor_value_advance(&rp_val);
    }
  }

  if (rp_id[0] == '\0') {
    Serial.println("[MC] rp_id not found");
    resp[0] = CTAP2_ERR_INVALID_PARAM; *resp_len = 1; return CTAP2_ERR_INVALID_PARAM;
  }
  Serial.printf("[MC] rp_id=%s\n", rp_id);

  // Derive credential key and ID
  uint8_t priv_key[32], cred_id[32], pub_key[64];
  key_storage_derive_credential_key(rp_id, priv_key);
  key_storage_derive_credential_id(rp_id, cred_id);
  if (!ecdsa_key_from_private(priv_key, pub_key)) {
    Serial.println("[MC] ecdsa_key_from_private failed");
    resp[0] = CTAP2_ERR_INVALID_PARAM; *resp_len = 1; return CTAP2_ERR_INVALID_PARAM;
  }

  // Store credential
  key_storage_store_credential(rp_id, pub_key);
  Serial.println("[MC] credential stored");

  // Build authData
  uint8_t rp_hash[32];
  sha256((const uint8_t *)rp_id, strlen(rp_id), rp_hash);

  uint8_t cose_key_buf[128];
  size_t cose_key_len = build_cose_key(pub_key, pub_key + 32, cose_key_buf);
  Serial.printf("[MC] cose_key_len=%u\n", cose_key_len);

  // authData = rpIdHash(32) + flags(1) + signCount(4) + attestedCredData
  size_t auth_data_len = 32 + 1 + 4 + 16 + 2 + 32 + cose_key_len;
  uint8_t auth_data[256];
  memcpy(auth_data, rp_hash, 32);
  auth_data[32] = 0x41; // AT=1, UP=1
  auth_data[33] = 0; auth_data[34] = 0; auth_data[35] = 0; auth_data[36] = 0; // signCount=0
  memcpy(auth_data + 37, AAGUID, 16);
  auth_data[53] = 0; auth_data[54] = 32; // credId length = 32 (big-endian)
  memcpy(auth_data + 55, cred_id, 32);
  memcpy(auth_data + 87, cose_key_buf, cose_key_len);
  auth_data_len = 87 + cose_key_len;

  // Build response CBOR: {0x01: authData, 0x02: fmt("none"), 0x03: attStmt({})}
  resp[0] = CTAP2_OK;
  CborEncoder enc, rmap;
  cbor_encoder_init(&enc, &resp[1], CTAPHID_MAX_MSG_SIZE - 1, 0);
  cbor_encoder_create_map(&enc, &rmap, 3);

  cbor_encode_uint(&rmap, 0x01);
  cbor_encode_byte_string(&rmap, auth_data, auth_data_len);

  cbor_encode_uint(&rmap, 0x02);
  cbor_encode_text_stringz(&rmap, "none");

  cbor_encode_uint(&rmap, 0x03);
  CborEncoder stmt_map;
  cbor_encoder_create_map(&rmap, &stmt_map, 0);
  cbor_encoder_close_container(&rmap, &stmt_map);

  cbor_encoder_close_container(&enc, &rmap);
  *resp_len = 1 + cbor_encoder_get_buffer_size(&enc, &resp[1]);
  Serial.printf("[MC] done resp_len=%u\n", *resp_len);
  return CTAP2_OK;
}

// ============================================================
// authenticatorGetAssertion
// ============================================================
uint8_t authenticator_get_assertion(const uint8_t *data, size_t data_len, uint8_t *resp, size_t *resp_len) {
  Serial.println("[GA] start");
  if (data_len < 2) { resp[0] = CTAP2_ERR_INVALID_PARAM; *resp_len = 1; return CTAP2_ERR_INVALID_PARAM; }

  CborParser parser;
  CborValue root, map;
  cbor_parser_init(&data[1], data_len - 1, 0, &parser, &root);
  if (!cbor_value_is_map(&root)) {
    resp[0] = CTAP2_ERR_INVALID_PARAM; *resp_len = 1; return CTAP2_ERR_INVALID_PARAM;
  }
  map = root;

  // Single-pass extraction of rp_id (key 0x01) and clientDataHash (key 0x02)
  char rp_id[256] = {0};
  uint8_t client_data_hash[64];
  size_t cdh_len = sizeof(client_data_hash);
  bool got_cdh = false;

  CborValue it;
  cbor_value_enter_container(&map, &it);
  while (!cbor_value_at_end(&it)) {
    uint64_t key;
    if (!cbor_value_is_unsigned_integer(&it)) { cbor_value_advance(&it); cbor_value_advance(&it); continue; }
    cbor_value_get_uint64(&it, &key);
    cbor_value_advance(&it);
    if (key == 0x01 && cbor_value_is_text_string(&it)) {
      size_t id_len = sizeof(rp_id) - 1;
      cbor_value_copy_text_string(&it, rp_id, &id_len, NULL);
      rp_id[id_len] = '\0';
    } else if (key == 0x02 && cbor_value_is_byte_string(&it)) {
      cbor_value_copy_byte_string(&it, client_data_hash, &cdh_len, NULL);
      got_cdh = true;
    }
    cbor_value_advance(&it);
  }

  if (rp_id[0] == '\0') {
    Serial.println("[GA] rp_id not found");
    resp[0] = CTAP2_ERR_INVALID_PARAM; *resp_len = 1; return CTAP2_ERR_INVALID_PARAM;
  }
  if (!got_cdh) {
    Serial.println("[GA] clientDataHash not found");
    resp[0] = CTAP2_ERR_INVALID_PARAM; *resp_len = 1; return CTAP2_ERR_INVALID_PARAM;
  }
  Serial.printf("[GA] rp_id=%s cdh_len=%u\n", rp_id, cdh_len);

  // Check credential exists
  if (!key_storage_credential_exists(rp_id)) {
    Serial.println("[GA] no credentials");
    resp[0] = CTAP2_ERR_NO_CREDENTIALS; *resp_len = 1; return CTAP2_ERR_NO_CREDENTIALS;
  }

  // Derive credential key and ID
  uint8_t priv_key[32], cred_id[32], pub_key[64];
  key_storage_derive_credential_key(rp_id, priv_key);
  key_storage_derive_credential_id(rp_id, cred_id);
  if (!ecdsa_key_from_private(priv_key, pub_key)) {
    Serial.println("[GA] ecdsa_key_from_private failed");
    resp[0] = CTAP2_ERR_INVALID_PARAM; *resp_len = 1; return CTAP2_ERR_INVALID_PARAM;
  }

  // Build authData (no AT flag for assertion)
  uint8_t rp_hash[32];
  sha256((const uint8_t *)rp_id, strlen(rp_id), rp_hash);

  uint8_t auth_data[37];
  memcpy(auth_data, rp_hash, 32);
  auth_data[32] = 0x01; // UP=1
  auth_data[33] = 0; auth_data[34] = 0; auth_data[35] = 0; auth_data[36] = 0; // signCount=0

  // Sign: authenticatorData || clientDataHash
  uint8_t sign_input[37 + 64];
  memcpy(sign_input, auth_data, 37);
  memcpy(sign_input + 37, client_data_hash, cdh_len);
  uint8_t sign_hash[32], sig[64];
  sha256(sign_input, 37 + cdh_len, sign_hash);
  if (!ecdsa_sign(priv_key, sign_hash, sig)) {
    Serial.println("[GA] ecdsa_sign failed");
    resp[0] = CTAP2_ERR_INVALID_PARAM; *resp_len = 1; return CTAP2_ERR_INVALID_PARAM;
  }

  // Build response: {0x01: credential, 0x02: authData, 0x03: signature}
  resp[0] = CTAP2_OK;
  CborEncoder enc, rmap, cred_map;
  cbor_encoder_init(&enc, &resp[1], CTAPHID_MAX_MSG_SIZE - 1, 0);
  cbor_encoder_create_map(&enc, &rmap, 3);

  // credential descriptor: {type:"public-key", id: cred_id}
  cbor_encode_uint(&rmap, 0x01);
  cbor_encoder_create_map(&rmap, &cred_map, 2);
  cbor_encode_text_stringz(&cred_map, "type");
  cbor_encode_text_stringz(&cred_map, "public-key");
  cbor_encode_text_stringz(&cred_map, "id");
  cbor_encode_byte_string(&cred_map, cred_id, 32);
  cbor_encoder_close_container(&rmap, &cred_map);

  cbor_encode_uint(&rmap, 0x02);
  cbor_encode_byte_string(&rmap, auth_data, 37);

  // CTAP2 uses raw r||s (64 bytes), not DER
  cbor_encode_uint(&rmap, 0x03);
  cbor_encode_byte_string(&rmap, sig, 64);

  cbor_encoder_close_container(&enc, &rmap);
  *resp_len = 1 + cbor_encoder_get_buffer_size(&enc, &resp[1]);
  Serial.printf("[GA] done resp_len=%u\n", *resp_len);
  return CTAP2_OK;
}

// ============================================================
// authenticatorSelection
// ============================================================
uint8_t authenticator_selection(uint8_t *resp, size_t *resp_len) {
  resp[0] = CTAP2_OK;
  *resp_len = 1;
  return CTAP2_OK;
}