// Xteink X3 — daily briefing, focus timer, bookshelf and e-book reader.
//
// The briefing comes from /brifing.txt on the SD card (created with a sample
// if missing) or from the Wi-Fi transfer page; books live in /kitaplar.
// Hardware glue only: app logic in app.cpp, drawing in ui.cpp, the reading
// engine in reader/. Button layout: docs-hardware.md.

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <BatteryMonitor.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <InputManager.h>
#include <PowerManager.h>
#include <Preferences.h>
#include <Rtc.h>
#include <SDCardManager.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <XteinkDetect.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <esp_system.h>

#include "app.h"
#include "clock.h"
#include "library.h"
#include "reader/book_reader.h"
#include "sd_storage.h"
#include "sync.h"
#include "ui.h"
#include "webpage.h"

// The reading engine (expat, stb_truetype, JPEG/PNG decode) needs more than
// Arduino's default 8 KB loop stack, as CrossPoint found on the same chip.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

namespace {

// Crash breadcrumb: survives a panic/watchdog reset (not a power cut), so the
// next boot can say what the reader was doing. Cleared after each success.
struct Breadcrumb {
  uint32_t magic;
  char step[64];
};
RTC_NOINIT_ATTR Breadcrumb crumb;
constexpr uint32_t kCrumbMagic = 0x58334252;  // "X3BR"

void setCrumb(const char* fmt, const char* a, int b) {
  crumb.magic = kCrumbMagic;
  snprintf(crumb.step, sizeof crumb.step, fmt, a, b);
}
void clearCrumb() { crumb.magic = 0; }

// After a Wi-Fi session the device restarts so the reader gets an
// unfragmented heap; this says which screen to come back to.
struct Resume {
  uint32_t magic;
  uint8_t screen;
  int16_t libSelected;
  uint8_t skipSync;  // the restart came right after a sync: do not sync again
  char note[48];     // sync status to show after the restart
};
RTC_NOINIT_ATTR Resume resume;
constexpr uint32_t kResumeMagic = 0x58335253;  // "X3RS"

constexpr int PIN_SCLK = 8;
constexpr int PIN_MOSI = 10;
constexpr int PIN_MISO = 7;
constexpr int PIN_CS = 21;
constexpr int PIN_DC = 4;
constexpr int PIN_RST = 5;
constexpr int PIN_BUSY = 6;
// GPIO13 gates the battery/SD power path on the C3 Xteink boards: HIGH keeps
// the device on, LOW (held through sleep) powers it off.
constexpr gpio_num_t PIN_POWER_LATCH = GPIO_NUM_13;

// /brifing.txt is preferred; /gorevler.txt is the older name.
constexpr const char* kBriefingPath = "/brifing.txt";
constexpr const char* kLegacyTasksPath = "/gorevler.txt";
const char* tasksPath = kBriefingPath;  // the file we loaded and write back to
constexpr const char* kSetTimePath = "/saat.txt";  // "10.11.2026 15:30": sets the RTC once
constexpr uint32_t kBatteryPollMs = 30000;
constexpr int kFastRefreshesBeforeFull = 20;
constexpr const char* kHotspotName = "GK's Xteink X3";
constexpr size_t kMaxUpload = 8000;
constexpr const char* kBooksDir = "/kitaplar";
constexpr const char* kBookStateDir = "/kitaplar/.x3";  // per-book state + page cache
constexpr int kReaderPagesBeforeClean = 12;  // e-ink ghosting cleanup while reading

EInkDisplay epd(PIN_SCLK, PIN_MOSI, PIN_CS, PIN_DC, PIN_RST, PIN_BUSY);
InputManager input;
freeink::Rtc rtc;
App app;
int fastRefreshes = 0;
bool waitingForRelease = true;  // ignore the press that woke the device
WebServer server(80);
bool wifiOn = false;
bool webRedraw = false;  // the web page changed something on screen

reader::BookReader bookReader;
SdBookSource bookSource;
SdCacheStorage bookCache;
reader::Memory readerMem{};
int openBookIndex = -1;
int readerPageTurns = 0;
bool libraryDirty = false;  // books were added/removed over Wi-Fi
SyncSettings syncSettings;
constexpr uint32_t kAutoSyncEveryMin = 30;

// Adafruit_GFX canvas that draws straight into the SDK framebuffer
// (792x528 native, row-major, MSB-first, 1 = white).
class Canvas : public Adafruit_GFX {
 public:
  Canvas() : Adafruit_GFX(EInkDisplay::X3_DISPLAY_WIDTH, EInkDisplay::X3_DISPLAY_HEIGHT) {}

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= width() || y >= height()) return;
    int16_t px = x, py = y;
    switch (getRotation()) {
      case 1: px = WIDTH - 1 - y; py = x; break;
      case 2: px = WIDTH - 1 - x; py = HEIGHT - 1 - y; break;
      case 3: px = y; py = HEIGHT - 1 - x; break;
      default: break;
    }
    uint8_t& cell = epd.getFrameBuffer()[py * epd.getDisplayWidthBytes() + (px >> 3)];
    const uint8_t mask = 0x80 >> (px & 7);
    if (color) cell &= ~mask;  // 1 = black ink
    else cell |= mask;
  }

  void fillScreen(uint16_t color) override { epd.clearScreen(color ? 0x00 : 0xFF); }
};

Canvas screen;

