#!/usr/bin/env python3
import re, sys, os

fw = sys.argv[1] if len(sys.argv) > 1 else "firmware"

candidates = [
    f"{fw}/settings.h",
    f"{fw}/Settings/settings.h",
    f"{fw}/include/settings.h",
]

path = None
for c in candidates:
    if os.path.exists(c):
        with open(c) as f:
            content = f.read()
        path = c
        break

if path is None:
    print("ERROR: settings.h not found", file=sys.stderr)
    sys.exit(1)

if "MESSENGER_KEY" in content:
    print(f"settings.h already patched ({path}), skipping.")
    sys.exit(0)

patch_block = (
    "\n"
    "    // --- Messenger / Scrambler (added by CI patch) ---\n"
    "    uint8_t  MESSENGER_KEY[32];         // ChaCha20-256 shared key\n"
    "    char     MESSENGER_CALLSIGN[11];    // local callsign (null-terminated)\n"
    "    uint8_t  SCRAMBLER_ENABLED;         // 0=off, 1=on\n"
)

patched = re.sub(r'(\}\s*EEPROM_Config_t\s*;)', patch_block + r'\1', content, count=1)
if patched == content:
    patched = re.sub(r'(\n\}\s*;)', patch_block + r'\1', content, count=1)

if patched == content:
    print("ERROR: could not locate struct closing brace in settings.h", file=sys.stderr)
    sys.exit(1)

with open(path, 'w') as f:
    f.write(patched)
print(f"Patched {path}")
