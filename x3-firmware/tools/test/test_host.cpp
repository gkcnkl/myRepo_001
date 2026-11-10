// Host unit tests for the hardware-independent parts (tasks, text, app).
#include <stdio.h>
#include <string.h>

#include "../../src/app.h"
#include "../../src/clock.h"
#include "../../src/tasks.h"
#include "../../src/text.h"

static int failures = 0;
#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) {                                                     \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
      ++failures;                                                      \
    }                                                                  \
  } while (0)

static void testParse() {
  const char* f = "\xEF\xBB\xBF# yorum\r\n[ ] Bir\r\n\r\n[x] İki  \n  - [X] Üç\nDört\n[ ]   \n";
  TaskList t;
  t.parse(f, strlen(f));
  CHECK(t.count == 4);
  CHECK(strcmp(t.items[0].text, "Bir") == 0 && !t.items[0].done);
  CHECK(strcmp(t.items[1].text, "İki") == 0 && t.items[1].done);
  CHECK(strcmp(t.items[2].text, "Üç") == 0 && t.items[2].done);
  CHECK(strcmp(t.items[3].text, "Dört") == 0 && !t.items[3].done);
  CHECK(t.doneCount() == 2);

  char buf[512];
  t.serialize(buf, sizeof buf);
  TaskList u;
  u.parse(buf, strlen(buf));
  CHECK(u.count == 4);
  for (int i = 0; i < 4; ++i) {
    CHECK(strcmp(u.items[i].text, t.items[i].text) == 0);
    CHECK(u.items[i].done == t.items[i].done);
  }
}

static void testBriefingFormat() {
  const char* f =
      "ust: Gökçen'in günlük brifingi\n"
      "başlık: Sabah Brifingi\n"
      "alt: Sprint haftası H6\n"
      "gösterge: Hava | 2,5 °C | kar sağanağı\n"
      "gosterge: Enerji | 67 · dengeli\n"
      "gosterge: yalnız değer\n"
      "gosterge: dördüncü | atlanır\n"
      "## Günün akışı\n"
      "[x] 09:15-10:00 | Sync: (A) Ekip | toplantısı\n"
      "[ ] 18:00-19:00 | Kitap oku\n"
      "## Bugün due\n"
      "[ ] Alt katı temizle.\n"
      "## Boş bölüm\n";
  TaskList t;
  t.parse(f, strlen(f));
  CHECK(strcmp(t.eyebrow, "Gökçen'in günlük brifingi") == 0);
  CHECK(strcmp(t.title, "Sabah Brifingi") == 0);
  CHECK(strcmp(t.subtitle, "Sprint haftası H6") == 0);
  CHECK(t.indicatorCount == 3);
  CHECK(strcmp(t.indicators[0].label, "Hava") == 0 && strcmp(t.indicators[0].value, "2,5 °C") == 0 &&
        strcmp(t.indicators[0].detail, "kar sağanağı") == 0);
  CHECK(strcmp(t.indicators[1].value, "67 · dengeli") == 0 && t.indicators[1].detail[0] == '\0');
  CHECK(strcmp(t.indicators[2].value, "yalnız değer") == 0 && t.indicators[2].label[0] == '\0');
  CHECK(t.sectionCount == 3);
  CHECK(t.count == 3);
  CHECK(strcmp(t.items[0].time, "09:15-10:00") == 0);
  CHECK(strcmp(t.items[0].text, "Sync: (A) Ekip | toplantısı") == 0);  // only the first bar splits
  CHECK(t.items[0].done && t.items[0].section == 0);
  CHECK(t.items[2].time[0] == '\0' && t.items[2].section == 1);

  char buf[2048];
  t.serialize(buf, sizeof buf);
  TaskList u;
  u.parse(buf, strlen(buf));
  CHECK(strcmp(u.eyebrow, t.eyebrow) == 0 && strcmp(u.title, t.title) == 0 && strcmp(u.subtitle, t.subtitle) == 0);
  CHECK(u.indicatorCount == 3 && strcmp(u.indicators[0].detail, "kar sağanağı") == 0);
  CHECK(u.sectionCount == 3 && strcmp(u.sections[2], "Boş bölüm") == 0);
  CHECK(u.count == 3);
  for (int i = 0; i < 3; ++i) {
    CHECK(strcmp(u.items[i].text, t.items[i].text) == 0);
    CHECK(strcmp(u.items[i].time, t.items[i].time) == 0);
    CHECK(u.items[i].done == t.items[i].done && u.items[i].section == t.items[i].section);
  }
}

