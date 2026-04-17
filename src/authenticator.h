#ifndef AUTHENTICATOR_H
#define AUTHENTICATOR_H

#include <stdint.h>
#include <stddef.h>

// AAGUID: all zeros for self-attestation
static const uint8_t AAGUID[16] = {0};

// authenticatorGetInfo: returns CTAP2 status byte
// Writes status byte at buf[0], CBOR payload starting at buf[1]
// Sets out_len to total bytes written (1 + CBOR length)
uint8_t authenticator_get_info(uint8_t *buf, size_t *out_len);

#endif // AUTHENTICATOR_H