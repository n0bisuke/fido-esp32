#ifndef CRYPTO_WRAPPER_H
#define CRYPTO_WRAPPER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void crypto_init();
void sha256(const uint8_t *input, size_t len, uint8_t out[32]);
void hkdf_sha256(const uint8_t *salt, size_t salt_len,
                 const uint8_t *ikm, size_t ikm_len,
                 const uint8_t *info, size_t info_len,
                 uint8_t *okm, size_t okm_len);
bool ecdsa_key_from_private(const uint8_t priv[32], uint8_t pub[64]);
bool ecdsa_sign(const uint8_t priv[32], const uint8_t hash[32], uint8_t sig[64]);

#endif // CRYPTO_WRAPPER_H