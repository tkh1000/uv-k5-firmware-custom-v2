#include "crypto.h"
#include "../settings.h"   // for gEeprom.MESSENGER_KEY
#include <string.h>

// --------------------------------------------------------------------------
// Internal helpers
// --------------------------------------------------------------------------

#define ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void chacha20_quarter_round(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    *a += *b; *d ^= *a; *d = ROTL32(*d, 16);
    *c += *d; *b ^= *c; *b = ROTL32(*b, 12);
    *a += *b; *d ^= *a; *d = ROTL32(*d,  8);
    *c += *d; *b ^= *c; *b = ROTL32(*b,  7);
}

static void chacha20_block(uint32_t out[16], const uint32_t in[16])
{
    uint32_t x[16];
    memcpy(x, in, 64);

    for (int i = 0; i < 10; i++) {
        // Column rounds
        chacha20_quarter_round(&x[0], &x[4], &x[ 8], &x[12]);
        chacha20_quarter_round(&x[1], &x[5], &x[ 9], &x[13]);
        chacha20_quarter_round(&x[2], &x[6], &x[10], &x[14]);
        chacha20_quarter_round(&x[3], &x[7], &x[11], &x[15]);
        // Diagonal rounds
        chacha20_quarter_round(&x[0], &x[5], &x[10], &x[15]);
        chacha20_quarter_round(&x[1], &x[6], &x[11], &x[12]);
        chacha20_quarter_round(&x[2], &x[7], &x[ 8], &x[13]);
        chacha20_quarter_round(&x[3], &x[4], &x[ 9], &x[14]);
    }

    for (int i = 0; i < 16; i++)
        out[i] = x[i] + in[i];
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

void CRYPTO_ChaCha20Init(ChaCha20_Ctx *ctx,
                         const uint8_t key[CHACHA20_KEY_SIZE],
                         const uint8_t nonce[CHACHA20_NONCE_SIZE],
                         uint32_t      counter)
{
    // Constants "expa", "nd 3", "2-by", "te k"
    ctx->state[0]  = 0x61707865u;
    ctx->state[1]  = 0x3320646eu;
    ctx->state[2]  = 0x79622d32u;
    ctx->state[3]  = 0x6b206574u;

    for (int i = 0; i < 8; i++) {
        ctx->state[4 + i] = (uint32_t)key[i*4    ]        |
                            (uint32_t)key[i*4 + 1] <<  8  |
                            (uint32_t)key[i*4 + 2] << 16  |
                            (uint32_t)key[i*4 + 3] << 24;
    }

    ctx->state[12] = counter;

    for (int i = 0; i < 3; i++) {
        ctx->state[13 + i] = (uint32_t)nonce[i*4    ]        |
                             (uint32_t)nonce[i*4 + 1] <<  8  |
                             (uint32_t)nonce[i*4 + 2] << 16  |
                             (uint32_t)nonce[i*4 + 3] << 24;
    }

    ctx->keystream_pos = 64; // force block generation on first use
}

void CRYPTO_ChaCha20XOR(ChaCha20_Ctx *ctx, uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        if (ctx->keystream_pos == 64) {
            uint32_t block[16];
            chacha20_block(block, ctx->state);
            memcpy(ctx->keystream, block, 64);
            ctx->state[12]++;
            ctx->keystream_pos = 0;
        }
        buf[i] ^= ctx->keystream[ctx->keystream_pos++];
    }
}

// Shared key + fixed nonce (nonce carried per-packet in a real deployment;
// this is a simplified version matching kamilsss655's approach).
static const uint8_t FIXED_NONCE[CHACHA20_NONCE_SIZE] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01
};

void CRYPTO_EncryptMessage(uint8_t *buf, uint16_t len)
{
    ChaCha20_Ctx ctx;
    CRYPTO_ChaCha20Init(&ctx, gEeprom.MESSENGER_KEY, FIXED_NONCE, 1);
    CRYPTO_ChaCha20XOR(&ctx, buf, len);
}

void CRYPTO_DecryptMessage(uint8_t *buf, uint16_t len)
{
    // ChaCha20 is self-inverse
    CRYPTO_EncryptMessage(buf, len);
}