// One shared buffer for the briefing file (load, save, web page): static RAM
// is precious next to the reader's ~170 KB of heap arenas.
char gTextBuf[8192];

void loadTasks() {
  app.sdAvailable = SdMan.begin();
  if (!app.sdAvailable) {
    Serial.println("[x3] no SD card, using sample tasks");
    app.tasks.loadSample();
    return;
  }
  tasksPath = SdMan.exists(kBriefingPath) || !SdMan.exists(kLegacyTasksPath) ? kBriefingPath : kLegacyTasksPath;
  FsFile f = SdMan.open(tasksPath, O_RDONLY);
  if (!f) {
    Serial.println("[x3] creating sample /brifing.txt");
    SdMan.writeFile(kBriefingPath, String(kSampleTasksFile));
    app.tasks.loadSample();
    return;
  }
  const int n = f.read(gTextBuf, sizeof gTextBuf);
  f.close();
  app.tasks.parse(gTextBuf, n > 0 ? static_cast<size_t>(n) : 0);
  app.tasks.sortFinishedLast();  // after a wake or sync: finished tasks at the bottom
  Serial.printf("[x3] loaded %d tasks from %s\n", app.tasks.count, tasksPath);
}

void saveTasks() {
  if (!app.sdAvailable) return;
  app.tasks.serialize(gTextBuf, sizeof gTextBuf);
  if (!SdMan.writeFile(tasksPath, String(gTextBuf))) Serial.println("[x3] save failed");
}

bool setClock(const LocalTime& t) {
  if (!rtc.present()) return false;
  freeink::Rtc::DateTime dt;
  dt.year = t.year;
  dt.month = t.month;
  dt.day = t.day;
  dt.hour = t.hour;
  dt.minute = t.minute;
  dt.second = t.second;
  dt.weekday = static_cast<uint8_t>(weekdayOf(t.year, t.month, t.day));
  if (!rtc.set(dt)) return false;
  Serial.printf("[x3] clock set to %04u-%02u-%02u %02u:%02u\n", t.year, t.month, t.day, t.hour, t.minute);
  return true;
}

// If /saat.txt exists, set the RTC from it and delete it.
void applySetTimeFile() {
  if (!app.sdAvailable || !rtc.present()) return;
  FsFile f = SdMan.open(kSetTimePath, O_RDONLY);
  if (!f) return;
  char buf[64];
  const int n = f.read(buf, sizeof buf);
  f.close();
  LocalTime t;
  if (n > 0 && parseSetTime(buf, static_cast<size_t>(n), t) && setClock(t)) {
    SdMan.remove(kSetTimePath);
    return;
  }
  Serial.println("[x3] /saat.txt not understood, left in place");
}

// Refreshes the date and battery fields shown in the header.
void updateStatus() {
  app.dateText[0] = '\0';
  freeink::Rtc::DateTime dt;
  if (rtc.present() && rtc.now(dt)) {
    LocalTime t;
    t.year = dt.year;
    t.month = dt.month;
    t.day = dt.day;
    t.hour = dt.hour;
    t.minute = dt.minute;
    t.second = dt.second;
    formatTurkishDate(t, app.dateText, sizeof app.dateText);
  }

  static uint32_t lastBatteryMs = 0;
  static bool batteryRead = false;
  const uint32_t now = millis();
  if (!batteryRead || now - lastBatteryMs >= kBatteryPollMs) {
    static const BatteryMonitor battery;
    uint16_t percent = 0;
    if (battery.readPercentageChecked(percent)) app.batteryPercent = percent;
    app.charging = battery.isCharging();
    lastBatteryMs = now;
    batteryRead = true;
  }
}

// Drives every pixel to white, then the next frame paints on a clean panel.
// The X3 driver's FULL mode assumes a white previous frame, so it can leave
// ghosts; HALF drives each pixel to its target regardless of the old image.
void cleanPanel() {
  epd.clearScreen(0xFF);
  epd.displayBuffer(EInkDisplay::HALF_REFRESH);
  fastRefreshes = 0;
}

// --- Bookshelf and reader ------------------------------------------------------

bool currentTime(LocalTime& t) {
  freeink::Rtc::DateTime dt;
  if (!rtc.present() || !rtc.now(dt)) return false;
  t.year = dt.year;
  t.month = dt.month;
  t.day = dt.day;
  t.hour = dt.hour;
  t.minute = dt.minute;
  t.second = dt.second;
  return isPlausible(t);
}

void bookDir(const BookInfo& b, char* out, size_t n) { snprintf(out, n, "%s/%s", kBookStateDir, b.id); }

void saveBookState(const BookInfo& b) {
  if (!app.sdAvailable) return;
  char dir[64], path[96], buf[512];
  bookDir(b, dir, sizeof dir);
  SdMan.ensureDirectoryExists(dir);
  snprintf(path, sizeof path, "%s/durum.txt", dir);
  serializeBookState(b, buf, sizeof buf);
  SdMan.writeFile(path, String(buf));
}

