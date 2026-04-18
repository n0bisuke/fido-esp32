#ifndef AUTHENTICATOR_H
#define AUTHENTICATOR_H

#include <stdint.h>
#include <stddef.h>

// AAGUID: all zeros for self-attestation
static const uint8_t AAGUID[16] = {0};

uint8_t authenticator_get_info(uint8_t *buf, size_t *out_len);
uint8_t authenticator_make_credential(const uint8_t *data, size_t data_len, uint8_t *resp, size_t *resp_len);
uint8_t authenticator_get_assertion(const uint8_t *data, size_t data_len, uint8_t *resp, size_t *resp_len);
uint8_t authenticator_selection(uint8_t *resp, size_t *resp_len);

#endif // AUTHENTICATOR_H