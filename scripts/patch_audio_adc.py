#!/usr/bin/env python3
import re, sys, os

fw = sys.argv[1] if len(sys.argv) > 1 else "firmware"

candidates = [
    f"{fw}/App/audio.c",
    f"{fw}/app/audio.c",
    f"{fw}/App/app.c",
    f"{fw}/app/app.c",
    f"{fw}/driver/bk4819.c",
]

adc_patterns = [
    r'(ADC_GetValue\s*\([^)]*\))',
    r'(BK4819_GetADC\s*\([^)]*\))',
    r'(gADC_Result\s*\[\s*\d+\s*\])',
    r'(\w+\s*=\s*\*\s*\(volatile uint16_t\s*\*\)\s*ADC_\w+)',
]

patched_any = False
for path in candidates:
    if not os.path.exists(path):
        continue
    with open(path) as f:
        content = f.read()
    if "SCRAMBLER_ProcessSample" in content:
        print(f"{path}: already patched, skipping.")
        patched_any = True
        continue
    for pat in adc_patterns:
        new_content = re.sub(pat, r'SCRAMBLER_ProcessSample(\1)', content, count=1)
        if new_content != content:
            if '#include "App/scrambler.h"' not in new_content:
                new_content = '#include "App/scrambler.h"\n' + new_content
            with open(path, 'w') as f:
                f.write(new_content)
            print(f"Patched ADC read in {path}")
            patched_any = True
            break

if not patched_any:
    print("WARNING: could not locate ADC read pattern — scrambler RX path may need manual hookup.")
