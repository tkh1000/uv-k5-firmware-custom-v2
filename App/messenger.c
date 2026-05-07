// ============================================================
// messenger.c — FSK SMS messenger for UV-K5 V1/V2
// Ported from kamilsss655's implementation; adapted for
// F4HWN Fusion firmware conventions.
// ============================================================

#include "messenger.h"
#include "bk4819_fsk.h"
#include "../Helper/fsk.h"
#include "../Helper/crypto.h"
#include "../driver/st7565.h"    // display driver
#include "../driver/keyboard.h"
#include "../settings.h"
#include "../misc.h"
#include <string.h>
#include <stdio.h>

// --------------------------------------------------------------------------
// Multi-tap key map (ITU-T E.161 / standard phone layout)
// --------------------------------------------------------------------------
typedef struct {
    const char *chars;
    uint8_t     count;
} KeyMap_t;

static const KeyMap_t KEY_MAP[10] = {
    { " 0",        2 },  // KEY_0
    { ".,!?1",     5 },  // KEY_1
    { "abc2",      4 },  // KEY_2
    { "def3",      4 },  // KEY_3
    { "ghi4",      4 },  // KEY_4
    { "jkl5",      4 },  // KEY_5
    { "mno6",      4 },  // KEY_6
    { "pqrs7",     5 },  // KEY_7
    { "tuv8",      4 },  // KEY_8
    { "wxyz9",     5 },  // KEY_9
};

#define MULTITAP_TIMEOUT_MS  800   // commit current char after this many ms

// --------------------------------------------------------------------------
// State
// --------------------------------------------------------------------------
static bool        s_active          = false;
static char        s_compose[MSG_MAX_LEN + 1];
static uint8_t     s_compose_len     = 0;

// Multi-tap state
static uint8_t     s_tap_key         = 0xFF;  // 0xFF = none
static uint8_t     s_tap_count       = 0;
static uint32_t    s_tap_timer_ms    = 0;

// History ring buffer
static MSG_Entry_t s_history[MSG_HISTORY_SIZE];
static uint8_t     s_history_head    = 0;  // next write position
static uint8_t     s_history_count   = 0;

// RX state machine
typedef enum { RX_IDLE, RX_RECEIVING } RX_State_t;
static RX_State_t  s_rx_state        = RX_IDLE;
static uint32_t    s_tick_ms         = 0;

// View state (scroll through history)
static uint8_t     s_view_offset     = 0;  // 0 = newest

// --------------------------------------------------------------------------
// Internal helpers
// --------------------------------------------------------------------------

static void history_push(const char *text, const char *from, bool sent)
{
    MSG_Entry_t *e = &s_history[s_history_head];
    strncpy(e->text, text, MSG_MAX_LEN);
    e->text[MSG_MAX_LEN] = '\0';
    strncpy(e->from, from, MSG_CALLSIGN_LEN);
    e->from[MSG_CALLSIGN_LEN] = '\0';
    e->sent  = sent;
    e->valid = true;

    s_history_head = (s_history_head + 1) % MSG_HISTORY_SIZE;
    if (s_history_count < MSG_HISTORY_SIZE)
        s_history_count++;
}

static MSG_Entry_t *history_get(uint8_t age)
{
    // age 0 = most recent
    if (age >= s_history_count)
        return NULL;
    uint8_t idx = (s_history_head + MSG_HISTORY_SIZE - 1 - age) % MSG_HISTORY_SIZE;
    return s_history[idx].valid ? &s_history[idx] : NULL;
}

static void multitap_commit(void)
{
    if (s_tap_key == 0xFF || s_compose_len >= MSG_MAX_LEN)
        return;
    const KeyMap_t *km = &KEY_MAP[s_tap_key];
    s_compose[s_compose_len++] = km->chars[s_tap_count % km->count];
    s_compose[s_compose_len]   = '\0';
    s_tap_key   = 0xFF;
    s_tap_count = 0;
}

