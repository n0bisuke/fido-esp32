#ifndef KEY_STORAGE_H
#define KEY_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

void key_storage_init();
bool key_storage_get_master_secret(uint8_t out[32]);
void key_storage_derive_credential_key(const char *rp_id, uint8_t priv[32]);
void key_storage_derive_credential_id(const char *rp_id, uint8_t out[32]);
bool key_storage_store_credential(const char *rp_id, const uint8_t pub_key[64]);
bool key_storage_credential_exists(const char *rp_id);
bool key_storage_get_credential_pubkey(const char *rp_id, uint8_t pub_key[64]);

#endif // KEY_STORAGE_H