// Lists /kitaplar (EPUB and TXT) with each book's saved reading state.
void scanLibrary() {
  Library& lib = app.lib;
  lib.count = 0;
  libraryDirty = false;
  if (!app.sdAvailable) return;
  SdMan.ensureDirectoryExists(kBooksDir);
  FsFile dir = SdMan.open(kBooksDir, O_RDONLY);
  if (!dir) return;
  FsFile f;
  char name[96];
  while (lib.count < Library::kMax && f.openNext(&dir, O_RDONLY)) {
    const bool isDir = f.isDir();
    f.getName(name, sizeof name);
    const uint32_t size = static_cast<uint32_t>(f.fileSize());
    f.close();
    if (isDir) continue;
    BookInfo& b = lib.books[lib.count];
    if (!bookFromFileName(name, size, b)) continue;
    char path[96];
    snprintf(path, sizeof path, "%s/%s/durum.txt", kBookStateDir, b.id);
    FsFile st = SdMan.open(path, O_RDONLY);
    if (st) {
      char buf[512];
      const int n = st.read(buf, sizeof buf);
      st.close();
      if (n > 0) parseBookState(buf, static_cast<size_t>(n), b);
    }
    ++lib.count;
  }
  dir.close();
  lib.sortByTitle();
  if (app.libSelected > lib.count) app.libSelected = lib.count;
  Serial.printf("[x3] library: %d books\n", lib.count);
}

void freeReaderMemory() {
  uint8_t** bufs[] = {&readerMem.big, &readerMem.book, &readerMem.glyph, &readerMem.bold, &readerMem.index, &readerMem.sheet};
  for (uint8_t** b : bufs) {
    free(*b);
    *b = nullptr;
  }
  readerMem = reader::Memory{};
}

char memoryNote[64] = "";  // what did not fit, shown on the shelf

bool allocReaderMemory() {
  const size_t freeBytes = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const unsigned largestKb = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024;
  Serial.printf("[x3] reader alloc: heap free %u KB, largest block %u KB\n", static_cast<unsigned>(freeBytes / 1024),
                largestKb);
  // The two layout arenas share the largest block (measured: 111 KB).
  readerMem.big = static_cast<uint8_t*>(heap_caps_malloc(reader::kBigBlock, MALLOC_CAP_8BIT));
  if (readerMem.big == nullptr) {
    snprintf(memoryNote, sizeof memoryNote, "%u KB blok yok (en büyük %u KB)",
             static_cast<unsigned>(reader::kBigBlock / 1024), largestKb);
    return false;
  }

  // Plan the small arenas inside a budget that leaves kHeapReserve free for
  // expat and the rasterizer: every minimum first, then extras by priority.
  struct Item {
    const char* name;
    const reader::ArenaSpec* spec;
    uint8_t** ptr;
    size_t* size;
  } items[] = {
      {"katalog", &reader::kBookArena, &readerMem.book, &readerMem.bookSize},
      {"font", &reader::kGlyphArena, &readerMem.glyph, &readerMem.glyphSize},
      {"dizin", &reader::kIndexArena, &readerMem.index, &readerMem.indexSize},
      {"kalın font", &reader::kBoldGlyphArena, &readerMem.bold, &readerMem.boldSize},
      {"css", &reader::kSheetArena, &readerMem.sheet, &readerMem.sheetSize},
  };
  const size_t afterBig = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t budget = afterBig > reader::kHeapReserve ? afterBig - reader::kHeapReserve : 0;
  size_t plan[5];
  for (int i = 0; i < 5; ++i) {
    plan[i] = items[i].spec->minimum;
    if (plan[i] > budget) {
      snprintf(memoryNote, sizeof memoryNote, "%s için yer yok (boş %u KB)", items[i].name,
               static_cast<unsigned>(afterBig / 1024));
      freeReaderMemory();
      return false;
    }
    budget -= plan[i];
  }
  // Extras: glyph cache (page speed), catalog, index, real bold, CSS.
  const int priority[] = {1, 0, 2, 3, 4};
  for (int i : priority) {
    const size_t want = items[i].spec->preferred - plan[i];
    // An optional arena is all-or-nothing above a useful size (6 KB bold).
    if (items[i].spec->minimum == 0 && want > budget && budget < 6 * 1024) continue;
    const size_t extra = want < budget ? want : budget;
    plan[i] += extra;
    budget -= extra;
  }
  for (int i = 0; i < 5; ++i) {
    if (plan[i] == 0) continue;
    // Fragmentation: step down 1 KB at a time (to the minimum) if needed.
    for (size_t size = plan[i];; size -= 1024) {
      *items[i].ptr = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_8BIT));
      if (*items[i].ptr != nullptr) {
        *items[i].size = size;
        break;
      }
      if (size < items[i].spec->minimum + 1024 || size <= 1024) break;
    }
    if (*items[i].ptr == nullptr && items[i].spec->minimum > 0) {
      snprintf(memoryNote, sizeof memoryNote, "%s için %u KB yok (en büyük %u KB)", items[i].name,
               static_cast<unsigned>(items[i].spec->minimum / 1024),
               static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024));
      freeReaderMemory();
      return false;
    }
  }
  Serial.printf("[x3] reader arenas: book %u, glyph %u, bold %u, index %u, css %u; heap left %u KB\n",
                static_cast<unsigned>(readerMem.bookSize), static_cast<unsigned>(readerMem.glyphSize),
                static_cast<unsigned>(readerMem.boldSize), static_cast<unsigned>(readerMem.indexSize),
                static_cast<unsigned>(readerMem.sheetSize),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024));
  return true;
}

void showReaderMessage(const char* title, const char* message) {
  updateStatus();
  ui::drawReaderMessage(screen, app, title, message);
  epd.displayBuffer(EInkDisplay::FAST_REFRESH);
}

