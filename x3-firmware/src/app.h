#pragma once
#include <stdint.h>

#include "library.h"
#include "tasks.h"

// Hardware-independent app state machine. main.cpp feeds it button presses
// and time; it says what to redraw, save or do next.
enum class Button : uint8_t { Back, Confirm, Left, Right, Up, Down, Power };
enum class Screen : uint8_t { List, Focus, FocusDone, Wifi, Library, Reader };

struct Effects {
  bool redraw = false;
  bool fullRefresh = false;  // screen change: clear ghosting
  bool deepClean = false;    // white pass first: removes all ghosting
  bool save = false;         // task list changed, write the SD file
  bool sleep = false;        // enter deep sleep
  bool startWifi = false;    // open the transfer hotspot + web page
  bool stopWifi = false;     // close it again
  bool openBook = false;     // open app.lib.books[app.libSelected] (main may fall back)
  bool closeBook = false;    // save the position and close the reader
  int8_t pageDelta = 0;      // +1 next page, -1 previous page
  bool syncNow = false;      // sync with GitHub over the home Wi-Fi
  bool nextFontSize = false; // reader: switch to the next text size and re-lay out
};

struct App {
  static constexpr uint32_t kFocusMs = 25UL * 60UL * 1000UL;
  static constexpr uint32_t kIdleSleepMs = 3UL * 60UL * 1000UL;
  // The transfer page is used from a phone, not the buttons: allow longer.
  static constexpr uint32_t kWifiIdleMs = 10UL * 60UL * 1000UL;
  // A page can take a while to read.
  static constexpr uint32_t kReaderIdleMs = 10UL * 60UL * 1000UL;

  // List layout (pixels, portrait 528x792), shared with ui.cpp so scrolling
  // and drawing agree on what fits.
  static constexpr int kTopNoIndicators = 140;
  static constexpr int kTopWithIndicators = 296;
  static constexpr int kListBottom = 712;
  static constexpr int kRowH = 46;
  static constexpr int kSectionH = 40;
  static constexpr int kMeasureH = 30;  // one horizontal bar under a heading

  TaskList tasks;
  Screen screen = Screen::List;
  int selected = 0;
  int scroll = 0;  // first visible row on the list

  // Bookshelf: items 0..lib.count-1 are books, lib.count is the "+ Ekle" spine.
  Library lib;
  int libSelected = 0;
  int shelfScroll = 0;  // first visible shelf
  Screen wifiReturn = Screen::List;  // where BACK on the Wi-Fi screen goes
  char libStatus[112] = "";  // e.g. "Kitap okunamadı (sayfalama: OutOfMemory)"

  int focusTask = -1;
  uint32_t focusStartMs = 0;
  uint32_t focusElapsedBeforePause = 0;
  bool focusPaused = false;
  int shownMinutes = -1;  // minutes value currently on the screen

  uint32_t lastInputMs = 0;
  bool sdAvailable = true;

  // Status line, filled by main.cpp before each render.
  char dateText[48] = "";  // "Salı, 10 Kasım"; empty when the clock is not set
  int batteryPercent = -1;  // -1 = unknown
  bool charging = false;

  // Wi-Fi transfer screen, filled by main.cpp.
  char wifiSsid[24] = "";
  char wifiPass[16] = "";
  char wifiUrl[32] = "";
  char wifiStatus[80] = "";
  char syncNote[64] = "";  // "Eşitlendi 07:02" or why it failed (status bar)
  bool syncConfigured = false;

  // Call when the web page did something, so the idle timer restarts.
  void touch(uint32_t nowMs) { lastInputMs = nowMs; }

  Effects onButton(Button b, uint32_t nowMs);
  Effects onTick(uint32_t nowMs);

  uint32_t focusElapsed(uint32_t nowMs) const;
  int listTop() const;
  // Does task i start a new section heading when drawn after `first`?
  bool needsHeading(int i, int first) const;
  // Height of the measures block drawn under task i's heading (only where its
  // section really starts, not when a scrolled list repeats the heading).
  int measuresHeight(int i) const;
  // How many tasks fit on screen when the list starts at task `first` (>= 1).
  int rowsFrom(int first) const;
  // Whole minutes left, rounded up (25 ... 1, 0 when finished).
  int focusMinutesLeft(uint32_t nowMs) const;

 private:
  void keepSelectionVisible();
  void keepShelfVisible();
  Effects onLibraryButton(Button b);
};
