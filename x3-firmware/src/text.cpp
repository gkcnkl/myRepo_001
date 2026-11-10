#include "text.h"

#include <stdint.h>

namespace {

// Same order as EXTRA in tools/fontgen/gen_font.py.
constexpr uint16_t kExtra[] = {0x00C7, 0x00E7, 0x011E, 0x011F, 0x0130, 0x0131, 0x00D6, 0x00F6,
                               0x015E, 0x015F, 0x00DC, 0x00FC, 0x00B7, 0x00B0, 0x2013};

char mapCodepoint(uint32_t cp) {
  if (cp >= 0x20 && cp <= 0x7E) return static_cast<char>(cp);
  for (unsigned i = 0; i < sizeof kExtra / sizeof kExtra[0]; ++i) {
    if (kExtra[i] == cp) return static_cast<char>(0x80 + i);
  }
  if (cp == '\t') return ' ';
  if (cp == 0x2014) return static_cast<char>(0x8E);  // em dash -> en dash
  return '?';
}

}  // namespace

void utf8ToFont(const char* in, char* out, size_t outSize) {
  if (outSize == 0) return;
  size_t o = 0;
  const auto* p = reinterpret_cast<const unsigned char*>(in);
  while (*p && o + 1 < outSize) {
    uint32_t cp;
    int extra;
    if (*p < 0x80) { cp = *p; extra = 0; }
    else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; extra = 1; }
    else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; extra = 2; }
    else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; extra = 3; }
    else { ++p; out[o++] = '?'; continue; }
    ++p;
    bool ok = true;
    for (int i = 0; i < extra; ++i) {
      if ((*p & 0xC0) != 0x80) { ok = false; break; }
      cp = (cp << 6) | (*p++ & 0x3F);
    }
    out[o++] = ok ? mapCodepoint(cp) : '?';
  }
  out[o] = '\0';
}

void upperFont(char* enc, bool turkish) {
  for (auto* p = reinterpret_cast<unsigned char*>(enc); *p; ++p) {
    const unsigned char c = *p;
    if (c == 'i') *p = turkish ? 0x84 : 'I';                  // i -> İ (Turkish) or I
    else if (c >= 'a' && c <= 'z') *p = c - 'a' + 'A';
    else if (c == 0x85) *p = 'I';                             // ı -> I
    else if (c >= 0x81 && c <= 0x8B && (c & 1)) *p = c - 1;   // ç ğ ö ş ü -> upper
  }
}
