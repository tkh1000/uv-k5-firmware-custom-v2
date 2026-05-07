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

if "messenger.c" in src:
    print(f"Makefile already patched ({path}), skipping.")
    sys.exit(0)

# Print structure for debugging
lines = src.splitlines(keepends=True)
print(f"=== {path} (first 100 lines) ===")
for i, line in enumerate(lines[:100]):
    print(f"{i+1:3}: {line}", end="")
print("=== Lines with .c / SRCS / OBJ ===")
for i, line in enumerate(lines):
    if re.search(r'\.c\b|SRCS?|OBJS?', line):
        print(f"{i+1:3}: {line}", end="")

our_files = [
    "Helper/fsk.c",
    "Helper/crypto.c",
    "App/bk4819_fsk.c",
    "App/messenger.c",
]

# Strategy 1: SRCS/SRC variable assignment
src_line_idx = None
src_assign = re.compile(r'^(SRCS?|OBJS?|C_SRCS?)\s*[+:?]?=', re.IGNORECASE)
for i, line in enumerate(lines):
    if src_assign.match(line.strip()) or ('app/' in line and '.c' in line):
        src_line_idx = i

if src_line_idx is not None:
    insert = "\n# --- Messenger + Scrambler sources ---\n"
    for f in our_files:
        insert += f"SRCS += {f}\n"
    lines.insert(src_line_idx + 1, insert)
    with open(path, 'w') as f:
        f.writelines(lines)
    print(f"OK: Patched Makefile with SRCS += after line {src_line_idx + 1}")
    sys.exit(0)

# Strategy 2: .o file list in link rule
obj_line_idx = None
for i, line in enumerate(lines):
    if re.search(r'app/app\.o|app/menu\.o', line):
        obj_line_idx = i

if obj_line_idx is not None:
    obj_insert = "".join(f"  {f.replace('.c', '.o')} \\\n" for f in our_files)
    lines.insert(obj_line_idx + 1, obj_insert)
    with open(path, 'w') as f:
        f.writelines(lines)
    print(f"OK: Patched Makefile .o entries after line {obj_line_idx + 1}")
    sys.exit(0)

print("ERROR: could not auto-patch Makefile — see structure printed above", file=sys.stderr)
sys.exit(1)
