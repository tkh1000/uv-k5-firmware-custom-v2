#!/usr/bin/env python3
import re, sys, os

fw = sys.argv[1] if len(sys.argv) > 1 else "firmware"

candidates = [f"{fw}/Makefile", f"{fw}/makefile"]

path = None
src = None
for c in candidates:
    if os.path.exists(c):
        with open(c) as f:
            src = f.read()
        path = c
        break

if path is None:
    print("ERROR: Makefile not found", file=sys.stderr)
    sys.exit(1)

if "messenger.o" in src:
    print(f"Makefile already patched ({path}), skipping.")
    sys.exit(0)

our_objs = [
    "Helper/fsk.o",
    "Helper/crypto.o",
    "App/bk4819_fsk.o",
    "App/messenger.o",
]

lines = src.splitlines(keepends=True)

# Find the last OBJS += line and insert our entries after it
last_objs_idx = None
for i, line in enumerate(lines):
    if re.match(r'\s*OBJS\s*\+=', line):
        last_objs_idx = i

if last_objs_idx is None:
    print("ERROR: could not find any 'OBJS +=' line in Makefile", file=sys.stderr)
    for i, line in enumerate(lines[:120]):
        print(f"{i+1:3}: {line}", end="")
    sys.exit(1)

obj_insert = "\n# --- Messenger + Scrambler ---\n"
for obj in our_objs:
    obj_insert += f"OBJS += {obj}\n"

lines.insert(last_objs_idx + 1, obj_insert)

# Also disable ENABLE_TX1750 (1750 Hz tone burst) to reclaim flash space.
# Uses = (not ?=) so it overrides the default. Re-enable in Makefile if needed.
# Saves ~200 bytes — more than enough to cover our 44-byte overflow.
space_insert = "\n# Free flash for Messenger+Scrambler (re-enable if needed)\nENABLE_TX1750 = 0\n"
# Insert near the top, after the first ?= block ends (before the ##### line)
sep_idx = None
for i, line in enumerate(lines):
    if line.startswith('######'):
        sep_idx = i
        break

if sep_idx is not None:
    lines.insert(sep_idx, space_insert)
else:
    # Fallback: insert at top
    lines.insert(0, space_insert)

with open(path, 'w') as f:
    f.writelines(lines)

print(f"OK: OBJS += entries added after line {last_objs_idx + 1}, ENABLE_TX1750 disabled to reclaim flash.")
