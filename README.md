# UV-K5 V1/V2 — FSK Messenger + Scrambler Port
**Base firmware**: F4HWN Fusion (`armel/uv-k5-firmware-custom`)
**Target hardware**: Quansheng UV-K5 V1/V2 (DP32G030, ARM Cortex-M0+)
**Features**: FSK SMS Messenger (ChaCha20-256) + Frequency-inversion voice scrambler

---

## What this adds to F4HWN Fusion

### FSK SMS Messenger
- Text messaging over radio using the BK4819 FSK modem at 1200 baud
- ChaCha20-256 encryption with a shared key stored in EEPROM
- CRC-16/CCITT-FALSE packet framing with magic-number sync
- Multi-tap keyboard input (standard phone layout)
- 8-message history ring buffer with scrollable UI
- Callsign tagging per message

### Frequency-inversion Voice Scrambler
- Classic analog frequency-inversion scrambler
- 10-bit ADC centering (ADC_CENTER = 512) — correct for DP32G030
- Self-inverse: same function scrambles TX and de-scrambles RX
- Enabled automatically on PTT press; disabled on PTT release
- RX de-scrambling hooks onto squelch-open/close events

---

## File layout

```
.
├── Helper/
│   ├── fsk.h / fsk.c          CRC-16 packet framing
│   └── crypto.h / crypto.c    ChaCha20-256 stream cipher
├── App/
│   ├── bk4819_fsk.h / .c      BK4819 FSK TX/RX register sequences
│   ├── messenger.h / .c       Messenger UI, keyboard, history
│   └── scrambler.h            Frequency-inversion scrambler (inline)
├── .github/workflows/
│   └── main.yml               GitHub Actions CI (replaces original)
├── build.sh                   Local build helper
└── scripts/
    ├── patch_settings.py      Extends EEPROM_Config_t struct
    ├── patch_app.py           Wires init/timeslice/key/PTT/squelch hooks
    ├── patch_audio_adc.py     Injects SCRAMBLER_ProcessSample() in ADC path
    └── patch_menu.py          Adds MSG + SCRAMBLER menu entries
```

---

## Usage

### CI (GitHub Actions)
1. Fork `armel/uv-k5-firmware-custom`
2. Copy this entire directory into the fork root
3. Replace `.github/workflows/main.yml` with the one here
4. Push — the workflow clones, patches, compiles, and uploads the `.bin`

Tag a release (`git tag v1.0.0 && git push --tags`) to get a GitHub Release with the firmware attached.

### Local build
```bash
chmod +x build.sh
./build.sh
```
Requires Docker (to run `compile-with-docker.sh`).

---

## Key differences from V3 port

| | V1/V2 (this) | V3 |
|---|---|---|
| MCU | DP32G030 | PY32F071 |
| ADC bits | 10 (center = 512) | 12 (center = 2048) |
| Build system | Docker script | CMake presets |
| CI approach | Patch → docker | ARM GCC action + CMake |
| Scrambler math | Verbatim from kamilsss655 | Adapted |

---

## EEPROM changes

The struct patch adds 44 bytes to `EEPROM_Config_t`:
- `MESSENGER_KEY[32]` — ChaCha20 shared key (set via menu)
- `MESSENGER_CALLSIGN[11]` — local callsign
- `SCRAMBLER_ENABLED` — persistent on/off flag

**Back up your EEPROM calibration data before flashing.**

---

## Interoperability

The FSK packet format (magic, CRC, encryption scheme) is compatible with kamilsss655's messenger implementation on V1/V2 hardware, so V1/V2 radios running either firmware can exchange messages.