// Saves the reading position into the book's state (and its file).
void recordProgress() {
  if (openBookIndex < 0 || !bookReader.isOpen()) return;
  BookInfo& b = app.lib.books[openBookIndex];
  const reader::Position pos = bookReader.position();
  b.spine = pos.spine;
  b.charOffset = pos.charOffset;
  b.percent = bookReader.percent();
  LocalTime t;
  if (currentTime(t)) {
    b.lastRead = static_cast<uint64_t>(t.year) * 100000000ull + t.month * 1000000ull + t.day * 10000ull +
                 t.hour * 100ull + t.minute;
  } else {
    uint64_t newest = 0;  // no clock: keep the order of reading anyway
    for (int i = 0; i < app.lib.count; ++i) newest = app.lib.books[i].lastRead > newest ? app.lib.books[i].lastRead : newest;
    if (b.lastRead != newest || b.lastRead == 0) b.lastRead = newest + 1;
  }
  saveBookState(b);
}

void closeBook() {
  if (openBookIndex >= 0) recordProgress();
  bookReader.close();
  bookSource.close();
  bookCache.closeAll();
  freeReaderMemory();
  openBookIndex = -1;
}

void readerTrace(const char* step, int value) {
  const char* what = strcmp(step, "katalog") == 0 ? "katalog okunurken"
                     : strcmp(step, "bolum") == 0  ? "bölüm %d sayfalanırken"
                                                   : "sayfa %d çizilirken";
  crumb.magic = kCrumbMagic;
  snprintf(crumb.step, sizeof crumb.step, what, value + 1);
  Serial.printf("[x3] reader: %s %d (heap %u, largest %u)\n", step, value, heap_caps_get_free_size(MALLOC_CAP_8BIT),
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

// Opens app.lib.books[app.libSelected]; on failure returns to the shelf.
// Reading typography lives in NVS ("x3read"), applied before each open.
void loadTypography() {
  reader::Typography t;
  Preferences p;
  if (p.begin("x3read", true)) {
    t.sizePx = p.getUChar("size", t.sizePx);
    t.lineSpacingPct = p.getUChar("space", t.lineSpacingPct);
    t.marginX = p.getUChar("margin", t.marginX);
    p.end();
  }
  reader::setTypography(t);
}

void saveTypography(const reader::Typography& t) {
  reader::setTypography(t);
  Preferences p;
  if (!p.begin("x3read", false)) return;
  const reader::Typography& s = reader::typography();
  p.putUChar("size", s.sizePx);
  p.putUChar("space", s.lineSpacingPct);
  p.putUChar("margin", s.marginX);
  p.end();
}

bool openSelectedBook();

// CONFIRM in the reader: next text size, same place in the book.
void cycleFontSize() {
  if (openBookIndex < 0) return;
  const int index = openBookIndex;
  closeBook();  // saves the position (a character offset: layout independent)
  reader::Typography t = reader::typography();
  t.sizePx = reader::nextSize(t.sizePx);
  saveTypography(t);
  app.libSelected = index;
  char msg[48];
  snprintf(msg, sizeof msg, "Yazı %u px · sayfalanıyor...", t.sizePx);
  showReaderMessage(app.lib.books[index].title, msg);
  if (!openSelectedBook()) app.screen = Screen::Library;
}

bool openSelectedBook() {
  const int index = app.libSelected;
  if (index < 0 || index >= app.lib.count) return false;
  BookInfo& b = app.lib.books[index];
  showReaderMessage(b.title, "Açılıyor...");
  const char* error = nullptr;
  char path[128];
  snprintf(path, sizeof path, "%s/%s", kBooksDir, b.file);
  char dir[64];
  bookDir(b, dir, sizeof dir);
  setCrumb("%s açılırken", b.file, 0);
  if (!allocReaderMemory()) {
    error = "Bellek yetmedi";  // memoryNote has the numbers
  } else if (!bookSource.open(path)) {
    error = "Dosya açılamadı";
  } else {
    bookCache.setDir(dir);
    switch (bookReader.open(bookSource, bookCache, b.isText, readerMem)) {
      case reader::OpenResult::Ok: break;
      case reader::OpenResult::Encrypted: error = "DRM korumalı kitap açılamaz"; break;
      case reader::OpenResult::TooBig: error = "Kitap bu cihaz için çok büyük"; break;
      default: error = "Kitap okunamadı"; break;
    }
  }
  if (error != nullptr) {
    Serial.printf("[x3] open %s failed: %s\n", b.file, error);
    bookReader.close();
    bookSource.close();
    bookCache.closeAll();
    freeReaderMemory();
    if (strcmp(error, "Bellek yetmedi") == 0) {
      snprintf(app.libStatus, sizeof app.libStatus, "Bellek: %s", memoryNote);
    } else if (reader::lastError()[0]) {
      snprintf(app.libStatus, sizeof app.libStatus, "%s (%s)", error, reader::lastError());
    } else {
      snprintf(app.libStatus, sizeof app.libStatus, "%s", error);
    }
    app.screen = Screen::Library;
    clearCrumb();
    return false;
  }
  openBookIndex = index;
  readerPageTurns = 0;
  // First open: take the title/author from the book and stamp the start date.
  if (bookReader.title()[0]) snprintf(b.title, sizeof b.title, "%s", bookReader.title());
  if (bookReader.author()[0]) snprintf(b.author, sizeof b.author, "%s", bookReader.author());
  LocalTime t;
  if (!b.started.known() && currentTime(t)) b.started = DateStamp{t.year, t.month, t.day};
  if (b.lastRead != 0) {
    reader::Position pos;
    pos.spine = b.spine;
    pos.charOffset = b.charOffset;
    bookReader.seek(pos);
  }
  recordProgress();
  return true;
}

void renderReaderPage() {
  bookReader.render(epd.getFrameBuffer());
  clearCrumb();  // the page is drawn: whatever was risky succeeded
  ui::drawReaderFooter(screen, bookReader.chapterTitle(), bookReader.percent(), bookReader.page() + 1,
                       bookReader.chapterPages(), bookReader.isTurkish());
}

// screenChange -> HALF refresh; deepClean -> white pass first (no ghosts).
void render(bool screenChange, bool deepClean = false) {
  if (deepClean) cleanPanel();
  updateStatus();
  switch (app.screen) {
    case Screen::List: ui::drawList(screen, app, false); break;
    case Screen::Focus: ui::drawFocus(screen, app, millis()); break;
    case Screen::FocusDone: ui::drawFocusDone(screen, app); break;
    case Screen::Wifi: ui::drawWifi(screen, app); break;
    case Screen::Library: ui::drawLibrary(screen, app); break;
    case Screen::Reader:
      renderReaderPage();
      // Page turns use FAST refresh; a clean HALF every few pages keeps text crisp.
      if (!screenChange && ++readerPageTurns < kReaderPagesBeforeClean) {
        epd.displayBuffer(EInkDisplay::FAST_REFRESH);
      } else {
        readerPageTurns = 0;
        epd.displayBuffer(EInkDisplay::HALF_REFRESH);
      }
      return;
  }
  if (screenChange || ++fastRefreshes >= kFastRefreshesBeforeFull) {
    fastRefreshes = 0;
    epd.displayBuffer(EInkDisplay::HALF_REFRESH);
  } else {
    epd.displayBuffer(EInkDisplay::FAST_REFRESH);
  }
}

// --- Wi-Fi transfer: hotspot + web page --------------------------------------

void handlePage() { server.send_P(200, "text/html; charset=utf-8", kTransferPage); }

void handleGetBriefing() {
  app.tasks.serialize(gTextBuf, sizeof gTextBuf);
  server.send(200, "text/plain; charset=utf-8", gTextBuf);
}

void handlePostBriefing() {
  const String& body = server.arg("plain");
  if (body.length() == 0 || body.length() > kMaxUpload) {
    server.send(413, "text/plain; charset=utf-8", "Dosya boş ya da çok büyük (en çok 8 KB).");
    return;
  }
  app.tasks.parse(body.c_str(), body.length());
  app.selected = 0;
  app.scroll = 0;
  tasksPath = kBriefingPath;
  saveTasks();
  char msg[96];
  snprintf(msg, sizeof msg, "Brifing alındı · %d görev", app.tasks.count);
  snprintf(app.wifiStatus, sizeof app.wifiStatus, "%s", msg);
  app.touch(millis());
  webRedraw = true;
  if (!app.sdAvailable) {
    snprintf(msg, sizeof msg, "Ekrana yüklendi (%d görev), SD kart yok: kapanınca silinir.", app.tasks.count);
  } else {
    snprintf(msg, sizeof msg, "Kaydedildi: %d görev. Cihaz ekranı yenileniyor.", app.tasks.count);
  }
  server.send(200, "text/plain; charset=utf-8", msg);
}

void handlePostTime() {
  const String& body = server.arg("plain");
  LocalTime t;
  if (!parseSetTime(body.c_str(), body.length(), t)) {
    server.send(400, "text/plain; charset=utf-8", "Saat anlaşılamadı.");
    return;
  }
  if (!setClock(t)) {
    server.send(500, "text/plain; charset=utf-8", "Saat çipine yazılamadı.");
    return;
  }
  app.touch(millis());
  webRedraw = true;  // the header date may change
  char msg[64];
  snprintf(msg, sizeof msg, "Cihaz saati ayarlandı: %02u.%02u.%04u %02u:%02u", t.day, t.month, t.year, t.hour,
           t.minute);
  server.send(200, "text/plain; charset=utf-8", msg);
}

// Books over Wi-Fi: list, upload (multipart, streamed to the card), delete.
void handleListBooks() {
  if (libraryDirty) scanLibrary();
  String out;
  for (int i = 0; i < app.lib.count; ++i) {
    const BookInfo& b = app.lib.books[i];
    char line[256];
    char meta[48];
    if (b.lastRead && !b.finished) snprintf(meta, sizeof meta, "%lu KB · %%%u", static_cast<unsigned long>(b.size / 1024), b.percent);
    else snprintf(meta, sizeof meta, "%lu KB", static_cast<unsigned long>(b.size / 1024));
    snprintf(line, sizeof line, "%s\t%s\t%s\n", b.file, b.title, meta);
    out += line;
  }
  server.send(200, "text/plain; charset=utf-8", out);
}

// Keeps the base name and replaces characters FAT does not allow.
void safeBookName(const String& in, char* out, size_t n) {
  const int slash = max(in.lastIndexOf('/'), in.lastIndexOf('\\'));
  String name = in.substring(slash + 1);
  size_t o = 0;
  for (size_t i = 0; i < name.length() && o + 1 < n; ++i) {
    const char c = name[i];
    out[o++] = (strchr("\\/:*?\"<>|", c) != nullptr || static_cast<unsigned char>(c) < 0x20) ? '_' : c;
  }
  out[o] = '\0';
}

FsFile uploadFile;
char uploadName[96];
bool uploadOk = false;

void handleUploadData() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    uploadOk = false;
    safeBookName(up.filename, uploadName, sizeof uploadName);
    BookInfo probe;
    if (!app.sdAvailable || !bookFromFileName(uploadName, 0, probe)) return;  // not .epub/.txt
    SdMan.ensureDirectoryExists(kBooksDir);
    char path[128];
    snprintf(path, sizeof path, "%s/%s", kBooksDir, uploadName);
    uploadFile = SdMan.open(path, O_WRONLY | O_CREAT | O_TRUNC);
    uploadOk = static_cast<bool>(uploadFile);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (uploadOk && uploadFile.write(up.buf, up.currentSize) != up.currentSize) uploadOk = false;
  } else if (up.status == UPLOAD_FILE_END || up.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    if (up.status == UPLOAD_FILE_ABORTED || !uploadOk) {
      char path[128];
      snprintf(path, sizeof path, "%s/%s", kBooksDir, uploadName);
      if (uploadName[0]) SdMan.remove(path);
      uploadOk = false;
    }
  }
  app.touch(millis());
}

