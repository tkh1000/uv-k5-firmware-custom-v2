#!/usr/bin/env python3
import re, sys, os

fw = sys.argv[1] if len(sys.argv) > 1 else "firmware"

# Check both app/menu.c AND ui/menu.c — the table is often in ui/menu.c
candidates = [
    f"{fw}/ui/menu.c",
    f"{fw}/app/menu.c",
    f"{fw}/App/menu.c",
    f"{fw}/ui/MENU.c",
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
    print("WARNING: no menu.c found in app/ or ui/ — skipping menu patch.")
    sys.exit(0)

if "MESSENGER" in src:
    print(f"menu.c already patched ({path}), skipping.")
    sys.exit(0)

lines = src.splitlines(keepends=True)

# Find last line matching a menu table row (various formats)
last_idx = None
patterns = [
    re.compile(r'\{\s*"[A-Z0-9_ ]{1,10}"\s*,'),   # { "SQL", ...
    re.compile(r'"\w{1,10}"\s*,\s*\w+\s*,'),        # "SQL", MENU_SQL,
    re.compile(r'MENU_\w+'),                          # any MENU_ constant
]
for pat in patterns:
    for i, line in enumerate(lines):
        if pat.search(line):
            last_idx = i

if last_idx is None:
    print(f"WARNING: could not locate menu table in {path}")
    print("First 80 lines:")
    for i, line in enumerate(lines[:80]):
        print(f"{i+1:3}: {line}", end="")
    print("Skipping — add MSG/SCRAMBL entries manually.")
    sys.exit(0)

insert = '    { "MSG",     MENU_MESSENGER, 0, 0 },\n    { "SCRAMBL", MENU_SCRAMBLER, 0, 0 },\n'
lines.insert(last_idx + 1, insert)

with open(path, 'w') as f:
    f.writelines(lines)
print(f"Patched {path} after line {last_idx + 1}")
