#include "book_reader.h"

#include <new>

#include <FreeInkBook.h>
#include <cache/PageCache.h>
#include <css/Css.h>
#include <layout/ChapterLayout.h>
#include <render/PageRenderer.h>
#include <render/TtfFont.h>
#include <string.h>
#include <text/Hyphenator.h>
#include <text/hyph_en_us.h>

#include "font_literata_bold.h"
#include "font_literata_regular.h"
#include "hyph_tr.h"

namespace reader {

using namespace freeink::book;

namespace {

// Typography: Literata 21 px, justified, hyphenated (Turkish or English).
constexpr uint16_t kBodySizePx = 21;
constexpr int16_t kMarginX = 30;
constexpr int16_t kMarginTop = 30;
// Bump when anything that changes page layout changes (fonts, metrics).
constexpr uint32_t kFontFingerprint = 0x4C495431u /* "LIT1" */ ^ (kBodySizePx << 8);

TraceFn gTrace = nullptr;
void trace(const char* step, int value) {
  if (gTrace != nullptr) gTrace(step, value);
}

// Thin frame with a cross where an image was skipped (logical portrait coords;
// FrameRotation::Portrait maps (x, y) to panel (y, 527 - x)).
void placeholder(uint8_t* fb, int16_t x, int16_t y, int16_t w, int16_t h) {
  auto plot = [fb](int px, int py) {
    if (px < 0 || py < 0 || px >= kPageW || py >= kPageH) return;
    const int col = py, row = kPageW - 1 - px;
    fb[row * (kPageH / 8) + (col >> 3)] &= static_cast<uint8_t>(~(0x80 >> (col & 7)));
  };
  for (int i = 0; i < w; ++i) plot(x + i, y), plot(x + i, y + h - 1);
  for (int i = 0; i < h; ++i) plot(x, y + i), plot(x + w - 1, y + i);
  const int n = w > h ? w : h;
  for (int i = 0; i < n; i += 2) {
    plot(x + i * w / n, y + i * h / n);
    plot(x + w - 1 - i * w / n, y + i * h / n);
  }
}

bool startsWithLang(const char* lang, const char* prefix) {
  if (lang == nullptr) return false;
  const size_t n = strlen(prefix);
  if (strncmp(lang, prefix, n) != 0) return false;
  return lang[n] == '\0' || lang[n] == '-' || lang[n] == '_';
}

}  // namespace

struct BookReader::Impl {
  BookSource* source = nullptr;
  CacheStorage* cache = nullptr;
  bool isText = false;