void handleUploadDone() {
  char msg[160];
  if (!app.sdAvailable) {
    snprintf(msg, sizeof msg, "SD kart yok: kitap kaydedilemedi.");
  } else if (!uploadOk) {
    snprintf(msg, sizeof msg, "%s eklenemedi (yalnız .epub ve .txt).", uploadName);
  } else {
    snprintf(msg, sizeof msg, "Rafa eklendi: %s", uploadName);
    snprintf(app.wifiStatus, sizeof app.wifiStatus, "Kitap eklendi");
    libraryDirty = true;
    webRedraw = true;
  }
  server.send(uploadOk ? 200 : 400, "text/plain; charset=utf-8", msg);
}

void handleDeleteBook() {
  char name[96];
  safeBookName(server.arg("plain"), name, sizeof name);
  BookInfo b;
  if (!bookFromFileName(name, 0, b)) {
    server.send(400, "text/plain; charset=utf-8", "Geçersiz ad.");
    return;
  }
  char path[128], dir[64];
  snprintf(path, sizeof path, "%s/%s", kBooksDir, name);
  bookDir(b, dir, sizeof dir);
  const bool ok = SdMan.remove(path);
  SdMan.removeDir(dir);  // reading state + page cache
  libraryDirty = true;
  app.touch(millis());
  if (ok) {
    snprintf(app.wifiStatus, sizeof app.wifiStatus, "Kitap silindi");
    webRedraw = true;
  }
  server.send(ok ? 200 : 404, "text/plain; charset=utf-8", ok ? "Silindi." : "Bulunamadı.");
}

