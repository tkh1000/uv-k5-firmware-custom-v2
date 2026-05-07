#!/usr/bin/env python3
import re, sys, os

fw = sys.argv[1] if len(sys.argv) > 1 else "firmware"

# Print action.h and action.c so we can see the enum and dispatch structure
for fname in [f"{fw}/app/action.h", f"{fw}/app/action.c"]:
    if os.path.exists(fname):
        with open(fname) as f:
            lines = f.readlines()
        print(f"\n=== {fname} ({len(lines)} lines) ===")
        for i, line in enumerate(lines):
            print(f"{i+1:4}: {line}", end="")
    else:
        print(f"NOT FOUND: {fname}")

# ---- Patch action.h — add enum values ----
h_path = f"{fw}/app/action.h"
if not os.path.exists(h_path):
    print("ERROR: app/action.h not found", file=sys.stderr)
    sys.exit(1)

with open(h_path) as f:
    h_src = f.read()

if "ACTION_MESSENGER" in h_src:
    print("\naction.h already patched, skipping.")
else:
    # Find the last enum value before the closing brace of the action enum
    # Insert ACTION_MESSENGER and ACTION_SCRAMBLER before it
    patched = re.sub(
        r'(\}\s*(?:AppAction_t|ACTION_\w+_t|COMMAND_\w+)?\s*;)',
        '    ACTION_MESSENGER,\n    ACTION_SCRAMBLER,\n' + r'\1',
        h_src, count=1
    )
    # Fallback: insert before last enum value
    if patched == h_src:
        patched = re.sub(
            r'(,?\s*\n\}\s*;)',
            ',\n    ACTION_MESSENGER,\n    ACTION_SCRAMBLER' + r'\1',
            h_src, count=1
        )
    if patched != h_src:
        with open(h_path, 'w') as f:
            f.write(patched)
        print(f"\nPatched {h_path} with ACTION_MESSENGER, ACTION_SCRAMBLER")
    else:
        print(f"\nWARNING: could not patch {h_path} — see content above")

# ---- Patch action.c — add cases to dispatch switch ----
c_path = f"{fw}/app/action.c"
if not os.path.exists(c_path):
    print("ERROR: app/action.c not found", file=sys.stderr)
    sys.exit(1)

with open(c_path) as f:
    c_src = f.read()

if "ACTION_MESSENGER" in c_src:
    print("action.c already patched, skipping.")
else:
    # Add include for messenger and scrambler
    includes = '#include "App/messenger.h"\n#include "App/scrambler.h"\n'
    c_src = re.sub(r'(#include\s+"[^"]+"\s*\n)(?!#include)', r'\1' + includes, c_src, count=1)

    # Find the switch dispatch and add our cases before the default
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
        # Fallback: insert before closing brace of the switch
        patched = re.sub(r'(\s*}\s*//\s*switch|\s*}\s*$)', cases + r'\1', c_src, count=1)

    if patched != c_src:
        with open(c_path, 'w') as f:
            f.write(patched)
        print(f"Patched {c_path} with ACTION_MESSENGER and ACTION_SCRAMBLER cases")
    else:
        print(f"WARNING: could not patch {c_path} dispatch — see content above")
