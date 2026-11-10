// Host unit tests for the hardware-independent parts (tasks, text, app).
#include <stdio.h>
#include <string.h>

#include "../../src/app.h"
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
  for (int i = 0; i < 12; ++i) strcat(f, "x\n");
  a.tasks.parse(f, strlen(f));
  for (int i = 0; i < 9; ++i) a.onButton(Button::Down, 0);
  CHECK(a.selected == 9);
  CHECK(a.scroll == 9 - App::kVisibleRows + 1);
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

int main() {
  testParse();
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
