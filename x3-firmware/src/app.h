#pragma once
#include <stdint.h>

#include "tasks.h"

// Hardware-independent app state machine. main.cpp feeds it button presses
// and time; it says what to redraw, save or do next.
enum class Button : uint8_t { Back, Confirm, Left, Right, Up, Down, Power };
enum class Screen : uint8_t { List, Focus, FocusDone };

struct Effects {
  bool redraw = false;
  bool fullRefresh = false;  // screen change: clear ghosting
  bool deepClean = false;    // white pass first: removes all ghosting
  bool save = false;         // task list changed, write the SD file
  bool sleep = false;        // enter deep sleep
};

struct App {
  static constexpr uint32_t kFocusMs = 25UL * 60UL * 1000UL;
  static constexpr uint32_t kIdleSleepMs = 3UL * 60UL * 1000UL;

  // List layout (pixels, portrait 528x792), shared with ui.cpp so scrolling
  // and drawing agree on what fits.
  static constexpr int kTopNoIndicators = 140;
  static constexpr int kTopWithIndicators = 296;
  static constexpr int kListBottom = 712;
  static constexpr int kRowH = 46;
  static constexpr int kSectionH = 40;

  TaskList tasks;
  Screen screen = Screen::List;
  int selected = 0;
  int scroll = 0;  // first visible row on the list

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

  Effects onButton(Button b, uint32_t nowMs);
  Effects onTick(uint32_t nowMs);

  uint32_t focusElapsed(uint32_t nowMs) const;
  int listTop() const;
  // Does task i start a new section heading when drawn after `first`?
  bool needsHeading(int i, int first) const;
  // How many tasks fit on screen when the list starts at task `first` (>= 1).
  int rowsFrom(int first) const;
  // Whole minutes left, rounded up (25 ... 1, 0 when finished).
  int focusMinutesLeft(uint32_t nowMs) const;

 private:
  void keepSelectionVisible();
};
