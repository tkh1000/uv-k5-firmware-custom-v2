#!/usr/bin/env bash
# ============================================================
# build.sh — local build helper for UV-K5 V1/V2 port
# Clones F4HWN firmware, injects our files, patches, compiles.
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$(mktemp -d /tmp/uv-k5-v12-build.XXXX)"
FW_DIR="$BUILD_DIR/firmware"

echo "==> Cloning F4HWN V1/V2 firmware..."
git clone --depth=1 https://github.com/armel/uv-k5-firmware-custom.git "$FW_DIR"

echo "==> Injecting source files..."
cp "$SCRIPT_DIR/Helper/fsk.h"      "$FW_DIR/Helper/fsk.h"
cp "$SCRIPT_DIR/Helper/fsk.c"      "$FW_DIR/Helper/fsk.c"
cp "$SCRIPT_DIR/Helper/crypto.h"   "$FW_DIR/Helper/crypto.h"
cp "$SCRIPT_DIR/Helper/crypto.c"   "$FW_DIR/Helper/crypto.c"
cp "$SCRIPT_DIR/App/bk4819_fsk.h"  "$FW_DIR/App/bk4819_fsk.h"
cp "$SCRIPT_DIR/App/bk4819_fsk.c"  "$FW_DIR/App/bk4819_fsk.c"
cp "$SCRIPT_DIR/App/messenger.h"   "$FW_DIR/App/messenger.h"
cp "$SCRIPT_DIR/App/messenger.c"   "$FW_DIR/App/messenger.c"
cp "$SCRIPT_DIR/App/scrambler.h"   "$FW_DIR/App/scrambler.h"

echo "==> Running patch scripts..."
python3 "$SCRIPT_DIR/scripts/patch_settings.py"  "$FW_DIR"
python3 "$SCRIPT_DIR/scripts/patch_app.py"        "$FW_DIR"
python3 "$SCRIPT_DIR/scripts/patch_audio_adc.py"  "$FW_DIR"
python3 "$SCRIPT_DIR/scripts/patch_menu.py"        "$FW_DIR"

echo "==> Verifying patch symbols..."
for sym in MSG_Init MSG_TimeSlice10ms MSG_HandleKey SCRAMBLER_ProcessSample; do
    if grep -rq "$sym" "$FW_DIR/"; then
        echo "  OK: $sym"
    else
        echo "  MISSING: $sym"
        exit 1
    fi
done

echo "==> Compiling with Docker..."
cd "$FW_DIR"
chmod +x compile-with-docker.sh
./compile-with-docker.sh

echo ""
echo "==> Build complete!"
echo "    Firmware: $FW_DIR/compiled-firmware/f4hwn.packed.bin"
echo ""
echo "    Copy to a permanent location before this temp dir is cleaned up."
