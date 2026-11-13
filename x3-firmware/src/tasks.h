#pragma once
#include <stddef.h>
#include <stdint.h>

// The day's briefing, stored on the SD card as /brifing.txt (or the older
// /gorevler.txt), UTF-8. Line types:
//   ust: Gökçen'in günlük brifingi        eyebrow above the title
//   baslik: Sabah Brifingi                 title (default "Bugün")
//   alt: Sprint haftası H6 · sezon 7/13    shown after the date
//   gosterge: Hava | 2,5 °C | kar sağanağı · gün 3,3 / -1,3 °C   (up to 3)
//   ## Günün akışı                         section heading (up to 6)
//   [x] 09:15-10:00 | Ekip toplantısı      done task with a time column
//   [ ] Alt katı temizle                   open task, no time
//   [-] 13:30-14:00 | İptal edilen iş      cancelled (also "[~]")
//   olcu: Adım | 3728 | 10000 | adım       a measure drawn as a horizontal bar
//                                          under its section heading (value "-"
//                                          = no data; up to 8). A section
//                                          with measures but no tasks gets
//                                          a placeholder row at the end
//   plain line                             open task
//   # comment / blank                      ignored
struct Task {
  static constexpr size_t kMaxText = 120;
  static constexpr size_t kMaxTime = 16;
  char text[kMaxText];
  char time[kMaxTime];  // "" when the line has no "time | text" part
  int8_t section;       // index into TaskList::sections, -1 = none
  bool done;
  bool cancelled;       // won't be done today; never both done and cancelled
  bool placeholder;     // not a task: stands in for a section that only has
                        // measures, so its heading and bars are drawn
};

struct Indicator {
  char label[32];
  char value[32];
  char detail[96];
};

struct Measure {
  char label[24];
  char unit[12];
  int32_t value;  // -1 = no data
  int32_t goal;   // > 0
  int8_t section; // -1 = before any heading
};

struct TaskList {
  static constexpr int kMax = 30;
  static constexpr int kMaxIndicators = 3;
  static constexpr int kMaxSections = 6;
  static constexpr int kMaxMeasures = 8;

  char eyebrow[64] = "";
  char title[48] = "";
  char subtitle[96] = "";
  Indicator indicators[kMaxIndicators];
  int indicatorCount = 0;
  char sections[kMaxSections][40];
  int sectionCount = 0;
  Task items[kMax];
  int count = 0;
  Measure measures[kMaxMeasures];
  int measureCount = 0;
  // How many measures belong to section `s`.
  int measuresIn(int s) const;

  int doneCount() const;
  // Tasks that still count (not cancelled): the "x / n" total.
  int activeCount() const;
  // Within each section: open tasks first, then done, then cancelled; the
  // order inside each group is kept. Sections stay where they are.
  void sortFinishedLast();
  // Parses file content (need not be NUL-terminated). Extras are dropped.
  // Keeps the file's order; call sortFinishedLast() to tidy it for display.
  void parse(const char* data, size_t len);
  // Writes the file content back; returns bytes written (excluding NUL),
  // truncating at bufSize-1.
  size_t serialize(char* buf, size_t bufSize) const;
  void loadSample();
};

extern const char kSampleTasksFile[];