static void testUpper() {
  char out[64];
  utf8ToFont("salı, çğıöşü iz", out, sizeof out);
  upperFont(out);
  char want[64];
  utf8ToFont("SALI, ÇĞIÖŞÜ İZ", want, sizeof want);
  CHECK(strcmp(out, want) == 0);
}

static void testLayoutFits() {
  App a;
  a.tasks.loadSample();
  // Sample: 4 tasks under "Günün akışı", 1 under "Bugün due", no indicators.
  CHECK(a.listTop() == App::kTopNoIndicators);
  CHECK(a.rowsFrom(0) == 5);
  // Heading only where a section starts (or at the top of the window).
  CHECK(a.needsHeading(0, 0) && !a.needsHeading(1, 0) && a.needsHeading(4, 0) && a.needsHeading(2, 2));
  a.tasks.indicatorCount = 1;
  CHECK(a.listTop() == App::kTopWithIndicators);
  const int fit = a.rowsFrom(0);
  int used = App::kTopWithIndicators + fit * App::kRowH + App::kSectionH * 2;
  CHECK(used <= App::kListBottom);
}

static void testLongLineKeepsUtf8Whole() {
  char line[400] = "";
  for (int i = 0; i < 100; ++i) strcat(line, "ş");  // 2 bytes each
  TaskList t;
  t.parse(line, strlen(line));
  CHECK(t.count == 1);
  CHECK(strlen(t.items[0].text) % 2 == 0);
  CHECK(strlen(t.items[0].text) < Task::kMaxText);
}

static void testMaxTasks() {
  char f[2000] = "";
  for (int i = 0; i < 40; ++i) strcat(f, "görev\n");
  TaskList t;
  t.parse(f, strlen(f));
  CHECK(t.count == TaskList::kMax);
}

static void testUtf8() {
  char out[64];
  utf8ToFont("Çç Ğğ İı Öö Şş Üü", out, sizeof out);
  const unsigned char want[] = {0x80, 0x81, ' ', 0x82, 0x83, ' ', 0x84, 0x85, ' ',
                                0x86, 0x87, ' ', 0x88, 0x89, ' ', 0x8A, 0x8B, 0};
  CHECK(memcmp(out, want, sizeof want) == 0);
  utf8ToFont("2,5 °C · a–b", out, sizeof out);
  const unsigned char want2[] = {'2', ',', '5', ' ', 0x8D, 'C', ' ', 0x8C, ' ', 'a', 0x8E, 'b', 0};
  CHECK(memcmp(out, want2, sizeof want2) == 0);
  utf8ToFont("a€b", out, sizeof out);
  CHECK(strcmp(out, "a?b") == 0);
  utf8ToFont("abcdef", out, 4);
  CHECK(strcmp(out, "abc") == 0);
}

static void testListNavigation() {
  App a;
  a.tasks.loadSample();
  const int n = a.tasks.count;
  CHECK(n == 5);
  CHECK(a.tasks.sectionCount == 2 && a.tasks.items[4].section == 1);
  a.onButton(Button::Up, 0);
  CHECK(a.selected == n - 1);  // wraps to the bottom
  a.onButton(Button::Down, 0);
  CHECK(a.selected == 0);
  Effects fx = a.onButton(Button::Confirm, 0);
  CHECK(fx.save && fx.redraw && a.tasks.items[0].done);
  fx = a.onButton(Button::Confirm, 0);
  CHECK(!a.tasks.items[0].done);
  fx = a.onButton(Button::Back, 0);
  CHECK(fx.redraw && fx.deepClean);
  CHECK(!a.onButton(Button::Down, 0).deepClean);
  CHECK(a.onButton(Button::Power, 0).sleep);
}

static void testScroll() {
  App a;
  char f[400] = "";
  for (int i = 0; i < 20; ++i) strcat(f, "x\n");
  a.tasks.parse(f, strlen(f));
  CHECK(a.rowsFrom(0) < 20);
  for (int i = 0; i < 17; ++i) {
    a.onButton(Button::Down, 0);
    CHECK(a.selected >= a.scroll && a.selected < a.scroll + a.rowsFrom(a.scroll));  // always on screen
  }
  CHECK(a.selected == 17 && a.scroll > 0);
  a.onButton(Button::Up, 0);
  CHECK(a.selected == 16 && a.selected < a.scroll + a.rowsFrom(a.scroll));
  a.onButton(Button::Down, 0);
  a.onButton(Button::Down, 0);
  a.onButton(Button::Down, 0);
  a.onButton(Button::Down, 0);  // wraps to 0
  CHECK(a.selected == 0 && a.scroll == 0);
}

