// ============================================================
// messenger.c — FSK SMS messenger for UV-K5 V1/V2
// Ported from kamilsss655; adapted for F4HWN Fusion V1/V2.
// ============================================================

#include "messenger.h"
#include "bk4819_fsk.h"
#include "../Helper/fsk.h"
#include "../Helper/crypto.h"
#include "../driver/st7565.h"
#include "../driver/keyboard.h"
#include "../settings.h"
#include "../misc.h"
#include "../ui/helper.h"       // UI_PrintStringSmall, UI_PrintString
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

#define MULTITAP_TIMEOUT_MS  800

// --------------------------------------------------------------------------
// State
// --------------------------------------------------------------------------
static bool        s_active       = false;
static char        s_compose[MSG_MAX_LEN + 1];
static uint8_t     s_compose_len  = 0;

static uint8_t     s_tap_key      = 0xFF;
static uint8_t     s_tap_count    = 0;
static uint32_t    s_tap_timer_ms = 0;

static MSG_Entry_t s_history[MSG_HISTORY_SIZE];
static uint8_t     s_history_head  = 0;
static uint8_t     s_history_count = 0;

static uint32_t    s_tick_ms      = 0;
static uint8_t     s_view_offset  = 0;

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
    multitap_commit();
    bool ok = BK4819_FSK_SendPacket((const uint8_t *)s_compose, s_compose_len);
    if (ok)
        history_push(s_compose, gEeprom.MESSENGER_CALLSIGN, true);
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
    s_active        = false;
}

void MSG_Open(void)
{
    s_active      = true;
    s_view_offset = 0;
    BK4819_FSK_EnableRX();
}

void MSG_Close(void)
{
    s_active = false;
    BK4819_FSK_DisableRX();
    multitap_commit();
}

bool MSG_IsActive(void) { return s_active; }

void MSG_TimeSlice10ms(void)
{
    s_tick_ms += 10;

    if (s_tap_key != 0xFF && (s_tick_ms - s_tap_timer_ms) >= MULTITAP_TIMEOUT_MS)
        multitap_commit();

    uint8_t rx_buf[FSK_PAYLOAD_MAX_LEN];
    uint8_t rx_len = 0;
    if (BK4819_FSK_ReceivePacket(rx_buf, &rx_len) && rx_len > 0) {
        uint8_t copy_len = rx_len < FSK_PAYLOAD_MAX_LEN ? rx_len : FSK_PAYLOAD_MAX_LEN - 1;
        rx_buf[copy_len] = '\0';
        MSG_OnReceive(rx_buf, rx_len);
    }
}

void MSG_OnReceive(const uint8_t *payload, uint8_t len)
{
    char text[MSG_MAX_LEN + 1];
    uint8_t copy_len = len < MSG_MAX_LEN ? len : MSG_MAX_LEN;
    memcpy(text, payload, copy_len);
    text[copy_len] = '\0';
    history_push(text, "RX", false);
    s_view_offset = 0;
}

void MSG_HandleKey(uint8_t key, bool pressed, bool held)
{
    (void)held;  // suppress unused-parameter warning

    if (!pressed)
        return;

    if (key <= KEY_9) {
        if (s_tap_key == key) {
            s_tap_count++;
        } else {
            multitap_commit();
            s_tap_key   = key;
            s_tap_count = 0;
        }
        s_tap_timer_ms = s_tick_ms;
        return;
    }

    switch (key) {
    case KEY_MENU:
        multitap_commit();
        send_compose_buffer();
        break;
    case KEY_EXIT:
        if (s_compose_len > 0) {
            s_tap_key = 0xFF;
            s_compose[--s_compose_len] = '\0';
        } else {
            MSG_Close();
        }
        break;
    case KEY_UP:
        if (s_view_offset + 1 < s_history_count)
            s_view_offset++;
        break;
    case KEY_DOWN:
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
// Uses gFrameBuffer[8][128] directly — 8 rows of 8px each.
// Row 0: header
// Rows 1-3: history (3 entries, 16px each = 2 rows per entry)
// Row 6: separator
// Row 7: compose line
// --------------------------------------------------------------------------

// Draw a horizontal separator line directly into the frame buffer
static void draw_hline(uint8_t page)
{
    if (page >= 8)
        return;
    memset(gFrameBuffer[page], 0xFF, 128);
}

void MSG_Render(void)
{
    // Clear frame buffer
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

    // Header — row 0
    UI_PrintStringSmall("[ MESSENGER ]", 0, 127, 0);

    // History — up to 3 entries, each on one text row (rows 1-3)
    for (int slot = 0; slot < 3; slot++) {
        uint8_t age = (uint8_t)(2 - slot) + s_view_offset;
        MSG_Entry_t *e = history_get(age);
        if (!e)
            continue;

        char line[22];
        snprintf(line, sizeof(line), "%c%.19s", e->sent ? '>' : '<', e->text);
        UI_PrintStringSmall(line, 0, 127, (uint8_t)(slot + 1));
    }

    // Separator — row 5
    draw_hline(5);

    // Compose line — row 6
    char compose_display[22];
    char cursor = ((s_tick_ms / 500) & 1) ? '_' : ' ';

    if (s_tap_key != 0xFF) {
        char pending = KEY_MAP[s_tap_key].chars[s_tap_count % KEY_MAP[s_tap_key].count];
        snprintf(compose_display, sizeof(compose_display),
                 ">%.16s[%c]%c", s_compose, pending, cursor);
    } else {
        snprintf(compose_display, sizeof(compose_display),
                 ">%.19s%c", s_compose, cursor);
    }
    UI_PrintStringSmall(compose_display, 0, 127, 6);

    ST7565_BlitFullScreen();
}
