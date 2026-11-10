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

// Working memory: one buffer per arena, allocated separately by the caller so
// a fragmented heap still fits them (no PSRAM on the X3).
constexpr size_t kBookArena = 24 * 1024;       // ZIP catalog, OPF, spine, TOC (Dubliners: 7.5 KB)
// Layout working set split in two so no single block is ~110 KB (the heap
// is fragmented once Wi-Fi, the display and the SD card are up).
constexpr size_t kScratchArena = 56 * 1024;    // layout + page render (measured peak ~52 KB)
constexpr size_t kParseArena = 50 * 1024;      // inflate window + XML parse (measured peak ~46 KB)
constexpr size_t kGlyphArena = 16 * 1024;      // rasterized glyph cache, regular face (flushes when full)
constexpr size_t kBoldGlyphArena = 12 * 1024;  // bold face (headings); each face flushes its own arena
// Each face also reserves ~7 KB of fixed tables (FREEINK_FONT_ADVANCE_SLOTS=256 in the build).
constexpr size_t kIndexArena = 12 * 1024;      // open chapter's page index (~8 B/page: ~1500 pages)
constexpr size_t kSheetArena = 4 * 1024;       // book CSS (oversized sheets are skipped)

struct Memory {
  uint8_t* book;     // kBookArena bytes
  uint8_t* scratch;  // kScratchArena
  uint8_t* parse;    // kParseArena
  uint8_t* glyph;    // kGlyphArena
  uint8_t* bold;     // kBoldGlyphArena
  uint8_t* index;    // kIndexArena
  uint8_t* sheet;    // kSheetArena
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

  // Arena high-water marks in bytes: book, scratch, glyph, bold glyph, index, sheet, parse.
  void memoryReport(size_t out[7]) const;

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