void handleGetSettings();
void handlePostSettings();

void startWifi() {
  if (wifiOn) return;
  // A fresh 8-digit password each time the hotspot opens.
  snprintf(app.wifiPass, sizeof app.wifiPass, "%08lu", static_cast<unsigned long>(esp_random() % 100000000UL));
  snprintf(app.wifiSsid, sizeof app.wifiSsid, "%s", kHotspotName);
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(app.wifiSsid, app.wifiPass)) {
    snprintf(app.wifiStatus, sizeof app.wifiStatus, "Wi-Fi açılamadı");
    return;
  }
  snprintf(app.wifiUrl, sizeof app.wifiUrl, "http://%s", WiFi.softAPIP().toString().c_str());
  server.on("/", HTTP_GET, handlePage);
  server.on("/brifing.txt", HTTP_GET, handleGetBriefing);
  server.on("/brifing", HTTP_POST, handlePostBriefing);
  server.on("/saat", HTTP_POST, handlePostTime);
  server.on("/kitaplar", HTTP_GET, handleListBooks);
  server.on("/kitap", HTTP_POST, handleUploadDone, handleUploadData);
  server.on("/kitap-sil", HTTP_POST, handleDeleteBook);
  server.on("/ayarlar", HTTP_GET, handleGetSettings);
  server.on("/ayarlar", HTTP_POST, handlePostSettings);
  server.on("/okuma", HTTP_GET, [] {
    const reader::Typography& t = reader::typography();
    char line[32];
    snprintf(line, sizeof line, "%u\t%u\t%u", t.sizePx, t.lineSpacingPct, t.marginX);
    server.send(200, "text/plain; charset=utf-8", line);
  });
  server.on("/okuma", HTTP_POST, [] {
    reader::Typography t = reader::typography();
    if (server.hasArg("size")) t.sizePx = static_cast<uint8_t>(server.arg("size").toInt());
    if (server.hasArg("space")) t.lineSpacingPct = static_cast<uint8_t>(server.arg("space").toInt());
    if (server.hasArg("margin")) t.marginX = static_cast<uint8_t>(server.arg("margin").toInt());
    saveTypography(t);
    server.send(200, "text/plain; charset=utf-8", "Kaydedildi: bir sonraki açılışta geçerli");
  });
  server.onNotFound([] { server.send(404, "text/plain; charset=utf-8", "Yok"); });
  server.begin();
  wifiOn = true;
  Serial.printf("[x3] hotspot %s up at %s\n", app.wifiSsid, app.wifiUrl);
}

