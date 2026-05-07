#!/usr/bin/env python3
import re, sys, os

fw = sys.argv[1] if len(sys.argv) > 1 else "firmware"

h_path = f"{fw}/app/action.h"
c_path = f"{fw}/app/action.c"

# Always print both files for debugging
for fpath in [h_path, c_path]:
    if os.path.exists(fpath):
        with open(fpath) as f:
            lines = f.readlines()
        print(f"\n=== {fpath} ({len(lines)} lines) ===")
        for i, line in enumerate(lines):
            print(f"{i+1:4}: {line}", end="")
    else:
        print(f"NOT FOUND: {fpath}")

# ---- Patch action.h ----
if not os.path.exists(h_path):
    print("ERROR: action.h not found", file=sys.stderr)
    sys.exit(1)

with open(h_path) as f:
    h_src = f.read()

if "ACTION_MESSENGER" in h_src:
    print("\naction.h already patched.")
else:
    # Strategy: find the last identifier-like line inside an enum block
    # (a line that is just an identifier, optionally with = value and comma)
    # and insert our values after it, before the closing brace.
    lines = h_src.splitlines(keepends=True)
    last_enum_val_idx = None
    in_enum = False
    for i, line in enumerate(lines):
        stripped = line.strip()
        if re.search(r'\benum\b', stripped):
            in_enum = True
        if in_enum and re.match(r'^[A-Z_][A-Z0-9_]*(\s*=\s*\S+)?\s*,?\s*(//.*)?$', stripped) and stripped:
            last_enum_val_idx = i
        if in_enum and stripped == '};':
            in_enum = False

    if last_enum_val_idx is not None:
        # Ensure the preceding line ends with a comma
        prev = lines[last_enum_val_idx].rstrip('\n')
        if not prev.rstrip().endswith(','):
            lines[last_enum_val_idx] = prev.rstrip() + ',\n'
        lines.insert(last_enum_val_idx + 1,
                     '    ACTION_MESSENGER,\n'
                     '    ACTION_SCRAMBLER,\n')
        with open(h_path, 'w') as f:
            f.writelines(lines)
        print(f"\nPatched {h_path}: ACTION_MESSENGER, ACTION_SCRAMBLER added after line {last_enum_val_idx + 1}")
    else:
        print(f"\nERROR: could not locate enum values in {h_path}", file=sys.stderr)
        sys.exit(1)

# ---- Patch action.c ----
if not os.path.exists(c_path):
    print("ERROR: action.c not found", file=sys.stderr)
    sys.exit(1)

with open(c_path) as f:
    c_src = f.read()

if "ACTION_MESSENGER" in c_src:
    print("action.c already patched.")
else:
    includes = '#include "App/messenger.h"\n#include "App/scrambler.h"\n'
    c_src = re.sub(r'(#include\s+"[^"]+"\s*\n)(?!#include)', r'\1' + includes, c_src, count=1)

    cases = (
        '\n        case ACTION_MESSENGER:\n'
        '            MSG_Open();\n'
        '            break;\n'
        '\n        case ACTION_SCRAMBLER:\n'
        '            if (SCRAMBLER_IsEnabled()) SCRAMBLER_Disable();\n'
        '            else SCRAMBLER_Enable();\n'
        '            gEeprom.SCRAMBLER_ENABLED = SCRAMBLER_IsEnabled() ? 1 : 0;\n'
        '            break;\n'
    )
    patched = re.sub(r'(\s*default\s*:)', cases + r'\1', c_src, count=1)
    if patched == c_src:
        patched = re.sub(r'(\n\s*}\s*\n})', cases + r'\1', c_src, count=1)

    if patched != c_src:
        with open(c_path, 'w') as f:
            f.write(patched)
        print(f"Patched {c_path}")
    else:
        print(f"WARNING: could not add cases to {c_path}")
