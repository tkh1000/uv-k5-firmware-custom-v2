#ifndef APP_BK4819_FSK_H
#define APP_BK4819_FSK_H

#include <stdint.h>
#include <stdbool.h>

void BK4819_FSK_EnableTX(void);
void BK4819_FSK_DisableTX(void);
bool BK4819_FSK_SendPacket(const uint8_t *payload, uint8_t len);

void BK4819_FSK_EnableRX(void);
void BK4819_FSK_DisableRX(void);
bool BK4819_FSK_ReceivePacket(uint8_t *payload_out, uint8_t *len_out);

#endif // APP_BK4819_FSK_H