void stopWifi() {
  if (!wifiOn) return;
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiOn = false;
  if (libraryDirty) scanLibrary();
  Serial.println("[x3] hotspot off");
}

// Wi-Fi leaves the heap fragmented and the reader needs large blocks:
// restart cleanly and come back to the same screen.
[[noreturn]] void restartInto(Screen screenAfter, bool afterSync, const char* note) {
  resume.magic = kResumeMagic;
  resume.screen = static_cast<uint8_t>(screenAfter);
  resume.libSelected = static_cast<int16_t>(app.libSelected);
  resume.skipSync = afterSync ? 1 : 0;
  snprintf(resume.note, sizeof resume.note, "%s", note != nullptr ? note : "");
  Serial.println("[x3] restarting after Wi-Fi");
  Serial.flush();
  esp_restart();
}

// GitHub sync over the home Wi-Fi, then a clean restart into `screenAfter`.
[[noreturn]] void syncAndRestart(Screen screenAfter) {
  snprintf(app.syncNote, sizeof app.syncNote, "Eşitleniyor...");
  if (app.screen == Screen::List) render(false);
  if (openBookIndex >= 0) closeBook();  // save the reading position before it goes up
  const SyncOutcome out = runSync(syncSettings, app.tasks, app.sdAvailable ? &app.lib : nullptr, setClock);
  Serial.printf("[x3] sync: ok=%d changed=%d clock=%d books +%d -%d !%d %s\n", out.ok, out.changed, out.clockSet,
                out.booksAdded, out.booksRemoved, out.booksFailed, out.message);
  if (out.changed) {
    tasksPath = kBriefingPath;
    saveTasks();
  }
  restartInto(screenAfter, true, out.message);
}

bool autoSyncDue() {
  if (!syncSettings.configured()) return false;
  const uint32_t last = lastSyncMinutes();
  LocalTime t;
  if (last == 0 || !currentTime(t)) return true;
  const uint32_t nowMin = minutesSince2000(t);
  return nowMin < last || nowMin - last >= kAutoSyncEveryMin;
}

// --- Sync settings on the transfer page ----------------------------------------

void handleGetSettings() {
  char hint[16] = "";
  const size_t n = strlen(syncSettings.token);
  if (n >= 4) snprintf(hint, sizeof hint, "...%s", syncSettings.token + n - 4);
  char line[256];
  snprintf(line, sizeof line, "%s\t%s\t%s\t%s\t%d\t%s\t%d", syncSettings.ssid, syncSettings.repo, syncSettings.branch,
           hint, syncSettings.pass[0] ? 1 : 0, syncSettings.ssid2, syncSettings.pass2[0] ? 1 : 0);
  server.send(200, "text/plain; charset=utf-8", line);  // never the password or the token
}

void handlePostSettings() {
  auto take = [](const char* name, char* dst, size_t n, bool keepIfEmpty) {
    if (!server.hasArg(name)) return;
    const String v = server.arg(name);
    if (v.length() == 0 && keepIfEmpty) return;
    snprintf(dst, n, "%s", v.c_str());
  };
  take("ssid", syncSettings.ssid, sizeof syncSettings.ssid, false);
  take("pass", syncSettings.pass, sizeof syncSettings.pass, true);
  take("ssid2", syncSettings.ssid2, sizeof syncSettings.ssid2, false);
  take("pass2", syncSettings.pass2, sizeof syncSettings.pass2, true);
  take("token", syncSettings.token, sizeof syncSettings.token, true);
  take("repo", syncSettings.repo, sizeof syncSettings.repo, false);
  take("branch", syncSettings.branch, sizeof syncSettings.branch, false);
  if (server.hasArg("sil") && server.arg("sil") == "1") syncSettings = SyncSettings{};
  saveSyncSettings(syncSettings);
  app.syncConfigured = syncSettings.configured();
  app.touch(millis());
  webRedraw = true;
  server.send(200, "text/plain; charset=utf-8",
              app.syncConfigured ? "Kaydedildi. Cihazda GERİ ile çıkınca ya da TAMAM ile hemen eşitlenir."
                                 : "Kaydedildi (eşitleme için Wi-Fi adı ve anahtar gerekli).");
}

[[noreturn]] void goToSleep() {
  Serial.println("[x3] sleep");
  stopWifi();
  closeBook();
  if (app.screen == Screen::Wifi || app.screen == Screen::Reader) app.screen = Screen::Library;
  // This image stays on the panel while off, so clean first: no ghosts.
  cleanPanel();
  updateStatus();
  ui::drawLendingCard(screen, app);
  epd.displayBuffer(EInkDisplay::HALF_REFRESH);
  epd.deepSleep();
  SdMan.shutdown();
  // Same order as CrossPoint: cut the battery path (no-op on USB power), then
  // deep sleep until the power button is pressed.
  gpio_hold_dis(PIN_POWER_LATCH);
  gpio_set_direction(PIN_POWER_LATCH, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_POWER_LATCH, 0);
  gpio_hold_en(PIN_POWER_LATCH);
  freeink::PowerManager::powerDownRailsForSleep();
  freeink::PowerManager::deepSleepUntilPowerButton();
}

bool anyButtonDown() {
  for (uint8_t b = InputManager::BTN_BACK; b <= InputManager::BTN_POWER; ++b) {
    if (input.isPressed(b)) return true;
  }
  return false;
}

}  // namespace

