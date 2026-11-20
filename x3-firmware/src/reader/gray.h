#pragma once
// Four-tone images on the X3 panel (hardware independent, host tested).
//
// The page is drawn in black and white as always; images are additionally
// kept as 2-bit "gray maps" (4 levels, 0 = black .. 3 = white) in logical
// portrait coordinates. Before the gray refresh, buildStrip() turns the B/W
// frame plus the maps into the panel's two absolute grayscale planes, a few
// panel rows at a time, so the planes never exist in full in memory.
//
// Absolute plane encoding (LSB, MSB): black (0,0), dark (1,0), light (0,1),
// white (1,1); bits MSB-first like the frame buffer.

#include <stddef.h>
#include <stdint.h>

namespace gray {

constexpr int kPanelCols = 792;  // native panel row length (logical y)
constexpr int kPanelRows = 528;  // native panel rows (logical x, reversed)
constexpr int kRowBytes = kPanelCols / 8;

// One image. It covers logical (x, y, w, h); the map holds it at
// 1 / (1 << shift) resolution. At full resolution it keeps the final levels
// (2 bits, dithered as decoded); scaled down it keeps 16 gray steps (4 bits)
// and dithers at panel resolution while the strips are built, so a cover kept
// at half size still shows smooth tones instead of coarse bands.
struct Map {
  uint8_t* data = nullptr;
  int16_t x = 0, y = 0;
  uint16_t w = 0, h = 0;
  uint8_t shift = 0;
  uint8_t depth = 2;        // bits per map pixel: 2 (levels) or 4 (gray steps)
  uint16_t mw = 0, mh = 0;  // map size: ceil(w >> shift), ceil(h >> shift)
};

// Sets the size fields for an image at (x, y, w, h) with `shift`; returns the
// bytes its data needs.
size_t plan(Map& m, int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t shift);

// 8-bit gray (0 = black) to a level 0..3, ordered (Bayer 4x4) dithered at
// (x, y) so smooth gradients keep their tone.
uint8_t quantize(uint8_t g, int x, int y);

// Raw map cells: a level (depth 2) or a gray step 0..15 (depth 4).
void setCell(Map& m, int mx, int my, uint8_t v);
uint8_t cell(const Map& m, int mx, int my);
// The level of logical pixel (x, y) inside the map's rectangle.
uint8_t levelAt(const Map& m, int x, int y);

// Stores one decoded image row (placement row `y`, `width` pixels) into the
// map, keeping every (1 << shift)th row and pixel.
void storeRow(Map& m, uint16_t y, const uint8_t* g, uint16_t width);

// Makes the B/W frame inside the maps agree with them, as the base of a gray
// pass: black and dark become black, light and white become white. The X3's
// absolute gray drive only moves the two gray states and leaves black and
// white as the base left them, so a base dithered on its own would leave
// stray dots where it disagrees with the levels.
void applyBase(uint8_t* fb, const Map* maps, int count);

// Fills panel rows [row0, row0 + n) of both planes (n * kRowBytes bytes each)
// from the B/W frame `fb` (1 = white) and the maps on top of it.
void buildStrip(const uint8_t* fb, const Map* maps, int count, int row0, int n, uint8_t* lsb, uint8_t* msb);

}  // namespace gray
