#ifndef HELPER_FSK_H
#define HELPER_FSK_H

#include <stdint.h>
#include <stdbool.h>

// FSK packet constants
#define FSK_PACKET_MAGIC    0xABCD
#define FSK_PACKET_MAX_LEN  128
#define FSK_PAYLOAD_MAX_LEN 112

// Packet structure (packed to match wire format)
typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  len;
    uint8_t  payload[FSK_PAYLOAD_MAX_LEN];
    uint16_t crc;
} FSK_Packet_t;

uint16_t FSK_CRC16(const uint8_t *data, uint16_t len);
bool     FSK_BuildPacket(FSK_Packet_t *pkt, const uint8_t *payload, uint8_t payload_len);
bool     FSK_ValidatePacket(const FSK_Packet_t *pkt);

#endif // HELPER_FSK_H
