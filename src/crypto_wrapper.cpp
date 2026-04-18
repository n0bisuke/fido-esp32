#include "crypto_wrapper.h"
#include <uECC.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include <esp_random.h>
#include <cstring>

static int uECC_rng(uint8_t *dest, unsigned size) {
  for (unsigned i = 0; i < size; i++) {
    dest[i] = (uint8_t)(esp_random() & 0xFF);
  }
  return 1;
}

void crypto_init() {
  uECC_set_rng(uECC_rng);
}

void sha256(const uint8_t *input, size_t len, uint8_t out[32]) {
  mbedtls_sha256_ret(input, len, out, 0);
}

// HKDF-Expand (RFC 5869 Section 2.3)
// T(1) = HMAC-Hash(PRK, info || 0x01)
// T(2) = HMAC-Hash(PRK, T(1) || info || 0x02)
static void hkdf_expand_sha256(const uint8_t *prk, size_t prk_len,
                               const uint8_t *info, size_t info_len,
                               uint8_t *okm, size_t okm_len) {
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  uint8_t T[32];
  uint8_t counter = 1;
  size_t offset = 0;

  while (offset < okm_len) {
    mbedtls_md_context_t ctx;
    mbedtls_md_setup(&ctx, md, 1);
    mbedtls_md_hmac_starts(&ctx, prk, prk_len);

    if (counter == 1) {
      // T(1) = HMAC(PRK, info || 0x01)
      mbedtls_md_hmac_update(&ctx, info, info_len);
      mbedtls_md_hmac_update(&ctx, &counter, 1);
    } else {
      // T(n) = HMAC(PRK, T(n-1) || info || n)
      mbedtls_md_hmac_update(&ctx, T, 32);
      mbedtls_md_hmac_update(&ctx, info, info_len);
      mbedtls_md_hmac_update(&ctx, &counter, 1);
    }
    mbedtls_md_hmac_finish(&ctx, T);
    mbedtls_md_free(&ctx);

    size_t copy_len = okm_len - offset;
    if (copy_len > 32) copy_len = 32;
    memcpy(okm + offset, T, copy_len);
    offset += copy_len;
    counter++;
  }
}

// HKDF-Extract (RFC 5869 Section 2.2)
// PRK = HMAC-Hash(salt, IKM)
static void hkdf_extract_sha256(const uint8_t *salt, size_t salt_len,
                                const uint8_t *ikm, size_t ikm_len,
                                uint8_t prk[32]) {
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (salt == NULL || salt_len == 0) {
    uint8_t zeros[32] = {0};
    mbedtls_md_hmac(md, zeros, 32, ikm, ikm_len, prk);
  } else {
    mbedtls_md_hmac(md, salt, salt_len, ikm, ikm_len, prk);
  }
}

void hkdf_sha256(const uint8_t *salt, size_t salt_len,
                 const uint8_t *ikm, size_t ikm_len,
                 const uint8_t *info, size_t info_len,
                 uint8_t *okm, size_t okm_len) {
  uint8_t prk[32];
  hkdf_extract_sha256(salt, salt_len, ikm, ikm_len, prk);
  hkdf_expand_sha256(prk, 32, info, info_len, okm, okm_len);
}

bool ecdsa_key_from_private(const uint8_t priv[32], uint8_t pub[64]) {
  return uECC_compute_public_key(priv, pub, uECC_secp256r1()) == 1;
}

bool ecdsa_sign(const uint8_t priv[32], const uint8_t hash[32], uint8_t sig[64]) {
  return uECC_sign(priv, hash, 32, sig, uECC_secp256r1()) == 1;
}