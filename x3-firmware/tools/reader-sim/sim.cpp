// Host simulator for src/reader: opens a book with stdio adapters, renders
// pages exactly as the device would and writes them as portrait PGM images.
// Usage: reader-sim <book.epub|.txt> <cache-dir> <out-dir> [pages] [Gİ arena report]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <vector>

#include <Adafruit_GFX.h>

#include "../../src/reader/book_reader.h"
#include "../../src/ui.h"

static uint8_t fb[528 * 99];

#ifdef HEAPTRACK
extern "C" void heaptrack_start(void);
extern "C" size_t heaptrack_peak(void);
#endif

// Same framebuffer canvas as the device (rotation 3 = upright portrait).
class Canvas : public Adafruit_GFX {
 public:
  Canvas() : Adafruit_GFX(792, 528) { setRotation(3); }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= width() || y >= height()) return;
    const int16_t px = y, py = HEIGHT - 1 - x;
    uint8_t& cell = fb[py * 99 + (px >> 3)];
    const uint8_t mask = 0x80 >> (px & 7);
    if (color) cell &= ~mask;
    else cell |= mask;
  }
};

using namespace freeink::book;

class FileSource : public BookSource {
 public:
  bool open(const char* path) {
    f_ = fopen(path, "rb");
    if (!f_) return false;
    fseek(f_, 0, SEEK_END);
    size_ = ftell(f_);
    return true;
  }
  int32_t readAt(uint64_t off, void* dst, uint32_t len) override {
    if (fseek(f_, static_cast<long>(off), SEEK_SET) != 0) return -1;
    return static_cast<int32_t>(fread(dst, 1, len, f_));
  }
  uint64_t size() const override { return size_; }

 private:
  FILE* f_ = nullptr;
  uint64_t size_ = 0;
};

class DirCache : public CacheStorage {
 public:
  explicit DirCache(const char* dir) : dir_(dir) { mkdir(dir, 0755); }
  bool exists(const char* n) override { FILE* f = fopen(path(n), "rb"); if (f) fclose(f); return f != nullptr; }
  bool remove(const char* n) override { return ::remove(path(n)) == 0; }
  int64_t fileSize(const char* n) override {
    FILE* f = fopen(path(n), "rb"); if (!f) return -1; fseek(f, 0, SEEK_END); long s = ftell(f); fclose(f); return s;
  }
  int32_t readAt(const char* n, uint32_t off, void* dst, uint32_t len) override {
    FILE* f = fopen(path(n), "rb"); if (!f) return -1;
    fseek(f, off, SEEK_SET); int32_t r = static_cast<int32_t>(fread(dst, 1, len, f)); fclose(f); return r;
  }
  bool beginWrite(const char* n) override { snprintf(wpath_, sizeof wpath_, "%s", path(n)); w_ = fopen(wpath_, "w+b"); return w_ != nullptr; }
  bool write(const void* d, uint32_t len) override { return fwrite(d, 1, len, w_) == len; }
  bool endWrite() override { bool ok = fclose(w_) == 0; w_ = nullptr; return ok; }
  int32_t readBackAt(uint32_t off, void* dst, uint32_t len) override {
    if (!w_) return -1; long cur = ftell(w_); fseek(w_, off, SEEK_SET);
    int32_t r = static_cast<int32_t>(fread(dst, 1, len, w_)); fseek(w_, cur, SEEK_SET); return r;
  }

 private:
  const char* path(const char* n) { snprintf(buf_, sizeof buf_, "%s/%s", dir_, n); return buf_; }
  const char* dir_;
  char buf_[512], wpath_[512];
  FILE* w_ = nullptr;
};

static void savePortrait(const uint8_t* fb, const char* path) {
  // Device mapping (Canvas rotation 3): logical (x, y) -> panel (y, 527 - x).
  FILE* f = fopen(path, "wb");
  fprintf(f, "P5\n528 792\n255\n");
  for (int y = 0; y < 792; ++y)
    for (int x = 0; x < 528; ++x) {
      const int px = y, py = 527 - x;
      const bool white = fb[py * 99 + (px >> 3)] & (0x80 >> (px & 7));
      fputc(white ? 255 : 0, f);
    }
  fclose(f);
}

int main(int argc, char** argv) {
  if (argc < 4) { fprintf(stderr, "usage: reader-sim book cache-dir out-dir [pages]\n"); return 2; }
  const int pages = argc > 4 ? atoi(argv[4]) : 3;
  FileSource src;
  if (!src.open(argv[1])) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
  DirCache cache(argv[2]);
  mkdir(argv[3], 0755);
  const bool isText = strstr(argv[1], ".txt") != nullptr;
  // Same arenas as the device; "min" as 5th argument uses every minimum size.
  const bool useMin = argc > 5 && strcmp(argv[5], "min") == 0;
  // "min" = the tightest the device may get: minimums, optional arenas absent.
  auto pick = [useMin](const reader::ArenaSpec& a) { return useMin ? a.minimum : a.preferred; };
  std::vector<uint8_t> big(reader::kBigBlock), b(pick(reader::kBookArena)), gl(pick(reader::kGlyphArena)),
      bo(pick(reader::kBoldGlyphArena)), ix(pick(reader::kIndexArena)), sh(pick(reader::kSheetArena));
  reader::Memory mem;
  mem.big = big.data();
  mem.book = b.data(), mem.bookSize = b.size();
  mem.glyph = gl.data(), mem.glyphSize = gl.size();
  mem.bold = bo.empty() ? nullptr : bo.data(), mem.boldSize = bo.size();
  mem.index = ix.data(), mem.indexSize = ix.size();
  mem.sheet = sh.empty() ? nullptr : sh.data(), mem.sheetSize = sh.size();
  Canvas canvas;
  reader::BookReader r;
#ifdef HEAPTRACK
  heaptrack_start();
#endif
  const reader::OpenResult res = r.open(src, cache, isText, mem);
  printf("open: %d  title=\"%s\" author=\"%s\" lang=%s chapters=%u pages(ch0)=%u\n", static_cast<int>(res), r.title(),
         r.author(), r.language(), r.chapterCount(), r.chapterPages());
  if (res != reader::OpenResult::Ok) return 1;
  for (int i = 0; i < pages; ++i) {
    if (!r.render(fb)) printf("render failed at page %u\n", r.page());
    ui::drawReaderFooter(canvas, r.chapterTitle(), r.percent(), r.page() + 1, r.chapterPages(), r.isTurkish());
    char path[512];
    snprintf(path, sizeof path, "%s/page%02d.pgm", argv[3], i);
    savePortrait(fb, path);
    printf("page %d: chapter %u page %u/%u  %u%%  \"%s\"\n", i, r.chapter(), r.page() + 1, r.chapterPages(),
           r.percent(), r.chapterTitle());
    if (!r.nextPage()) break;
  }
  const reader::Position pos = r.position();
  printf("position: spine %u char %u\n", pos.spine, pos.charOffset);
  size_t hw[7], cap[7];
  r.memoryReport(hw, cap);
  const char* names[7] = {"book", "scratch", "glyph", "bold", "index", "css", "parse"};
#ifdef HEAPTRACK
  printf("heap outside arenas (expat etc.), peak: %zu B\n", heaptrack_peak());
#endif
  printf("high water:");
  for (int i = 0; i < 7; ++i) printf(" %s %zu/%zu", names[i], hw[i], cap[i]);
  printf("\n");
  return 0;
}
