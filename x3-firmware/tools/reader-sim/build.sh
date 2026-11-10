#!/bin/sh
# Builds the reader simulator. Run from x3-firmware/: tools/reader-sim/build.sh <out-dir>
set -e
B=freeink-sdk/libs/book/FreeInkBook
F=freeink-sdk/libs/font/FreeInkFont
OUT="${1:-/tmp/x3-reader-sim}"
mkdir -p "$OUT/obj"
python3 tools/patch_sdk.py .
INC="-I$B/include -I$B/third_party/expat -I$B/third_party/miniz -I$B/third_party/libunibreak -I$B/third_party/pngle -I$B/third_party/tjpgd -I$B/third_party/stb -I$F/include -I$F/third_party/stb"
for src in miniz_impl expat_xmlparse expat_xmlrole expat_xmltok unibreak_impl pngle_impl tjpgd_impl; do
  [ -f "$OUT/obj/$src.o" ] || cc -O1 -std=c99 $INC -c "$B/src/vendor/$src.c" -o "$OUT/obj/$src.o"
done
CORE="$B/src/FreeInkBook.cpp $B/src/BookCatalog.cpp $B/src/epub/ZipCatalog.cpp $B/src/epub/XmlSax.cpp \
  $B/src/epub/PackageParsers.cpp $B/src/epub/ImageProbe.cpp $B/src/text/EntityFilter.cpp \
  $B/src/text/Hyphenator.cpp $B/src/css/Css.cpp $B/src/layout/ChapterLayout.cpp \
  $B/src/cache/PageCache.cpp $B/src/render/ImageRenderer.cpp $F/src/TtfFont.cpp $B/src/render/PageRenderer.cpp"
G=".pio/libdeps/x3/Adafruit GFX Library"
Q=".pio/libdeps/x3/QRCode/src"
[ -f "$OUT/obj/qrcode.o" ] || cc -O1 -c "$Q/qrcode.c" -I"$Q" -o "$OUT/obj/qrcode.o"
c++ -std=c++17 -O1 -w -DFREEINK_BOOK_SMALL=1 -DFREEINK_FONT_ADVANCE_SLOTS=128 -DFREEINK_FONT_GLYPH_SLOTS=48 -DFREEINK_BOOK_MAX_RUNS_PER_PAGE=448 -include src/reader/stbtt_alloc.h $INC -Itools/preview/shim -I"$G" -I"$Q" \
  $CORE src/reader/book_reader.cpp tools/reader-sim/sim.cpp \
  src/ui.cpp src/app.cpp src/tasks.cpp src/text.cpp src/clock.cpp src/library.cpp "$G/Adafruit_GFX.cpp" \
  "$OUT"/obj/*.o -o "$OUT/reader-sim"
