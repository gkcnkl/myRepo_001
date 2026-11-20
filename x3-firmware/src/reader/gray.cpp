#include "gray.h"

#include <string.h>

namespace gray {

namespace {
const uint8_t kBayer4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

inline void putBit(uint8_t* row, int col, bool on) {
  const uint8_t mask = static_cast<uint8_t>(0x80 >> (col & 7));
  if (on) row[col >> 3] |= mask;
  else row[col >> 3] &= static_cast<uint8_t>(~mask);
}
}  // namespace

size_t plan(Map& m, int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t shift) {
  m.x = x;
  m.y = y;
  m.w = w;
  m.h = h;
  m.shift = shift;
  m.depth = shift == 0 ? 2 : 4;
  const uint16_t step = static_cast<uint16_t>(1u << shift);
  m.mw = static_cast<uint16_t>((w + step - 1) / step);
  m.mh = static_cast<uint16_t>((h + step - 1) / step);
  const size_t cells = static_cast<size_t>(m.mw) * m.mh;
  return m.depth == 2 ? (cells + 3) / 4 : (cells + 1) / 2;
}

uint8_t quantize(uint8_t g, int x, int y) {
  // Three steps between four levels; the threshold moves through each step.
  const int t = (kBayer4[y & 3][x & 3] * 255 + 128) / 16;  // 8 .. 247
  const int lv = (g * 3 + t) / 255;
  return static_cast<uint8_t>(lv > 3 ? 3 : lv);
}

void setCell(Map& m, int mx, int my, uint8_t v) {
  const size_t i = static_cast<size_t>(my) * m.mw + mx;
  if (m.depth == 2) {
    const int sh = static_cast<int>((3 - (i & 3)) * 2);
    uint8_t& b = m.data[i >> 2];
    b = static_cast<uint8_t>((b & ~(3 << sh)) | ((v & 3) << sh));
  } else {
    const int sh = (i & 1) ? 0 : 4;
    uint8_t& b = m.data[i >> 1];
    b = static_cast<uint8_t>((b & ~(15 << sh)) | ((v & 15) << sh));
  }
}

uint8_t cell(const Map& m, int mx, int my) {
  const size_t i = static_cast<size_t>(my) * m.mw + mx;
  if (m.depth == 2) return static_cast<uint8_t>((m.data[i >> 2] >> ((3 - (i & 3)) * 2)) & 3);
  return static_cast<uint8_t>((m.data[i >> 1] >> ((i & 1) ? 0 : 4)) & 15);
}

uint8_t levelAt(const Map& m, int x, int y) {
  const uint8_t v = cell(m, (x - m.x) >> m.shift, (y - m.y) >> m.shift);
  return m.depth == 2 ? v : quantize(static_cast<uint8_t>(v * 17), x, y);
}

void storeRow(Map& m, uint16_t y, const uint8_t* g, uint16_t width) {
  if (m.data == nullptr || (y & ((1u << m.shift) - 1)) != 0) return;
  const int my = y >> m.shift;
  if (my >= m.mh) return;
  const int n = width < m.w ? width : m.w;
  for (int mx = 0; mx < m.mw; ++mx) {
    const int sx = mx << m.shift;
    if (sx >= n) break;
    if (m.depth == 2) {
      setCell(m, mx, my, quantize(g[sx], m.x + sx, m.y + y));
    } else {
      // Average the block's first row (cheap, and smoother than one sample).
      const int e = sx + (1 << m.shift) < n ? sx + (1 << m.shift) : n;
      int sum = 0;
      for (int k = sx; k < e; ++k) sum += g[k];
      setCell(m, mx, my, static_cast<uint8_t>((sum / (e - sx) + 8) / 17));
    }
  }
}

void applyBase(uint8_t* fb, const Map* maps, int count) {
  for (int k = 0; k < count; ++k) {
    const Map& m = maps[k];
    if (m.data == nullptr) continue;
    for (int x = m.x < 0 ? 0 : m.x; x < m.x + m.w && x < kPanelRows; ++x) {
      uint8_t* row = fb + static_cast<size_t>(kPanelRows - 1 - x) * kRowBytes;
      for (int y = m.y < 0 ? 0 : m.y; y < m.y + m.h && y < kPanelCols; ++y) putBit(row, y, levelAt(m, x, y) >= 2);
    }
  }
}

void buildStrip(const uint8_t* fb, const Map* maps, int count, int row0, int n, uint8_t* lsb, uint8_t* msb) {
  for (int r = 0; r < n; ++r) {
    const int row = row0 + r;
    uint8_t* l = lsb + static_cast<size_t>(r) * kRowBytes;
    uint8_t* h = msb + static_cast<size_t>(r) * kRowBytes;
    // B/W pixels: white (1) is (1,1), black (0) is (0,0) - the frame row itself.
    memcpy(l, fb + static_cast<size_t>(row) * kRowBytes, kRowBytes);
    memcpy(h, fb + static_cast<size_t>(row) * kRowBytes, kRowBytes);
    const int x = kPanelRows - 1 - row;  // logical column of this panel row
    for (int k = 0; k < count; ++k) {
      const Map& m = maps[k];
      if (m.data == nullptr || x < m.x || x >= m.x + m.w) continue;
      const int y0 = m.y < 0 ? 0 : m.y;
      const int y1 = m.y + m.h > kPanelCols ? kPanelCols : m.y + m.h;
      for (int y = y0; y < y1; ++y) {  // logical y is the panel column
        const uint8_t lv = levelAt(m, x, y);
        putBit(l, y, lv & 1);
        putBit(h, y, lv >> 1);
      }
    }
  }
}

}  // namespace gray