static void testFocusFlow() {
  App a;
  a.tasks.loadSample();
  a.selected = 2;
  Effects fx = a.onButton(Button::Right, 1000);
  CHECK(a.screen == Screen::Focus && a.focusTask == 2 && fx.fullRefresh);
  CHECK(a.focusMinutesLeft(1000) == 25);
  CHECK(a.focusMinutesLeft(1000 + 60000) == 24);
  CHECK(a.focusMinutesLeft(1000 + 60000 - 1) == 25);
  CHECK(!a.onButton(Button::Power, 2000).sleep);  // power ignored while focusing

  // Minute change triggers a redraw; same minute does not.
  CHECK(!a.onTick(1000 + 30000).redraw);
  CHECK(a.onTick(1000 + 60000).redraw);

  // Pause for 10 minutes: time does not run.
  a.onButton(Button::Confirm, 1000 + 5 * 60000);
  CHECK(a.focusPaused);
  CHECK(a.focusMinutesLeft(1000 + 15 * 60000) == 20);
  CHECK(!a.onTick(1000 + 15 * 60000).sleep);  // no idle sleep while focusing
  a.onButton(Button::Confirm, 1000 + 15 * 60000);
  CHECK(!a.focusPaused);
  CHECK(a.focusMinutesLeft(1000 + 16 * 60000) == 19);

  // Finish: 20 more minutes after resume.
  fx = a.onTick(1000 + 35 * 60000);
  CHECK(a.screen == Screen::FocusDone && fx.redraw);
  fx = a.onButton(Button::Confirm, 1000 + 35 * 60000);
  CHECK(fx.save && a.tasks.items[2].done && a.screen == Screen::List);
}

static void testFocusCancel() {
  App a;
  a.tasks.loadSample();
  a.onButton(Button::Right, 0);
  a.onButton(Button::Back, 1000);
  CHECK(a.screen == Screen::List);
  CHECK(!a.tasks.items[0].done);
}

static void testIdleSleep() {
  App a;
  a.tasks.loadSample();
  a.onButton(Button::Down, 1000);
  CHECK(!a.onTick(1000 + App::kIdleSleepMs - 1).sleep);
  CHECK(a.onTick(1000 + App::kIdleSleepMs).sleep);
}

static void testEmptyList() {
  App a;
  CHECK(!a.onButton(Button::Confirm, 0).save);
  CHECK(!a.onButton(Button::Right, 0).redraw);
  CHECK(a.screen == Screen::List);
}

static void testClock() {
  CHECK(weekdayOf(2026, 11, 10) == 2);  // Tuesday
  CHECK(weekdayOf(2024, 2, 29) == 4);   // Thursday
  CHECK(weekdayOf(2027, 1, 3) == 0);    // Sunday

  LocalTime t;
  t.year = 2026, t.month = 11, t.day = 10;
  char buf[64];
  formatTurkishDate(t, buf, sizeof buf);
  CHECK(strcmp(buf, "Salı, 10 Kasım") == 0);
  t.year = 2027, t.month = 2, t.day = 3;
  formatTurkishDate(t, buf, sizeof buf);
  CHECK(strcmp(buf, "Çarşamba, 3 Şubat") == 0);

  LocalTime never;  // RTC never set reads 2000-01-01
  never.year = 2000, never.month = 1, never.day = 1;
  formatTurkishDate(never, buf, sizeof buf);
  CHECK(buf[0] == '\0');
  t.year = 2026, t.month = 2, t.day = 29;
  CHECK(!isPlausible(t));

  LocalTime p;
  const char* a = "\xEF\xBB\xBF 10.11.2026 15:30\r\n";
  CHECK(parseSetTime(a, strlen(a), p));
  CHECK(p.year == 2026 && p.month == 11 && p.day == 10 && p.hour == 15 && p.minute == 30 && p.second == 0);
  const char* b = "2026-11-10 09:05:42";
  CHECK(parseSetTime(b, strlen(b), p));
  CHECK(p.hour == 9 && p.minute == 5 && p.second == 42);
  const char* bad[] = {"10.11.2026", "31.02.2026 10:00", "10.11.2026 25:00", "yarın 10:00", "10/11/2026 10:00",
                       "10.11.2026 10:00 x"};
  for (const char* s : bad) CHECK(!parseSetTime(s, strlen(s), p));
}

int main() {
  testClock();
  testParse();
  testBriefingFormat();
  testUpper();
  testLayoutFits();
  testLongLineKeepsUtf8Whole();
  testMaxTasks();
  testUtf8();
  testListNavigation();
  testScroll();
  testFocusFlow();
  testFocusCancel();
  testIdleSleep();
  testEmptyList();
  if (failures) {
    printf("%d check(s) failed\n", failures);
    return 1;
  }
  printf("all host tests passed\n");
  return 0;
}