  Arena bookArena, scratch, parse, glyphArena, boldGlyphArena, indexArena, sheetArena;
  Book book;
  TtfFont regular, bold;
  FontChain fonts;
  Hyphenator hyphenator;
  CssStylesheet sheet;
  LayoutParams params;
  uint32_t generation = 0;
  PageCacheReader pages;
  bool pagesOpen = false;
};

void setTrace(TraceFn fn) { gTrace = fn; }

namespace {
BookReader::Impl* gImpl = nullptr;  // one reader per device; storage below
alignas(8) uint8_t gImplStorage[sizeof(BookReader::Impl)];
}  // namespace

BookReader::BookReader() {
  if (gImpl == nullptr) gImpl = new (gImplStorage) Impl();
  impl_ = gImpl;
}

BookReader::~BookReader() { close(); }

OpenResult BookReader::open(BookSource& source, CacheStorage& cache, bool isText, const Memory& mem) {
  close();
  Impl& m = *impl_;
  m.~Impl();
  new (&m) Impl();
  m.source = &source;
  m.cache = &cache;
  m.isText = isText;

  m.scratch.init(mem.big, kScratchArena);
  m.parse.init(mem.big + kScratchArena, kParseArena);
  m.bookArena.init(mem.book, mem.bookSize);
  m.glyphArena.init(mem.glyph, mem.glyphSize);
  m.boldGlyphArena.init(mem.bold, mem.boldSize);
  m.indexArena.init(mem.index, mem.indexSize);
  m.sheetArena.init(mem.sheet, mem.sheetSize);

  const char* lang = "tr";
  if (!isText) {
    trace("katalog", 0);
    // Container parse needs an inflate window too: use the parse arena.
    const BookStatus st = m.book.open(source, m.bookArena, m.parse);
    m.parse.reset();
    if (st == BookStatus::Encrypted) return OpenResult::Encrypted;
    if (st == BookStatus::OutOfMemory) return OpenResult::TooBig;
    if (st != BookStatus::Ok) return OpenResult::Broken;
    if (m.book.spineCount() == 0) return OpenResult::Broken;
    if (m.book.metadata().language[0] != '\0') lang = m.book.metadata().language;

    CssStylesheetBuilder builder;
    builder.begin(m.sheetArena);
    for (size_t i = 0; i < m.book.manifestCount(); ++i) {
      const ManifestItem* item = m.book.manifestItem(i);
      if (item->mediaType == nullptr || strcmp(item->mediaType, "text/css") != 0) continue;
      const ZipEntry* entry = m.book.zip().find(item->href);
      if (entry != nullptr) builder.addSheet(source, *entry, m.parse);
      m.parse.reset();
    }
    m.sheet = builder.finish();
  }

  if (!m.regular.init(kLiterataRegular, kLiterataRegular_size, m.glyphArena) ||
      !m.bold.init(kLiterataBold, kLiterataBold_size, m.boldGlyphArena)) {
    return OpenResult::Broken;
  }
  m.fonts.add(&m.regular, freeink::font::StyleNone);
  m.fonts.add(&m.bold, freeink::font::StyleBold);

  if (startsWithLang(lang, "tr")) {
    m.hyphenator.init(freeink::book::k_hyph_tr, freeink::book::k_hyph_tr_size);
  } else if (startsWithLang(lang, "en")) {
    m.hyphenator.init(freeink::book::k_hyph_en_us, freeink::book::k_hyph_en_us_size);
  }

  LayoutParams& lp = m.params;
  lp.pageWidth = kPageW;
  lp.pageHeight = kPageH;
  lp.marginLeft = lp.marginRight = kMarginX;
  lp.marginTop = kMarginTop;
  lp.marginBottom = kFooterH + 8;
  lp.baseSizePx = kBodySizePx;
  lp.language = lang;
  lp.font = &m.fonts;
  lp.lineSpacingPct = 112;
  lp.defaultAlign = TextAlign::Justify;
  lp.stylesheet = isText ? nullptr : &m.sheet;
  lp.hyphenator = m.hyphenator.ready() ? &m.hyphenator : nullptr;
  m.generation = layoutGenerationHash(lp, kFontFingerprint);

  spineCount_ = isText ? 1 : static_cast<uint16_t>(m.book.spineCount());
  open_ = true;
  if (!openChapter(0)) {
    close();
    return OpenResult::Broken;
  }
  // A cover-only first chapter (an image, no text) is skipped: start reading
  // where the text starts.
  if (!isText && spineCount_ > 1 && chapterPages_ <= 1) {
    Page page{};
    m.scratch.reset();
    const bool coverOnly = chapterPages_ == 0 || (m.pages.readPage(0, m.scratch, &page) == BookStatus::Ok &&
                                                   page.runCount == 0);
    m.scratch.reset();
    if (coverOnly) {
      firstSpine_ = 1;
      if (!openChapter(1)) {
        close();
        return OpenResult::Broken;
      }
    }
  }
  return OpenResult::Ok;
}

void BookReader::close() {
  open_ = false;
  firstSpine_ = 0;
  impl_->pagesOpen = false;
  spine_ = spineCount_ = 0;
  page_ = chapterPages_ = 0;
}

const char* BookReader::title() const { return impl_->isText ? "" : impl_->book.metadata().title; }
const char* BookReader::author() const { return impl_->isText ? "" : impl_->book.metadata().author; }
const char* BookReader::language() const { return impl_->params.language; }

bool BookReader::openChapter(uint16_t spine) {
  Impl& m = *impl_;
  m.pagesOpen = false;
  m.indexArena.reset();
  m.scratch.reset();
  m.parse.reset();

  trace("bolum", spine);
  char name[64];
  if (!pageCacheName(spine, m.generation, name, sizeof name)) return false;
  if (m.pages.open(*m.cache, name, m.generation, m.indexArena) != BookStatus::Ok) {
    // Not cached yet (or stale): lay the chapter out into the cache once.
    m.indexArena.reset();
    PageCacheWriter writer;
    if (!writer.begin(*m.cache, name, m.generation, m.scratch)) return false;
    uint32_t totalChars = 0;
    BookStatus st;
    if (m.isText) {
      st = ChapterLayout::layoutPlainText(*m.source, m.params, m.scratch, writer, nullptr, &totalChars);
    } else {
      const ManifestItem* item = m.book.spineItem(spine);
      const ZipEntry* entry = item != nullptr ? m.book.zip().find(item->href) : nullptr;
      if (entry == nullptr) return false;
      st = ChapterLayout::layout(*m.source, m.book.zip(), *entry, item->href, m.params, m.scratch, writer,
                                 nullptr, &totalChars, &m.parse);
    }
    if (st != BookStatus::Ok || writer.failed()) return false;
    writer.setTotalChars(totalChars);
    if (!writer.finish()) return false;
    m.scratch.reset();
    m.parse.reset();
    if (m.pages.open(*m.cache, name, m.generation, m.indexArena) != BookStatus::Ok) return false;
  }
  m.pagesOpen = true;
  spine_ = spine;
  page_ = 0;
  chapterPages_ = m.pages.pageCount();
  return chapterPages_ > 0 || true;
}

bool BookReader::seek(const Position& pos) {
  if (!open_) return false;
  uint16_t spine = pos.spine < spineCount_ ? pos.spine : firstSpine_;
  if (spine < firstSpine_) spine = firstSpine_;
  if (spine != spine_ || !impl_->pagesOpen) {
    if (!openChapter(spine)) return false;
  }
  page_ = impl_->pages.pageForChar(pos.charOffset);
  if (page_ >= chapterPages_) page_ = chapterPages_ > 0 ? chapterPages_ - 1 : 0;
  return true;
}

Position BookReader::position() const {
  Position p;
  p.spine = spine_;
  p.charOffset = impl_->pagesOpen && chapterPages_ > 0 ? impl_->pages.charStart(page_) : 0;
  return p;
}

bool BookReader::nextPage() {
  if (!open_) return false;
  if (page_ + 1 < chapterPages_) {
    ++page_;
    return true;
  }
  // Next non-empty chapter.
  for (uint16_t s = spine_ + 1; s < spineCount_; ++s) {
    if (!openChapter(s)) return false;
    if (chapterPages_ > 0) return true;
  }
  return false;
}

bool BookReader::prevPage() {
  if (!open_) return false;
  if (page_ > 0) {
    --page_;
    return true;
  }
  for (int s = static_cast<int>(spine_) - 1; s >= static_cast<int>(firstSpine_); --s) {
    if (!openChapter(static_cast<uint16_t>(s))) return false;
    if (chapterPages_ > 0) {
      page_ = chapterPages_ - 1;
      return true;
    }
  }
  // Nothing before: back to the first page of the first chapter.
  if (spine_ != firstSpine_) openChapter(firstSpine_);
  page_ = 0;
  return false;
}

bool BookReader::render(uint8_t* fb) {
  Impl& m = *impl_;
  memset(fb, 0xFF, static_cast<size_t>(kPageH / 8) * kPageW);  // panel: 792 x 528, 99 bytes/row
  if (!open_ || !m.pagesOpen || chapterPages_ == 0) return false;
  m.scratch.reset();
  Page page{};
  trace("sayfa", static_cast<int>(page_));
  if (m.pages.readPage(page_, m.scratch, &page) != BookStatus::Ok) return false;
  FrameTarget target{fb, kPageH, kPageW, kPageH / 8, FrameFormat::Mono1Sharp, FrameRotation::Portrait};
  const ZipCatalog& zip = m.book.zip();
  // Decode images only when they are small enough for the C3.
  bool imagesOk = true;
  for (uint16_t i = 0; i < page.imageCount && !m.isText; ++i) {
    const ZipEntry* e = zip.find(page.images[i].href);
    if (e == nullptr || e->uncompressedSize > kMaxImageBytes) imagesOk = false;
  }
  BookStatus st = BookStatus::Ok;
  if (imagesOk) {
    st = PageRenderer::render(page, m.fonts, *m.source, zip, m.scratch, target);
  } else {
    PageRenderer::renderText(page, m.fonts, target);
    for (uint16_t i = 0; i < page.imageCount; ++i) {
      const PageImage& im = page.images[i];
      placeholder(fb, im.x, im.y, static_cast<int16_t>(im.width), static_cast<int16_t>(im.height));
    }
  }
  m.scratch.reset();
  return st == BookStatus::Ok;
}

void BookReader::memoryReport(size_t out[7], size_t cap[7]) const {
  const Impl& m = *impl_;
  out[0] = m.bookArena.highWater();
  out[1] = m.scratch.highWater();
  out[2] = m.glyphArena.highWater();
  out[3] = m.boldGlyphArena.highWater();
  out[4] = m.indexArena.highWater();
  out[5] = m.sheetArena.highWater();
  out[6] = m.parse.highWater();
  const Arena* all[7] = {&m.bookArena, &m.scratch, &m.glyphArena, &m.boldGlyphArena, &m.indexArena, &m.sheetArena, &m.parse};
  for (int i = 0; i < 7; ++i) cap[i] = all[i]->capacity();
}

uint8_t BookReader::percent() const {
  if (!open_ || spineCount_ == 0) return 0;
  const uint32_t per = 1000;
  uint32_t v = spine_ * per;
  if (chapterPages_ > 0) v += (page_ + 1) * per / chapterPages_;
  return static_cast<uint8_t>(v * 100 / (spineCount_ * per));
}

bool BookReader::isTurkish() const { return startsWithLang(impl_->params.language, "tr"); }

const char* BookReader::chapterTitle() const {
  const Impl& m = *impl_;
  if (m.isText || !open_) return "";
  const ManifestItem* item = m.book.spineItem(spine_);
  if (item == nullptr) return "";
  for (size_t i = 0; i < m.book.tocCount(); ++i) {
    const TocEntry* e = m.book.tocEntry(i);
    if (e->href != nullptr && strcmp(e->href, item->href) == 0) return e->title;
  }
  return "";
}

}  // namespace reader
