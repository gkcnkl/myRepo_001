#!/bin/sh
# Regenerates src/fonts/*.h. Run from x3-firmware/.
# Inter and Playfair Display are both under the SIL Open Font License 1.1.
# Inter ships with most Linux distros (fonts-inter); Playfair Display is
# downloaded from the Google Fonts repository.
set -e
I=/usr/share/fonts/opentype/inter
P="${PLAYFAIR_TTF:-/tmp/PlayfairDisplay.ttf}"
[ -f "$P" ] || curl -sSfo "$P" "https://raw.githubusercontent.com/google/fonts/main/ofl/playfairdisplay/PlayfairDisplay%5Bwght%5D.ttf"
G="python3 tools/fontgen/gen_font.py"
$G "$P" 160 Play160Digits src/fonts/Play160Digits.h digits wght=500
# "Sabah Brifingi" style
$G $I/Inter-Medium.otf 18 InterMed18 src/fonts/InterMed18.h
$G $I/Inter-Regular.otf 14 Inter14 src/fonts/Inter14.h
$G $I/Inter-SemiBold.otf 12 InterSemi12 src/fonts/InterSemi12.h
$G "$P" 46 Play46 src/fonts/Play46.h wght=500
$G "$P" 26 Play26 src/fonts/Play26.h wght=500
$G "$P" 18 Play18 src/fonts/Play18.h wght=500
