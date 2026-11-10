#include "library.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

uint32_t fnv1a(const char* s) {
  uint32_t h = 2166136261u;
  for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
  return h;
}

bool endsWithNoCase(const char* s, const char* suffix) {
  const size_t n = strlen(s), m = strlen(suffix);
  if (n < m) return false;
  for (size_t i = 0; i < m; ++i) {
    char a = s[n - m + i], b = suffix[i];
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
    if (a != b) return false;
  }
  return true;
}

void copy(char* dst, size_t size, const char* src, size_t n) {
  if (n >= size) {
    n = size - 1;
    while (n > 0 && (static_cast<unsigned char>(src[n]) & 0xC0) == 0x80) --n;  // keep UTF-8 whole
  }
  memcpy(dst, src, n);
  dst[n] = '\0';
}

// Case-folded byte for sorting (ASCII only; Turkish letters keep byte order).
int fold(unsigned char c) { return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c; }

int compareTitles(const void* a, const void* b) {
  const auto* x = reinterpret_cast<const unsigned char*>(static_cast<const BookInfo*>(a)->title);
  const auto* y = reinterpret_cast<const unsigned char*>(static_cast<const BookInfo*>(b)->title);
  while (*x && fold(*x) == fold(*y)) ++x, ++y;
  return fold(*x) - fold(*y);
}

}  // namespace

bool bookFromFileName(const char* name, uint32_t size, BookInfo& out) {
  if (name[0] == '.' || name[0] == '\0') return false;
  const bool epub = endsWithNoCase(name, ".epub");
  const bool txt = endsWithNoCase(name, ".txt");
  if (!epub && !txt) return false;
  out = BookInfo{};
  copy(out.file, sizeof out.file, name, strlen(name));
  snprintf(out.id, sizeof out.id, "%08lx", static_cast<unsigned long>(fnv1a(name)));
  out.size = size;
  out.isText = txt;
  // Title: the name without extension, '_' and '-' as spaces.
  const size_t stem = strlen(name) - (epub ? 5 : 4);
  copy(out.title, sizeof out.title, name, stem);
  for (char* p = out.title; *p; ++p)
    if (*p == '_') *p = ' ';
  return true;
}

void parseBookState(const char* data, size_t len, BookInfo& b) {
  size_t pos = 0;
  while (pos < len) {
    size_t end = pos;
    while (end < len && data[end] != '\n') ++end;
    const char* line = data + pos;
    size_t n = end - pos;
    pos = end + 1;
    while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ')) --n;
    const char* colon = static_cast<const char*>(memchr(line, ':', n));
    if (colon == nullptr) continue;
    const size_t klen = static_cast<size_t>(colon - line);
    const char* v = colon + 1;
    size_t vn = n - klen - 1;
    while (vn > 0 && *v == ' ') ++v, --vn;
    char val[100];
    copy(val, sizeof val, v, vn);
    auto key = [&](const char* k) { return strlen(k) == klen && strncmp(line, k, klen) == 0; };
    if (key("title")) copy(b.title, sizeof b.title, v, vn);
    else if (key("author")) copy(b.author, sizeof b.author, v, vn);
    else if (key("started")) {
      int y = 0, m = 0, d = 0;
      if (sscanf(val, "%d-%d-%d", &y, &m, &d) == 3 && y > 2000 && m >= 1 && m <= 12 && d >= 1 && d <= 31) {
        b.started.year = static_cast<uint16_t>(y);
        b.started.month = static_cast<uint8_t>(m);
        b.started.day = static_cast<uint8_t>(d);
      }
    } else if (key("lastRead")) b.lastRead = strtoull(val, nullptr, 10);
    else if (key("spine")) b.spine = static_cast<uint16_t>(strtoul(val, nullptr, 10));
    else if (key("char")) b.charOffset = static_cast<uint32_t>(strtoul(val, nullptr, 10));
    else if (key("percent")) {
      const unsigned long p = strtoul(val, nullptr, 10);
      b.percent = static_cast<uint8_t>(p > 100 ? 100 : p);
    } else if (key("finished")) b.finished = val[0] == '1';
  }
}

size_t serializeBookState(const BookInfo& b, char* buf, size_t bufSize) {
  int n = snprintf(buf, bufSize, "title: %s\nauthor: %s\n", b.title, b.author);
  if (b.started.known() && n >= 0 && static_cast<size_t>(n) < bufSize) {
    n += snprintf(buf + n, bufSize - n, "started: %04u-%02u-%02u\n", b.started.year, b.started.month, b.started.day);
  }
  if (n >= 0 && static_cast<size_t>(n) < bufSize) {
    n += snprintf(buf + n, bufSize - n, "lastRead: %llu\nspine: %u\nchar: %lu\npercent: %u\nfinished: %d\n",
                  static_cast<unsigned long long>(b.lastRead), b.spine, static_cast<unsigned long>(b.charOffset),
                  b.percent, b.finished ? 1 : 0);
  }
  if (n < 0) return 0;
  return static_cast<size_t>(n) < bufSize ? static_cast<size_t>(n) : bufSize - 1;
}

void Library::sortByTitle() { qsort(books, count, sizeof books[0], compareTitles); }

int Library::inProgress(const BookInfo** out, int max) const {
  const BookInfo* all[kMax];
  int n = 0;
  for (int i = 0; i < count; ++i) {
    if (books[i].lastRead != 0 && !books[i].finished) all[n++] = &books[i];
  }
  // Newest first (insertion sort; at most kMax items).
  for (int i = 1; i < n; ++i) {
    const BookInfo* b = all[i];
    int j = i;
    while (j > 0 && all[j - 1]->lastRead < b->lastRead) {
      all[j] = all[j - 1];
      --j;
    }
    all[j] = b;
  }
  if (n > max) n = max;
  for (int i = 0; i < n; ++i) out[i] = all[i];
  return n;
}

int16_t ShelfLayout::spineWidth(const BookInfo& b) {
  // ~34 px for a short story up to ~64 px for a long novel (log-ish scale).
  uint32_t kb = b.size / 1024;
  int16_t w = 32;
  while (kb > 64 && w < 64) {
    kb /= 2;
    w += 6;
  }
  return w;
}

int16_t ShelfLayout::spineHeight(const BookInfo& b) {
  // Deterministic per book: 116..144 px so the shelf looks lived in.
  uint32_t h = 0;
  for (const char* p = b.id; *p; ++p) h = h * 31 + static_cast<uint8_t>(*p);
  return static_cast<int16_t>(kMaxSpineH - static_cast<int16_t>(h % 29));
}

int ShelfLayout::place(const Library& lib, SpineBox* boxes, int maxBoxes) {
  int shelf = 0;
  int16_t x = kLeft;
  const int items = lib.count + 1;
  for (int i = 0; i < items && i < maxBoxes; ++i) {
    const bool add = i == lib.count;
    const int16_t w = add ? kAddW : spineWidth(lib.books[i]);
    const int16_t h = add ? kMaxSpineH - 20 : spineHeight(lib.books[i]);
    if (x + w > kRight && x > kLeft) {
      ++shelf;
      x = kLeft;
    }
    const int16_t boardY = kTop + (shelf % kShelves) * kShelfH + kMaxSpineH;
    boxes[i] = SpineBox{x, static_cast<int16_t>(boardY - h), w, h, shelf};
    x += w + kGap;
  }
  return shelf + 1;
}
