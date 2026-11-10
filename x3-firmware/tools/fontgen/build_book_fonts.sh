#!/bin/sh
# Regenerates the embedded reading fonts and Turkish hyphenation data.
# Literata (SIL OFL 1.1, Google Fonts) is instanced at text size (opsz 12) in
# Regular and Bold and subset to Latin + Turkish; needs fonttools
# (pip install fonttools). Run from x3-firmware/.
set -e
W="${WORK:-/tmp/x3-bookfonts}"
mkdir -p "$W"
[ -f "$W/Literata.ttf" ] || curl -sSfo "$W/Literata.ttf" "https://raw.githubusercontent.com/google/fonts/main/ofl/literata/Literata%5Bopsz,wght%5D.ttf"
[ -f "$W/hyph-tr.pat.txt" ] || curl -sSfo "$W/hyph-tr.pat.txt" "https://raw.githubusercontent.com/hyphenation/tex-hyphen/master/hyph-utf8/tex/generic/hyph-utf8/patterns/txt/hyph-tr.pat.txt"
U="U+0020-007E,U+00A0-017F,U+0218-021B,U+02BC,U+02C6,U+02DC,U+2010-2027,U+2030,U+2032-2033,U+2039-203A,U+20AC,U+20BA,U+2122,U+2212,U+FB01-FB02"
for spec in 400:Regular 700:Bold; do
  w=${spec%%:*}; n=${spec##*:}
  fonttools varLib.instancer "$W/Literata.ttf" opsz=12 wght=$w -o "$W/Literata-$n-full.ttf" -q
  pyftsubset "$W/Literata-$n-full.ttf" --unicodes="$U" --layout-features='kern,liga' --output-file="$W/Literata-$n.ttf"
done
python3 tools/fontgen/bin2h.py "$W/Literata-Regular.ttf" kLiterataRegular src/reader/font_literata_regular.h
python3 tools/fontgen/bin2h.py "$W/Literata-Bold.ttf" kLiterataBold src/reader/font_literata_bold.h
python3 freeink-sdk/libs/book/FreeInkBook/tools/hyphc.py "$W/hyph-tr.pat.txt" src/reader/hyph_tr.h
