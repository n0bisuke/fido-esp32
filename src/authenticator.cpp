#include "authenticator.h"
#include "ctap2.h"
#include <cbor.h>

uint8_t authenticator_get_info(uint8_t *buf, size_t *out_len) {
  buf[0] = CTAP2_OK;

  CborEncoder enc, map_enc, arr_enc, opt_enc;
  cbor_encoder_init(&enc, &buf[1], CTAPHID_MAX_MSG_SIZE - 1, 0);
  cbor_encoder_create_map(&enc, &map_enc, 5);

  // 0x01: versions
  cbor_encode_uint(&map_enc, 0x01);
  cbor_encoder_create_array(&map_enc, &arr_enc, 2);
  cbor_encode_text_stringz(&arr_enc, "FIDO_2_0");
  cbor_encode_text_stringz(&arr_enc, "U2F_V2");
  cbor_encoder_close_container(&map_enc, &arr_enc);

  // 0x02: extensions (none)
  cbor_encode_uint(&map_enc, 0x02);
  cbor_encoder_create_array(&map_enc, &arr_enc, 0);
  cbor_encoder_close_container(&map_enc, &arr_enc);

  // 0x03: aaguid
  cbor_encode_uint(&map_enc, 0x03);
  cbor_encode_byte_string(&map_enc, AAGUID, 16);

  // 0x04: options
  cbor_encode_uint(&map_enc, 0x04);
  cbor_encoder_create_map(&map_enc, &opt_enc, 5);
  cbor_encode_text_stringz(&opt_enc, "rk");        cbor_encode_boolean(&opt_enc, false);
  cbor_encode_text_stringz(&opt_enc, "up");        cbor_encode_boolean(&opt_enc, true);
  cbor_encode_text_stringz(&opt_enc, "uv");        cbor_encode_boolean(&opt_enc, true);
  cbor_encode_text_stringz(&opt_enc, "plat");      cbor_encode_boolean(&opt_enc, false);
  cbor_encode_text_stringz(&opt_enc, "clientPin"); cbor_encode_boolean(&opt_enc, false);
  cbor_encoder_close_container(&map_enc, &opt_enc);

  // 0x05: maxMsgSize
  cbor_encode_uint(&map_enc, 0x05);
  cbor_encode_uint(&map_enc, CTAPHID_MAX_MSG_SIZE);

  cbor_encoder_close_container(&enc, &map_enc);

  *out_len = 1 + cbor_encoder_get_buffer_size(&enc, &buf[1]);
  return CTAP2_OK;
}