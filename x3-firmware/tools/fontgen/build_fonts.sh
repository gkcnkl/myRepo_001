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

# Web page fonts for the transfer page (the hotspot has no internet): Playfair
# Display 500 and Inter 400/500/600 of the "Sabah Brifingi" design system,
# subset to Turkish Latin, as woff2. Needs fonttools + brotli (pip).
W=$(mktemp -d)
python3 -m fontTools.varLib.instancer "$P" wght=500 -o "$W/p500.ttf" -q
U="U+0020-007E,U+00A0-00FF,U+011E-011F,U+0130-0131,U+015E-015F,U+2013-2014,U+2018-201D,U+2022,U+2026,U+20BA,U+2190-2193"
for f in "$W/p500.ttf:p500" "$I/Inter-Regular.otf:i400" "$I/Inter-Medium.otf:i500" "$I/Inter-SemiBold.otf:i600"; do
  pyftsubset "${f%%:*}" --unicodes="$U" --flavor=woff2 --layout-features='kern,liga,tnum,lnum' --output-file="$W/${f##*:}.woff2"
  python3 tools/fontgen/bin2h.py "$W/${f##*:}.woff2" "kWebFont_${f##*:}" "src/web/font_${f##*:}.h"
done
