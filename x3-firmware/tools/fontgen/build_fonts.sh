#!/bin/sh
# Regenerates src/fonts/*.h from the Inter typeface (SIL Open Font License 1.1).
# Run from x3-firmware/. Inter ships with most Linux distros (fonts-inter).
set -e
I=/usr/share/fonts/opentype/inter
G="python3 tools/fontgen/gen_font.py"
$G $I/Inter-Regular.otf 22 Inter22 src/fonts/Inter22.h
$G $I/Inter-SemiBold.otf 22 InterSemi22 src/fonts/InterSemi22.h
$G $I/Inter-Regular.otf 17 Inter17 src/fonts/Inter17.h
$G $I/Inter-Bold.otf 34 InterBold34 src/fonts/InterBold34.h
$G $I/Inter-Bold.otf 150 InterBold150Digits src/fonts/InterBold150Digits.h digits
