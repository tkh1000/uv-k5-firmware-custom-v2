#include "bk4819_fsk.h"
#include "../driver/bk4819.h"
#include "../Helper/fsk.h"
#include "../Helper/crypto.h"
#include <string.h>

// --------------------------------------------------------------------------
// FSK modem configuration (ported from kamilsss655, BK4819 identical chip)
// --------------------------------------------------------------------------

#define FSK_BAUD_RATE_REG   0x5C   // 1200 baud
#define FSK_MODE_REG        0x58
#define FSK_FIFO_REG        0x5F
#define FSK_STATUS_REG      0x0C

#define FSK_TX_TIMEOUT_MS   3000
#define FSK_RX_TIMEOUT_MS   500

// --------------------------------------------------------------------------
// TX
// --------------------------------------------------------------------------

void BK4819_FSK_EnableTX(void)
{
    BK4819_WriteRegister(BK4819_REG_58, 0x0070);  // FSK TX mode
    BK4819_WriteRegister(BK4819_REG_5C, 0x5555);  // 1200 baud preamble pattern
    BK4819_WriteRegister(BK4819_REG_59, 0x8068);  // enable FSK TX
}

void BK4819_FSK_DisableTX(void)
{
    BK4819_WriteRegister(BK4819_REG_59, 0x0068);
    BK4819_WriteRegister(BK4819_REG_58, 0x0000);
}

bool BK4819_FSK_SendPacket(const uint8_t *payload, uint8_t len)
{
    if (!payload || len == 0 || len > FSK_PAYLOAD_MAX_LEN)
        return false;

    // Build and encrypt packet
    FSK_Packet_t pkt;
    if (!FSK_BuildPacket(&pkt, payload, len))
        return false;
    CRYPTO_EncryptMessage(pkt.payload, FSK_PAYLOAD_MAX_LEN);

    BK4819_FSK_EnableTX();

    // Write packet bytes into FIFO
    const uint8_t *raw = (const uint8_t *)&pkt;
    uint16_t total = sizeof(FSK_Packet_t);
    for (uint16_t i = 0; i < total; i++) {
        // Wait for FIFO not full (bit 15 of status reg)
        uint32_t timeout = FSK_TX_TIMEOUT_MS * 100;
        while ((BK4819_ReadRegister(BK4819_REG_0C) & 0x8000) && timeout--)
            ;
        if (!timeout) {
            BK4819_FSK_DisableTX();
            return false;
        }
        BK4819_WriteRegister(BK4819_REG_5F, raw[i]);
    }

    // Wait for TX complete (FIFO empty)
    uint32_t timeout = FSK_TX_TIMEOUT_MS * 100;
    while ((BK4819_ReadRegister(BK4819_REG_0C) & 0x0020) && timeout--)
        ;

    BK4819_FSK_DisableTX();
    return true;
}

// --------------------------------------------------------------------------
// RX
// --------------------------------------------------------------------------

void BK4819_FSK_EnableRX(void)
{
    BK4819_WriteRegister(BK4819_REG_58, 0x0068);  // FSK RX mode
    BK4819_WriteRegister(BK4819_REG_5C, 0x5555);
    BK4819_WriteRegister(BK4819_REG_59, 0x0068);
}

void BK4819_FSK_DisableRX(void)
{
    BK4819_WriteRegister(BK4819_REG_58, 0x0000);
}

bool BK4819_FSK_ReceivePacket(uint8_t *payload_out, uint8_t *len_out)
{
    if (!payload_out || !len_out)
        return false;

    FSK_Packet_t pkt;
    uint8_t *raw = (uint8_t *)&pkt;
    uint16_t total = sizeof(FSK_Packet_t);

    for (uint16_t i = 0; i < total; i++) {
        uint32_t timeout = FSK_RX_TIMEOUT_MS * 100;
        // Wait for FIFO data available (bit 14 of status reg)
        while (!(BK4819_ReadRegister(BK4819_REG_0C) & 0x4000) && timeout--)
            ;
        if (!timeout)
            return false;
        raw[i] = (uint8_t)(BK4819_ReadRegister(BK4819_REG_5F) & 0xFF);
    }

    // Decrypt then validate
    CRYPTO_DecryptMessage(pkt.payload, FSK_PAYLOAD_MAX_LEN);
    if (!FSK_ValidatePacket(&pkt))
        return false;

    *len_out = pkt.len;
    memcpy(payload_out, pkt.payload, pkt.len);
    return true;
}
