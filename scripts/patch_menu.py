#!/usr/bin/env python3
import re, sys, os

fw = sys.argv[1] if len(sys.argv) > 1 else "firmware"

candidates = [
    f"{fw}/app/menu.c",
    f"{fw}/App/menu.c",
    f"{fw}/app/MENU.c",
]

path = None
src = None
for c in candidates:
    if os.path.exists(c):
        with open(c) as f:
            src = f.read()
        path = c
        break

if path is None:
    print("WARNING: menu.c not found — add MSG/SCRAMBL entries manually.")
    sys.exit(0)

if "MESSENGER" in src:
    print(f"menu.c already patched ({path}), skipping.")
    sys.exit(0)

lines = src.splitlines(keepends=True)

# Find last line matching a menu table row pattern
last_idx = None
for pat in [r'\{\s*"[A-Z0-9_ ]{1,8}"\s*,', r'\{\s*"[^"]{1,12}"\s*,']:
    compiled = re.compile(pat)
    for i, line in enumerate(lines):
        if compiled.search(line):
            last_idx = i

if last_idx is None:
    print("WARNING: could not locate menu table rows — first 60 lines:")
    for i, line in enumerate(lines[:60]):
        print(f"{i+1:3}: {line}", end="")
    print("Add MSG and SCRAMBL entries to menu.c manually.")
    sys.exit(0)

insert = '    { "MSG",     MENU_MESSENGER, 0, 0 },\n    { "SCRAMBL", MENU_SCRAMBLER, 0, 0 },\n'
lines.insert(last_idx + 1, insert)

with open(path, 'w') as f:
    f.writelines(lines)
print(f"Patched {path} after line {last_idx + 1}")
