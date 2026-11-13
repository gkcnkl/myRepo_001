#pragma once
// E-book reader core: opens an EPUB (or .txt), paginates chapters into an
// on-card page cache and renders pages into the 1-bit framebuffer.
//
// Hardware independent: the book file and the cache directory come in as
// FreeInkBook BookSource / CacheStorage adapters (SD card on the device,
// stdio in tools/reader-sim), and every byte of working memory is one block
// handed to open(), so nothing stays allocated after close().

#include <stddef.h>
#include <stdint.h>

#include <BookStorage.h>

namespace reader {

// Panel geometry (portrait page on the 792x528 landscape panel).
constexpr int16_t kPageW = 528;
constexpr int16_t kPageH = 792;
constexpr int16_t kFooterH = 40;  // status line drawn by the UI under the text

// Working memory. Measured on the device: ~161 KB free, but the largest block
// is 111 KB and the rest is scattered over smaller regions. So the two big
// layout arenas share one block, and the small ones have a preferred and a
// minimum size: the caller tries the preferred size, then steps down.
constexpr size_t kScratchArena = 58 * 1024;    // layout + page render (measured peak ~55.5 KB on a 64-bit PC)
constexpr size_t kParseArena = 50 * 1024;      // inflate window + XML parse (measured peak ~46 KB)
constexpr size_t kBigBlock = kScratchArena + kParseArena;  // one allocation, split in two

struct ArenaSpec {
  size_t preferred;
  size_t minimum;  // 0 = optional: the reader works without it
};
// The XML parser (expat) and the glyph rasterizer allocate from the general
// heap, ~20 KB at peak on the device: arenas must leave at least this free.
constexpr size_t kHeapReserve = 28 * 1024;
// ZIP catalog, OPF, spine, TOC (Dubliners: 7.5 KB).
constexpr ArenaSpec kBookArena{16 * 1024, 8 * 1024};
// Glyph caches: ~2.5 KB of fixed tables per face (FREEINK_FONT_ADVANCE_SLOTS=128,
// FREEINK_FONT_GLYPH_SLOTS=48 in the build); the rest caches bitmaps and
// flushes when full.
constexpr ArenaSpec kGlyphArena{12 * 1024, 6 * 1024};
// Real bold face for headings; without it bold is drawn as a double strike.
constexpr ArenaSpec kBoldGlyphArena{8 * 1024, 0};
// Open chapter's page index, ~8 B/page: 6 KB ~750 pages, 2 KB ~250 pages.
constexpr ArenaSpec kIndexArena{6 * 1024, 2 * 1024};
// Book CSS (oversized sheets are skipped); without it the reader's own styles apply.
constexpr ArenaSpec kSheetArena{2 * 1024, 0};

struct Memory {
  uint8_t* big = nullptr;  // kBigBlock bytes: scratch then parse
  uint8_t* book = nullptr;
  uint8_t* glyph = nullptr;
  uint8_t* bold = nullptr;
  uint8_t* index = nullptr;
  uint8_t* sheet = nullptr;
  size_t bookSize = 0, glyphSize = 0, boldSize = 0, indexSize = 0, sheetSize = 0;
};

enum class OpenResult : uint8_t { Ok, NotFound, Encrypted, TooBig, Broken };

// Images larger than this (bytes, uncompressed) are not decoded on the device:
// a 2-3 MB cover JPEG takes tens of seconds on the ESP32-C3. A framed
// placeholder is drawn in their place.
constexpr uint32_t kMaxImageBytes = 200 * 1024;

// Optional progress hook (e.g. a crash breadcrumb): called with a short step
// name ("katalog", "bolum", "sayfa") and a number (chapter or page index).
using TraceFn = void (*)(const char* step, int value);
void setTrace(TraceFn fn);
// Why the last open or chapter layout failed ("sayfalama: OutOfMemory"), or "".
const char* lastError();

// Reading typography, chosen on the device (CONFIRM cycles the size) or on the
// transfer page. Every combination lays out into its own page cache, and the
// reading position survives a change (it is a character offset).
struct Typography {
  uint8_t sizePx = 27;          // body text size
  uint8_t lineSpacingPct = 118; // line height in % of the size
  uint8_t marginX = 30;         // left and right page margin
};
constexpr uint8_t kSizePresets[] = {21, 24, 27, 30, 34};
constexpr uint8_t kSpacingPresets[] = {108, 118, 132};
constexpr uint8_t kMarginPresets[] = {18, 30, 42};
// Clamps a typography to the supported presets (nearest value).
Typography sanitize(const Typography& t);
// The next size preset after `sizePx` (wraps around).
uint8_t nextSize(uint8_t sizePx);
// Applies to the next open(); an open book must be reopened to change.
void setTypography(const Typography& t);
const Typography& typography();

struct Position {
  uint16_t spine = 0;
  uint32_t charOffset = 0;  // layout independent: survives font/size changes
};

class BookReader {
 public:
  BookReader();
  ~BookReader();

  // The buffers in `memory` must outlive the reader. `isText` selects the
  // .txt path (the whole file is one chapter).
  OpenResult open(freeink::book::BookSource& source, freeink::book::CacheStorage& cache, bool isText,
                  const Memory& memory);
  void close();
  bool isOpen() const { return open_; }

  const char* title() const;
  const char* author() const;
  const char* language() const;

  // Moves to a saved position (lays the chapter out first if needed).
  bool seek(const Position& pos);
  Position position() const;

  bool nextPage();  // false at the end of the book
  bool prevPage();  // false at the start

  // Draws the current page into a 792x528 1-bit framebuffer (1 = white),
  // portrait, upright on the X3. Clears the page area first.
  bool render(uint8_t* framebuffer);

  uint16_t chapter() const { return spine_; }
  uint16_t chapterCount() const { return spineCount_; }
  uint32_t page() const { return page_; }
  uint32_t chapterPages() const { return chapterPages_; }
  // Rough whole-book progress 0..100 (chapters weighted equally).
  uint8_t percent() const;
  // Title of the current chapter from the TOC, or "" when unknown.
  const char* chapterTitle() const;
  // Language of the current chapter's text (xml:lang of the book or "tr").
  bool isTurkish() const;

  // Arena high-water marks and capacities in bytes: book, scratch, glyph,
  // bold glyph, index, sheet, parse.
  void memoryReport(size_t out[7], size_t cap[7]) const;

  struct Impl;  // engine state, defined in book_reader.cpp

 private:
  Impl* impl_;
  bool open_ = false;
  uint16_t spine_ = 0;
  uint16_t spineCount_ = 0;
  uint16_t firstSpine_ = 0;  // 1 when a cover-only first chapter is skipped
  uint32_t page_ = 0;
  uint32_t chapterPages_ = 0;

  bool openChapter(uint16_t spine);
};

}  // namespace reader
