#ifndef APP_MESSENGER_H
#define APP_MESSENGER_H

#include <stdint.h>
#include <stdbool.h>

// --------------------------------------------------------------------------
// Messenger configuration
// --------------------------------------------------------------------------
#define MSG_MAX_LEN        80      // max text message length (chars)
#define MSG_HISTORY_SIZE    8      // ring buffer depth
#define MSG_CALLSIGN_LEN   10      // max callsign length

// --------------------------------------------------------------------------
// Message record stored in history ring buffer
// --------------------------------------------------------------------------
typedef struct {
    char     text[MSG_MAX_LEN + 1];
    char     from[MSG_CALLSIGN_LEN + 1];
    bool     sent;          // true = TX'd by us, false = received
    bool     valid;
} MSG_Entry_t;

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

// Called from app.c init hook
void     MSG_Init(void);

// Called from app.c 10 ms timeslice
void     MSG_TimeSlice10ms(void);

// Called from app.c key handler
void     MSG_HandleKey(uint8_t key, bool pressed, bool held);

// Called from app.c when a complete FSK packet arrives over the air
void     MSG_OnReceive(const uint8_t *payload, uint8_t len);

// Draw the messenger UI onto the display
void     MSG_Render(void);

// True when the messenger UI is the active foreground app
bool     MSG_IsActive(void);

// Open/close the messenger screen
void     MSG_Open(void);
void     MSG_Close(void);

#endif // APP_MESSENGER_H