void setup() {
  // Keep power on before anything else (a previous sleep may have held it LOW).
  gpio_hold_dis(PIN_POWER_LATCH);
  pinMode(PIN_POWER_LATCH, OUTPUT);
  digitalWrite(PIN_POWER_LATCH, HIGH);

  Serial.begin(115200);
  delay(100);
  Serial.println("[x3] boot");

  BoardConfig::selectDevice(BoardConfig::Board::XteinkX3);
  freeink::applyXteinkDisplayController();
  if (BoardConfig::ACTIVE.displayController == BoardConfig::DisplayController::UC8279) {
    BoardConfig::selectDevice(BoardConfig::Board::XteinkX3Uc8279);
  }

  SPI.begin(PIN_SCLK, PIN_MISO, PIN_MOSI, PIN_CS);
  input.begin();

  epd.setDisplayX3();
  epd.begin();
  epd.requestResync();
  screen.setRotation(3);  // portrait 528x792, upright on the X3

  // Did the last session crash inside the reader?
  const esp_reset_reason_t reason = esp_reset_reason();
  const bool crashed = reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT ||
                       reason == ESP_RST_WDT || reason == ESP_RST_BROWNOUT;
  if (crashed && crumb.magic == kCrumbMagic) {
    crumb.step[sizeof crumb.step - 1] = '\0';
    snprintf(app.libStatus, sizeof app.libStatus, "Çöktü: %s", crumb.step);
    Serial.printf("[x3] previous run crashed (reason %d) %s\n", static_cast<int>(reason), crumb.step);
  } else if (crashed) {
    snprintf(app.libStatus, sizeof app.libStatus, "Yeniden başladı (hata %d)", static_cast<int>(reason));
  }
  clearCrumb();
  reader::setTrace(readerTrace);
  const bool resumed = reason == ESP_RST_SW && resume.magic == kResumeMagic;
  Resume resumeState = resume;
  resume.magic = 0;

  rtc.begin();
  loadSyncSettings(syncSettings);
  loadTypography();
  app.syncConfigured = syncSettings.configured();
  loadTasks();
  applySetTimeFile();
  scanLibrary();
  if (resumed) {
    const Screen target = static_cast<Screen>(resumeState.screen);
    if (target == Screen::Library || target == Screen::List) app.screen = target;
    if (resumeState.libSelected >= 0 && resumeState.libSelected <= app.lib.count) {
      app.libSelected = resumeState.libSelected;
    }
    waitingForRelease = false;  // no wake press to swallow after a restart
    resumeState.note[sizeof resumeState.note - 1] = '\0';
    snprintf(app.syncNote, sizeof app.syncNote, "%s", resumeState.note);
  }
  app.lastInputMs = millis();
  // After a restart the panel already shows a clean screen: skip the white pass.
  render(true, /*deepClean=*/!resumed);
  // Pull the latest briefing on wake when it is due (not right after a sync).
  if (!(resumed && resumeState.skipSync) && autoSyncDue()) syncAndRestart(app.screen);
}

void loop() {
  input.update();
  const uint32_t now = millis();

  if (waitingForRelease) {
    if (!anyButtonDown()) waitingForRelease = false;
  } else {
    for (uint8_t b = InputManager::BTN_BACK; b <= InputManager::BTN_POWER; ++b) {
      if (!input.wasPressed(b)) continue;
      const Screen before = app.screen;
      const Effects fx = app.onButton(static_cast<Button>(b), now);
      if (fx.closeBook) closeBook();
      if (fx.sleep) goToSleep();
      if (fx.stopWifi) {
        stopWifi();
        if (fx.syncNow) syncAndRestart(app.screen);
        // With new settings saved, sync on the way out (the restart is needed anyway).
        if (syncSettings.configured() && autoSyncDue()) syncAndRestart(app.screen);
        restartInto(app.screen, false, nullptr);
      }
      if (fx.syncNow && !fx.stopWifi) syncAndRestart(app.screen);  // SOL on the list
      if (fx.startWifi) startWifi();
      if (fx.save) saveTasks();
      if (libraryDirty && app.screen == Screen::Library) scanLibrary();
      if (fx.openBook && !openSelectedBook()) {
        render(true);  // back on the shelf with the error
        break;
      }
      if (fx.nextFontSize) cycleFontSize();
      if (fx.pageDelta != 0) {
        const bool moved = fx.pageDelta > 0 ? bookReader.nextPage() : bookReader.prevPage();
        if (!moved) break;  // first/last page: nothing to redraw
        if (fx.pageDelta > 0 && bookReader.percent() >= 100 && openBookIndex >= 0) {
          app.lib.books[openBookIndex].finished = true;
        }
      }
      if (fx.redraw) render(fx.fullRefresh || app.screen != before, fx.deepClean);
      break;
    }
  }

  static uint32_t lastTick = 0;
  if (now - lastTick >= 500) {
    lastTick = now;
    const Effects fx = app.onTick(now);
    if (fx.closeBook) closeBook();
    if (fx.sleep) goToSleep();
    if (fx.redraw) render(fx.fullRefresh);
  }

  if (wifiOn) {
    server.handleClient();
    if (webRedraw) {
      webRedraw = false;
      render(false);
    }
    delay(2);
  } else {
    delay(10);
  }
}
