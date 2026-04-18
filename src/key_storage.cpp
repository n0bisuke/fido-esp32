#include "key_storage.h"
#include "crypto_wrapper.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_random.h>
#include <string.h>

static const char *NVS_NS = "fido2";
static const char *NVS_MASTER_KEY = "master_sec";

static uint8_t master_secret[32];
static bool master_secret_loaded = false;

static void ensure_master_secret() {
  if (master_secret_loaded) return;

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &handle);
  if (err != ESP_OK) return;

  size_t len = 32;
  err = nvs_get_blob(handle, NVS_MASTER_KEY, master_secret, &len);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    for (int i = 0; i < 32; i++) {
      master_secret[i] = (uint8_t)(esp_random() & 0xFF);
    }
    nvs_set_blob(handle, NVS_MASTER_KEY, master_secret, 32);
    nvs_commit(handle);
  }
  nvs_close(handle);
  master_secret_loaded = true;
}

void key_storage_init() {
  nvs_flash_init();
  ensure_master_secret();
}

bool key_storage_get_master_secret(uint8_t out[32]) {
  ensure_master_secret();
  memcpy(out, master_secret, 32);
  return true;
}

void key_storage_derive_credential_key(const char *rp_id, uint8_t priv[32]) {
  ensure_master_secret();
  size_t rp_len = strlen(rp_id);
  size_t info_len = 9 + rp_len; // "cred_key" + 0x00 + rp_id
  uint8_t info[9 + 256];
  memcpy(info, "cred_key", 8);
  info[8] = 0x00; // CBOR-style separator
  memcpy(info + 9, rp_id, rp_len);

  hkdf_sha256(NULL, 0, master_secret, 32, info, info_len, priv, 32);
}

void key_storage_derive_credential_id(const char *rp_id, uint8_t out[32]) {
  ensure_master_secret();
  size_t rp_len = strlen(rp_id);
  size_t info_len = 8 + rp_len; // "cred_id" + 0x00 + rp_id
  uint8_t info[8 + 256];
  memcpy(info, "cred_id", 7);
  info[7] = 0x00;
  memcpy(info + 8, rp_id, rp_len);

  hkdf_sha256(NULL, 0, master_secret, 32, info, info_len, out, 32);
}

// Store rp_id hash + public key in NVS
// Key format: "c_<first 8 hex chars of rp_id_hash>"
// Value: rp_id_hash(32B) || pub_key(64B)
bool key_storage_store_credential(const char *rp_id, const uint8_t pub_key[64]) {
  uint8_t rp_hash[32];
  sha256((const uint8_t *)rp_id, strlen(rp_id), rp_hash);

  char nvs_key[16];
  for (int i = 0; i < 4; i++) {
    snprintf(nvs_key + i * 2, 3, "%02x", rp_hash[i]);
  }

  uint8_t blob[96]; // 32 + 64
  memcpy(blob, rp_hash, 32);
  memcpy(blob + 32, pub_key, 64);

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &handle);
  if (err != ESP_OK) return false;

  nvs_set_blob(handle, nvs_key, blob, 96);
  nvs_commit(handle);
  nvs_close(handle);
  return true;
}

bool key_storage_credential_exists(const char *rp_id) {
  uint8_t rp_hash[32];
  sha256((const uint8_t *)rp_id, strlen(rp_id), rp_hash);

  char nvs_key[16];
  for (int i = 0; i < 4; i++) {
    snprintf(nvs_key + i * 2, 3, "%02x", rp_hash[i]);
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &handle);
  if (err != ESP_OK) return false;

  size_t len = 0;
  err = nvs_get_blob(handle, nvs_key, NULL, &len);
  nvs_close(handle);
  return err == ESP_OK && len == 96;
}

bool key_storage_get_credential_pubkey(const char *rp_id, uint8_t pub_key[64]) {
  uint8_t rp_hash[32];
  sha256((const uint8_t *)rp_id, strlen(rp_id), rp_hash);

  char nvs_key[16];
  for (int i = 0; i < 4; i++) {
    snprintf(nvs_key + i * 2, 3, "%02x", rp_hash[i]);
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &handle);
  if (err != ESP_OK) return false;

  uint8_t blob[96];
  size_t len = 96;
  err = nvs_get_blob(handle, nvs_key, blob, &len);
  nvs_close(handle);

  if (err != ESP_OK || len != 96) return false;
  memcpy(pub_key, blob + 32, 64);
  return true;
}