#!/usr/bin/env python3
# Menu patching is skipped for F4HWN V1/V2.
# The menu table is built dynamically inside UI_DisplayMenu() and requires
# firmware-specific enum values (MENU_MESSENGER, MENU_SCRAMBLER) that don't
# exist upstream. Access messenger and scrambler via key bindings instead:
#
#   Long-press SIDE1 = open messenger
#   Long-press SIDE2 = toggle scrambler on/off
import sys
print("Menu patch skipped — use SIDE1/SIDE2 long-press to access features.")
sys.exit(0)