static void send_compose_buffer(void)
{
    if (s_compose_len == 0)
        return;

    multitap_commit();  // flush any pending tap

    bool ok = BK4819_FSK_SendPacket((const uint8_t *)s_compose, s_compose_len);
    if (ok)
        history_push(s_compose, gEeprom.MESSENGER_CALLSIGN, true);

    // Clear compose
    memset(s_compose, 0, sizeof(s_compose));
    s_compose_len = 0;
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

void MSG_Init(void)
{
    memset(s_history, 0, sizeof(s_history));
    memset(s_compose, 0, sizeof(s_compose));
    s_history_head  = 0;
    s_history_count = 0;
    s_compose_len   = 0;
    s_tap_key       = 0xFF;
    s_rx_state      = RX_IDLE;
    s_active        = false;
}

void MSG_Open(void)
{
    s_active       = true;
    s_view_offset  = 0;
    BK4819_FSK_EnableRX();
}

void MSG_Close(void)
{
    s_active = false;
    BK4819_FSK_DisableRX();
    multitap_commit();  // don't lose a pending character
}

bool MSG_IsActive(void) { return s_active; }

void MSG_TimeSlice10ms(void)
{
    s_tick_ms += 10;

    // Multi-tap commit timer
    if (s_tap_key != 0xFF && (s_tick_ms - s_tap_timer_ms) >= MULTITAP_TIMEOUT_MS)
        multitap_commit();

    // Poll FSK FIFO for incoming packet
    if (s_active) {
        uint8_t rx_buf[FSK_PAYLOAD_MAX_LEN];
        uint8_t rx_len = 0;
        if (BK4819_FSK_ReceivePacket(rx_buf, &rx_len) && rx_len > 0) {
            rx_buf[rx_len < FSK_PAYLOAD_MAX_LEN ? rx_len : FSK_PAYLOAD_MAX_LEN - 1] = '\0';
            MSG_OnReceive(rx_buf, rx_len);
        }
    }
}

void MSG_OnReceive(const uint8_t *payload, uint8_t len)
{
    char text[MSG_MAX_LEN + 1];
    uint8_t copy_len = len < MSG_MAX_LEN ? len : MSG_MAX_LEN;
    memcpy(text, payload, copy_len);
    text[copy_len] = '\0';
    history_push(text, "RX", false);
    s_view_offset = 0;  // jump to newest
}

void MSG_HandleKey(uint8_t key, bool pressed, bool held)
{
    if (!pressed)
        return;

    // Number keys 0-9: multi-tap input
    if (key <= KEY_9) {
        if (s_tap_key == key) {
            // Same key: advance to next character in set
            s_tap_count++;
        } else {
            // Different key: commit previous, start new
            multitap_commit();
            s_tap_key   = key;
            s_tap_count = 0;
        }
        s_tap_timer_ms = s_tick_ms;
        return;
    }

    switch (key) {
    case KEY_MENU:   // Send
        multitap_commit();
        send_compose_buffer();
        break;

    case KEY_EXIT:   // Backspace / close
        if (s_compose_len > 0) {
            s_tap_key = 0xFF;
            if (s_compose_len > 0)
                s_compose[--s_compose_len] = '\0';
        } else {
            MSG_Close();
        }
        break;

    case KEY_UP:     // Scroll history up (older)
        if (s_view_offset + 1 < s_history_count)
            s_view_offset++;
        break;

    case KEY_DOWN:   // Scroll history down (newer)
        if (s_view_offset > 0)
            s_view_offset--;
        break;

    default:
        break;
    }
}

// --------------------------------------------------------------------------
// UI rendering
// --------------------------------------------------------------------------
// Layout (128×64 display):
//   Row 0 (y=0):  header bar "[ MESSENGER ]"
//   Rows 1-3:     history (up to 3 entries, newest at bottom)
//   Row 4 (y=48): separator line
//   Row 5 (y=56): compose line  "> _"
// --------------------------------------------------------------------------

void MSG_Render(void)
{
    // Clear framebuffer
    ST7565_Fill(0x00);

    // Header
    ST7565_DrawString(0, 0, "[ MESSENGER ]", false);

    // History: show 3 entries above the compose bar
    for (int row = 0; row < 3; row++) {
        uint8_t age = (uint8_t)(2 - row) + s_view_offset;
        MSG_Entry_t *e = history_get(age);
        if (!e)
            continue;

        char line[22];  // 128px / ~6px per char ≈ 21 chars
        uint8_t y = (uint8_t)(8 + row * 16);

        // Prefix: ">" for sent, "<" for received
        snprintf(line, sizeof(line), "%c %.18s", e->sent ? '>' : '<', e->text);
        ST7565_DrawString(0, y, line, false);
    }

    // Separator
    ST7565_DrawLineH(0, 127, 49);

    // Compose line with cursor
    char compose_display[22];
    char cursor = ((s_tick_ms / 500) & 1) ? '_' : ' ';

    // Show pending multi-tap character in brackets if any
    if (s_tap_key != 0xFF) {
        char pending = KEY_MAP[s_tap_key].chars[s_tap_count % KEY_MAP[s_tap_key].count];
        snprintf(compose_display, sizeof(compose_display),
                 ">%.17s[%c]%c", s_compose, pending, cursor);
    } else {
        snprintf(compose_display, sizeof(compose_display),
                 ">%.19s%c", s_compose, cursor);
    }
    ST7565_DrawString(0, 56, compose_display, false);

    ST7565_BlitFullScreen();
}
