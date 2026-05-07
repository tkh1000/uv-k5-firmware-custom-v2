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

includes = '#include "App/messenger.h"\n#include "App/scrambler.h"\n'
content = re.sub(r'(#include\s+"[^"]+"\s*\n)(?!#include)', r'\1' + includes, content, count=1)

content = re.sub(r'(void\s+APP_Init\s*\(.*?\)\s*\{)',
                 r'\1\n    MSG_Init();', content, count=1, flags=re.DOTALL)

content = re.sub(r'(void\s+APP_TimeSlice10ms\s*\(.*?\)\s*\{)',
                 r'\1\n    if (MSG_IsActive()) MSG_TimeSlice10ms();', content, count=1, flags=re.DOTALL)

content = re.sub(r'(void\s+APP_ProcessKey\s*\(.*?\)\s*\{)',
                 r'\1\n    if (MSG_IsActive()) { MSG_HandleKey(Key, bKeyPressed, bKeyHeld); return; }',
                 content, count=1, flags=re.DOTALL)

content = re.sub(r'(RADIO_PrepareTX\s*\(\s*\)\s*;)',
                 r'SCRAMBLER_Enable();\n    \1', content, count=1)

content = re.sub(r'(RADIO_ReleaseTX\s*\(\s*\)\s*;|APP_EndTransmission\s*\(\s*\)\s*;)',
                 r'\1\n    SCRAMBLER_Disable();', content, count=1)

content = re.sub(r'(AUDIO_AudioBegin\s*\(\s*\)\s*;)',
                 r'\1\n    SCRAMBLER_Enable();', content, count=1)

content = re.sub(r'(AUDIO_AudioEnd\s*\(\s*\)\s*;)',
                 r'\1\n    SCRAMBLER_Disable();', content, count=1)

with open(path, 'w') as f:
    f.write(content)
print(f"Patched {path}")
