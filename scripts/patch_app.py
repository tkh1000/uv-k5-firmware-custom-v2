#!/usr/bin/env python3
import re, sys, os

fw = sys.argv[1] if len(sys.argv) > 1 else "firmware"

candidates = [
    f"{fw}/app/app.c",
    f"{fw}/App/app.c",
    f"{fw}/src/app.c",
]

path = None
content = None
for c in candidates:
    if os.path.exists(c):
        with open(c) as f:
            content = f.read()
        path = c
        break

if path is None:
    print("ERROR: app.c not found", file=sys.stderr)
    sys.exit(1)

if "MSG_Init" in content:
    print(f"app.c already patched ({path}), skipping.")
    sys.exit(0)

# ---- Includes ----
includes = '#include "App/messenger.h"\n#include "App/scrambler.h"\n'
content = re.sub(r'(#include\s+"[^"]+"\s*\n)(?!#include)', r'\1' + includes, content, count=1)

# ---- Init hook ----
content = re.sub(r'(void\s+APP_Init\s*\(.*?\)\s*\{)',
                 r'\1\n    MSG_Init();', content, count=1, flags=re.DOTALL)

# ---- 10ms timeslice ----
content = re.sub(r'(void\s+APP_TimeSlice10ms\s*\(.*?\)\s*\{)',
                 r'\1\n    if (MSG_IsActive()) MSG_TimeSlice10ms();', content, count=1, flags=re.DOTALL)

# ---- Key handler: intercept all keys when messenger is active ----
# Also add long-press bindings to open messenger / toggle scrambler
key_hook = (
    '\n'
    '    /* Messenger: intercept all keys when active */\n'
    '    if (MSG_IsActive()) { MSG_HandleKey(Key, bKeyPressed, bKeyHeld); return; }\n'
    '    /* Long-press SIDE1 = open messenger */\n'
    '    if (Key == KEY_SIDE1 && bKeyHeld && bKeyPressed) { MSG_Open(); return; }\n'
    '    /* Long-press SIDE2 = toggle scrambler */\n'
    '    if (Key == KEY_SIDE2 && bKeyHeld && bKeyPressed) {\n'
    '        if (SCRAMBLER_IsEnabled()) SCRAMBLER_Disable();\n'
    '        else SCRAMBLER_Enable();\n'
    '        gEeprom.SCRAMBLER_ENABLED = SCRAMBLER_IsEnabled() ? 1 : 0;\n'
    '        return;\n'
    '    }\n'
)
content = re.sub(r'(void\s+APP_ProcessKey\s*\(.*?\)\s*\{)',
                 r'\1' + key_hook, content, count=1, flags=re.DOTALL)

# ---- PTT press → scrambler TX enable ----
content = re.sub(r'(RADIO_PrepareTX\s*\(\s*\)\s*;)',
                 r'if (gEeprom.SCRAMBLER_ENABLED) SCRAMBLER_Enable();\n    \1', content, count=1)

# ---- PTT release → scrambler TX disable ----
content = re.sub(r'(RADIO_ReleaseTX\s*\(\s*\)\s*;|APP_EndTransmission\s*\(\s*\)\s*;)',
                 r'\1\n    SCRAMBLER_Disable();', content, count=1)

# ---- Squelch open → scrambler RX enable ----
content = re.sub(r'(AUDIO_AudioBegin\s*\(\s*\)\s*;)',
                 r'\1\n    if (gEeprom.SCRAMBLER_ENABLED) SCRAMBLER_Enable();', content, count=1)

# ---- Squelch close → scrambler RX disable ----
content = re.sub(r'(AUDIO_AudioEnd\s*\(\s*\)\s*;)',
                 r'\1\n    SCRAMBLER_Disable();', content, count=1)

with open(path, 'w') as f:
    f.write(content)
print(f"Patched {path}")
print("  KEY_SIDE1 long-press → open messenger")
print("  KEY_SIDE2 long-press → toggle scrambler")
