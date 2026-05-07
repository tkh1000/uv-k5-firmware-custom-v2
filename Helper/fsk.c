#include "fsk.h"
#include <string.h>

// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection
uint16_t FSK_CRC16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

bool FSK_BuildPacket(FSK_Packet_t *pkt, const uint8_t *payload, uint8_t payload_len)
{
    if (!pkt || !payload || payload_len > FSK_PAYLOAD_MAX_LEN)
        return false;

    pkt->magic = FSK_PACKET_MAGIC;
    pkt->len   = payload_len;
    memset(pkt->payload, 0, FSK_PAYLOAD_MAX_LEN);
    memcpy(pkt->payload, payload, payload_len);
    pkt->crc = FSK_CRC16((const uint8_t *)pkt, sizeof(FSK_Packet_t) - sizeof(uint16_t));
    return true;
}

bool FSK_ValidatePacket(const FSK_Packet_t *pkt)
{
    if (!pkt)
        return false;
    if (pkt->magic != FSK_PACKET_MAGIC)
        return false;
    if (pkt->len > FSK_PAYLOAD_MAX_LEN)
        return false;
    uint16_t expected = FSK_CRC16((const uint8_t *)pkt, sizeof(FSK_Packet_t) - sizeof(uint16_t));
    return pkt->crc == expected;
}
