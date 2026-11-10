#!/bin/sh
# Usage: tools/preview/build.sh <out-dir>   (run from x3-firmware/, after `pio run`)
set -e
G=".pio/libdeps/x3/Adafruit GFX Library"
OUT="${1:-.}"
mkdir -p "$OUT"
g++ -std=c++17 -O1 -w -Itools/preview/shim -I"$G" \
  tools/preview/preview.cpp src/ui.cpp src/app.cpp src/tasks.cpp src/text.cpp "$G/Adafruit_GFX.cpp" -o "$OUT/x3-preview"
"$OUT/x3-preview" "$OUT"
