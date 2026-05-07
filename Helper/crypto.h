#ifndef HELPER_CRYPTO_H
#define HELPER_CRYPTO_H

#include <stdint.h>

#define CHACHA20_KEY_SIZE   32
#define CHACHA20_NONCE_SIZE 12

typedef struct {
    uint32_t state[16];
    uint8_t  keystream[64];
    uint8_t  keystream_pos;
} ChaCha20_Ctx;

void CRYPTO_ChaCha20Init(ChaCha20_Ctx *ctx,
                         const uint8_t key[CHACHA20_KEY_SIZE],
                         const uint8_t nonce[CHACHA20_NONCE_SIZE],
                         uint32_t      counter);

void CRYPTO_ChaCha20XOR(ChaCha20_Ctx *ctx, uint8_t *buf, uint16_t len);

// Convenience: encrypt/decrypt in-place with a fixed key derived from a
// shared passphrase stored in EEPROM settings.
void CRYPTO_EncryptMessage(uint8_t *buf, uint16_t len);
void CRYPTO_DecryptMessage(uint8_t *buf, uint16_t len);

#endif // HELPER_CRYPTO_H
