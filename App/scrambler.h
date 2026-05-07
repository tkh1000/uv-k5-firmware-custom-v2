#ifndef APP_SCRAMBLER_H
#define APP_SCRAMBLER_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// Frequency-inversion voice scrambler for UV-K5 V1/V2
// Target: DP32G030, 10-bit ADC  →  centre = 512
//
// Frequency inversion is self-inverse: the same function
// scrambles on TX and de-scrambles on RX.
// ============================================================

#define SCRAMBLER_ADC_BITS    10
#define SCRAMBLER_ADC_CENTER  512          // 2^(10-1)
#define SCRAMBLER_ADC_MAX     1023         // 2^10 - 1

static bool g_scrambler_enabled = false;

static inline void SCRAMBLER_Enable(void)  { g_scrambler_enabled = true;  }
static inline void SCRAMBLER_Disable(void) { g_scrambler_enabled = false; }
static inline bool SCRAMBLER_IsEnabled(void) { return g_scrambler_enabled; }

/**
 * SCRAMBLER_ProcessSample - frequency inversion around ADC midpoint.
 *
 * Maps sample s  →  (2 * CENTER) - s
 * which reflects the audio spectrum about the carrier.
 *
 * For 10-bit ADC: CENTER = 512, so result = 1024 - s
 * Clamped to [0, ADC_MAX] to avoid wrap-around artefacts.
 *
 * Call this on every audio sample in both the TX path (scramble)
 * and the RX path after squelch opens (de-scramble).
 */
static inline int16_t SCRAMBLER_ProcessSample(int16_t s)
{
    if (!g_scrambler_enabled)
        return s;

    int16_t inv = (int16_t)(2 * SCRAMBLER_ADC_CENTER) - s;

    // Clamp
    if (inv < 0)                     inv = 0;
    if (inv > SCRAMBLER_ADC_MAX)     inv = SCRAMBLER_ADC_MAX;

    return inv;
}

#endif // APP_SCRAMBLER_H
