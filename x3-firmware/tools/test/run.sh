#!/bin/sh
# Builds and runs the host unit tests. Run from x3-firmware/.
set -e
OUT="${1:-/tmp/x3-test}"
mkdir -p "$OUT"
g++ -std=c++17 -Wall -Wextra -O1 tools/test/test_host.cpp src/tasks.cpp src/text.cpp src/app.cpp src/clock.cpp -o "$OUT/x3-test"
"$OUT/x3-test"
