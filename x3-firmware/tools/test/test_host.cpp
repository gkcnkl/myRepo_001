// Host unit tests for the hardware-independent parts (tasks, text, app).
#include <stdio.h>
#include <string.h>

#include "../../src/app.h"
#include "../../src/clock.h"
#include "../../src/sync_logic.h"
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
  utf8ToFont("rabbit-hole", out, sizeof out);
  upperFont(out, false);
  CHECK(strcmp(out, "RABBIT-HOLE") == 0);
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
  fx = a.onButton(Button::Back, 0);  // BACK opens the bookshelf
  CHECK(fx.redraw && a.screen == Screen::Library);
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

static void testWifiFlow() {
  App a;
  a.tasks.loadSample();
  Effects fx = a.onButton(Button::Left, 0);
  CHECK(a.screen == Screen::Wifi && fx.startWifi && fx.redraw);
  CHECK(!a.onButton(Button::Down, 0).redraw);  // list keys do nothing here
  // The page restarts the idle timer; 10 minutes without use closes it.
  a.touch(5 * 60000);
  CHECK(!a.onTick(5 * 60000 + App::kIdleSleepMs).sleep);  // the short list timeout does not apply
  fx = a.onTick(5 * 60000 + App::kWifiIdleMs);
  CHECK(fx.sleep && fx.stopWifi);

  App b;
  b.tasks.loadSample();
  b.selected = 4;
  b.onButton(Button::Left, 0);
  b.tasks.parse("[ ] tek\n", 8);  // an upload replaced the list with one task
  fx = b.onButton(Button::Back, 1000);
  CHECK(b.screen == Screen::List && fx.stopWifi && fx.redraw);
  CHECK(b.selected == 0);
  fx = b.onButton(Button::Left, 2000);
  CHECK(b.onButton(Button::Power, 3000).stopWifi);

  App synced;  // set up: SOL syncs directly, no hotspot
  synced.tasks.loadSample();
  synced.syncConfigured = true;
  fx = synced.onButton(Button::Left, 0);
  CHECK(fx.syncNow && !fx.startWifi && synced.screen == Screen::List);

  App empty;  // works with an empty list too
  CHECK(empty.onButton(Button::Left, 0).startWifi);
  CHECK(!empty.onButton(Button::Confirm, 0).syncNow);  // sync not configured: nothing
  empty.syncConfigured = true;
  fx = empty.onButton(Button::Confirm, 0);
  CHECK(fx.syncNow && fx.stopWifi && empty.screen == Screen::List);
}

static void testLibraryModel() {
  BookInfo b;
  CHECK(bookFromFileName("Kürk_Mantolu_Madonna.EPUB", 300000, b));
  CHECK(strcmp(b.title, "Kürk Mantolu Madonna") == 0 && !b.isText && strlen(b.id) == 8);
  CHECK(bookFromFileName("notlar.txt", 10, b) && b.isText);
  CHECK(!bookFromFileName("kapak.jpg", 10, b));
  CHECK(!bookFromFileName(".gizli.epub", 10, b));

  BookInfo s;
  bookFromFileName("x.epub", 1000, s);
  strcpy(s.title, "Suç ve Ceza");
  strcpy(s.author, "Dostoyevski");
  s.started = DateStamp{2026, 11, 10};
  s.lastRead = 202611101530ull;
  s.spine = 7;
  s.charOffset = 12345;
  s.percent = 42;
  char buf[512];
  serializeBookState(s, buf, sizeof buf);
  BookInfo t;
  bookFromFileName("x.epub", 1000, t);
  parseBookState(buf, strlen(buf), t);
  CHECK(strcmp(t.title, "Suç ve Ceza") == 0 && strcmp(t.author, "Dostoyevski") == 0);
  CHECK(t.started.year == 2026 && t.started.month == 11 && t.started.day == 10);
  CHECK(t.lastRead == 202611101530ull && t.spine == 7 && t.charOffset == 12345 && t.percent == 42 && !t.finished);

  Library lib;
  const char* names[] = {"c.epub", "a.epub", "b.epub", "d.txt"};
  for (const char* n : names) bookFromFileName(n, 50000, lib.books[lib.count++]);
  lib.sortByTitle();
  CHECK(strcmp(lib.books[0].title, "a") == 0 && strcmp(lib.books[3].title, "d") == 0);
  lib.books[1].lastRead = 5;  // b
  lib.books[2].lastRead = 9;  // c
  lib.books[3].lastRead = 7;  // d, finished
  lib.books[3].finished = true;
  const BookInfo* reading[4];
  CHECK(lib.inProgress(reading, 4) == 2);
  CHECK(strcmp(reading[0]->title, "c") == 0 && strcmp(reading[1]->title, "b") == 0);
  CHECK(lib.inProgress(reading, 1) == 1 && strcmp(reading[0]->title, "c") == 0);
}

static void testShelfLayout() {
  Library lib;
  for (int i = 0; i < 20; ++i) {
    char n[16];
    snprintf(n, sizeof n, "k%02d.epub", i);
    bookFromFileName(n, 100000u * (i + 1), lib.books[lib.count++]);
  }
  SpineBox boxes[Library::kMax + 1];
  const int shelves = ShelfLayout::place(lib, boxes, Library::kMax + 1);
  CHECK(shelves >= 2);
  for (int i = 0; i <= lib.count; ++i) {
    CHECK(boxes[i].x >= ShelfLayout::kLeft && boxes[i].x + boxes[i].w <= ShelfLayout::kRight);
    CHECK(boxes[i].h <= ShelfLayout::kMaxSpineH && boxes[i].w >= 30);
    if (i > 0 && boxes[i].shelf == boxes[i - 1].shelf) CHECK(boxes[i].x > boxes[i - 1].x);  // side by side
  }
  CHECK(boxes[lib.count].w == ShelfLayout::kAddW);  // the "+ Ekle" spine comes last

  // Thicker books get thicker spines.
  BookInfo small, big;
  bookFromFileName("s.epub", 20 * 1024, small);
  bookFromFileName("b.epub", 3000 * 1024, big);
  CHECK(ShelfLayout::spineWidth(big) > ShelfLayout::spineWidth(small));
}

static void testLibraryNavigation() {
  App a;
  a.tasks.loadSample();
  Effects fx = a.onButton(Button::Back, 0);  // briefing -> shelf
  CHECK(a.screen == Screen::Library && fx.redraw);
  // Empty shelf: only "+ Ekle"; TAMAM opens Wi-Fi and BACK there returns here.
  CHECK(a.libSelected == 0);
  fx = a.onButton(Button::Confirm, 0);
  CHECK(a.screen == Screen::Wifi && fx.startWifi);
  fx = a.onButton(Button::Back, 0);
  CHECK(a.screen == Screen::Library && fx.stopWifi);

  for (int i = 0; i < 20; ++i) {
    char n[16];
    snprintf(n, sizeof n, "k%02d.epub", i);
    bookFromFileName(n, 100000u * (i + 1), a.lib.books[a.lib.count++]);
  }
  a.libSelected = 0;
  a.onButton(Button::Left, 0);
  CHECK(a.libSelected == a.lib.count);  // wraps to "+ Ekle"
  a.onButton(Button::Right, 0);
  CHECK(a.libSelected == 0);
  SpineBox boxes[Library::kMax + 1];
  ShelfLayout::place(a.lib, boxes, Library::kMax + 1);
  a.onButton(Button::Down, 0);
  CHECK(boxes[a.libSelected].shelf == 1);
  a.onButton(Button::Up, 0);
  CHECK(boxes[a.libSelected].shelf == 0);
  CHECK(!a.onButton(Button::Up, 0).redraw);  // no shelf above
  // Walk to the last item: the view scrolls so it stays on screen.
  for (int i = 0; i < a.lib.count; ++i) a.onButton(Button::Right, 0);
  CHECK(boxes[a.libSelected].shelf >= a.shelfScroll &&
        boxes[a.libSelected].shelf < a.shelfScroll + ShelfLayout::kShelves);

  a.libSelected = 3;
  fx = a.onButton(Button::Confirm, 0);
  CHECK(a.screen == Screen::Reader && fx.openBook);
  CHECK(a.onButton(Button::Right, 0).pageDelta == 1);
  CHECK(a.onButton(Button::Down, 0).pageDelta == 1);
  CHECK(a.onButton(Button::Left, 0).pageDelta == -1);
  CHECK(a.onButton(Button::Up, 0).pageDelta == -1);
  fx = a.onButton(Button::Confirm, 0);  // next text size, stays in the book
  CHECK(fx.nextFontSize && !fx.closeBook && a.screen == Screen::Reader);
  CHECK(!a.onTick(App::kIdleSleepMs).sleep);  // reading allows longer idle
  fx = a.onButton(Button::Power, 0);
  CHECK(fx.sleep && fx.closeBook);
  fx = a.onButton(Button::Back, 0);
  CHECK(a.screen == Screen::Library && fx.closeBook);
  fx = a.onButton(Button::Back, 0);
  CHECK(a.screen == Screen::List);
}

static void testSyncLogic() {
  // Done marks carry over by (time, text); never lost.
  TaskList local, fresh;
  const char* l = "## A\n[x] 09:00 | Toplantı\n[ ] 10:00 | Rapor\n[x] Kitap oku\n";
  const char* f = "## A\n[ ] 09:00 | Toplantı\n[ ] 10:00 | Rapor\n[ ] Kitap oku\n[ ] 11:00 | Toplantı\n[x] Yeni\n";
  local.parse(l, strlen(l));
  fresh.parse(f, strlen(f));
  CHECK(mergeDoneMarks(fresh, local) == 2);
  CHECK(fresh.items[0].done && !fresh.items[1].done && fresh.items[2].done);
  CHECK(!fresh.items[3].done);  // same text, different time: another task
  CHECK(fresh.items[4].done);   // done on the server stays done

  char b64[64];
  const char* in = "Merhaba ğ!";
  CHECK(base64Encode(reinterpret_cast<const uint8_t*>(in), strlen(in), b64, sizeof b64) > 0);
  CHECK(strcmp(b64, "TWVyaGFiYSDEnyE=") == 0);
  CHECK(base64Encode(reinterpret_cast<const uint8_t*>("ab"), 2, b64, sizeof b64) == 4 && strcmp(b64, "YWI=") == 0);
  CHECK(base64Encode(reinterpret_cast<const uint8_t*>("abc"), 3, b64, 4) == 0);  // too small

  char sha[48];
  const char* json = "{\"name\": \"cihaz.txt\", \"sha\" : \"3d21ec53a331a6f037a91c368710b99387d012c1\", \"size\": 12}";
  CHECK(jsonStringField(json, "sha", sha, sizeof sha) && strcmp(sha, "3d21ec53a331a6f037a91c368710b99387d012c1") == 0);
  CHECK(!jsonStringField(json, "content", sha, sizeof sha));
  CHECK(!jsonStringField(json, "sha", sha, 8));  // too long for the buffer

  char body[256];
  CHECK(buildPutBody("X3 \"eşitleme\"", "YWI=", "abc", "main", body, sizeof body));
  CHECK(strcmp(body, "{\"message\":\"X3 \\\"eşitleme\\\"\",\"content\":\"YWI=\",\"branch\":\"main\",\"sha\":\"abc\"}") == 0);
  CHECK(buildPutBody("m", "YWI=", "", "main", body, sizeof body) && strstr(body, "sha") == nullptr);
  CHECK(!buildPutBody("m", "YWI=", "", "main", body, 10));

  char state[512];
  const size_t n = buildDeviceStateFile(local, nullptr, "2026-11-11 07:02", state, sizeof state);
  CHECK(n > 0 && strncmp(state, "# X3 cihaz durumu. guncellendi: 2026-11-11 07:02\n", 49) == 0);
  TaskList back;
  back.parse(state, n);
  CHECK(back.count == 3 && back.items[0].done && back.items[2].done);
}

static void testSortFinished() {
  TaskList t;
  const char* f = "[x] a\n[ ] b\n## S1\n[-] c\n[x] d\n[ ] e\n[ ] f\n## S2\n[ ] g\n";
  t.parse(f, strlen(f));
  CHECK(strcmp(t.items[0].text, "a") == 0);  // parse keeps the file order
  t.sortFinishedLast();
  const char* want[] = {"b", "a", "e", "f", "d", "c", "g"};
  CHECK(t.count == 7);
  for (int i = 0; i < 7; ++i) CHECK(strcmp(t.items[i].text, want[i]) == 0);
  CHECK(t.items[2].section == 0 && t.items[5].section == 0 && t.items[6].section == 1);
  // Marking in place does not move the row until the list is re-sorted.
  App a;
  a.tasks = t;
  a.selected = 2;
  a.onButton(Button::Confirm, 0);
  CHECK(strcmp(a.tasks.items[2].text, "e") == 0 && a.tasks.items[2].done);
  a.onButton(Button::Back, 0);  // to the shelf
  a.onButton(Button::Back, 0);  // and back: sorted
  CHECK(a.screen == Screen::List && strcmp(a.tasks.items[2].text, "f") == 0);
}

static void testCancel() {
  TaskList t;
  const char* f = "## A\n[ ] 09:00 | Toplantı\n[x] Rapor\n[-] 11:00 | İptal olan\n[~] Eski iptal\n";
  t.parse(f, strlen(f));
  CHECK(t.count == 4 && t.items[2].cancelled && !t.items[2].done && t.items[3].cancelled);
  CHECK(t.doneCount() == 1 && t.activeCount() == 2);
  char out[512];
  t.serialize(out, sizeof out);
  CHECK(strstr(out, "[-] 11:00 | İptal olan\n") != nullptr && strstr(out, "[-] Eski iptal\n") != nullptr);

  // CONFIRM cycles open -> done -> cancelled -> open.
  App a;
  a.tasks = t;
  a.selected = 0;
  a.onButton(Button::Confirm, 0);
  CHECK(a.tasks.items[0].done && !a.tasks.items[0].cancelled);
  Effects fx = a.onButton(Button::Confirm, 0);
  CHECK(!a.tasks.items[0].done && a.tasks.items[0].cancelled && fx.save);
  a.onButton(Button::Confirm, 0);
  CHECK(!a.tasks.items[0].done && !a.tasks.items[0].cancelled);

  // Device marks survive a fresh download; a server "done" wins.
  TaskList local, fresh;
  const char* l = "[-] 09:00 | Toplantı\n[x] Rapor\n[-] Okuma\n";
  const char* r = "[ ] 09:00 | Toplantı\n[ ] Rapor\n[x] Okuma\n[ ] Yeni\n";
  local.parse(l, strlen(l));
  fresh.parse(r, strlen(r));
  CHECK(mergeDoneMarks(fresh, local) == 2);
  CHECK(fresh.items[0].cancelled && !fresh.items[0].done);
  CHECK(fresh.items[1].done && !fresh.items[1].cancelled);
  CHECK(fresh.items[2].done && !fresh.items[2].cancelled);
  CHECK(!fresh.items[3].done && !fresh.items[3].cancelled);
}

static void testPickNetwork() {
  const char* saved[2] = {"Ev", "GKphone"};
  const char* seen[] = {"Komsu", "GKphone", "Ev", "Ev"};
  const int rssi[] = {-40, -60, -80, -70};
  CHECK(pickNetwork(saved, seen, rssi, 4) == 1);  // phone is stronger than home
  const int rssi2[] = {-40, -90, -50, -70};
  CHECK(pickNetwork(saved, seen, rssi2, 4) == 0);
  CHECK(pickNetwork(saved, seen, rssi, 1) == -1);  // only the neighbour
  const char* onlyPhone[2] = {"", "GKphone"};
  CHECK(pickNetwork(onlyPhone, seen, rssi, 4) == 1);
  const char* none[2] = {"", ""};
  CHECK(pickNetwork(none, seen, rssi, 4) == -1);
}

static void testBookSync() {
  CHECK(safeBookName("2026-06-06 SİNYAL #08 · Hasat- Döngüyü Tamamlamak.epub"));
  CHECK(safeBookName("not.TXT"));
  CHECK(!safeBookName("../x.epub") && !safeBookName("a/b.epub") && !safeBookName(".gizli.epub"));
  CHECK(!safeBookName("resim.png") && !safeBookName("a\nb.epub") && !safeBookName(""));

  const char* json =
      "[{\"name\":\"README.md\",\"path\":\"kitaplar/README.md\",\"sha\":\"aaa\",\"size\":10,\"type\":\"file\","
      "\"_links\":{\"self\":\"x\"}},"
      "{\"name\": \"2026-06-06 Bülten #8.epub\", \"path\":\"kitaplar/2026-06-06 Bülten #8.epub\", \"sha\": \"b1\","
      " \"size\": 3587, \"url\":\"u\", \"type\": \"file\", \"_links\": {\"git\":\"g\"}},"
      "{\"name\":\"arsiv\",\"sha\":\"c\",\"size\":0,\"type\":\"dir\"},"
      "{\"name\":\"Kısa.txt\",\"sha\":\"d2\",\"size\":12,\"type\":\"file\"}]";
  RemoteBook rb[4];
  CHECK(parseDirListing(json, rb, 4) == 2);
  CHECK(strcmp(rb[0].name, "2026-06-06 Bülten #8.epub") == 0 && strcmp(rb[0].sha, "b1") == 0 && rb[0].size == 3587);
  CHECK(strcmp(rb[1].name, "Kısa.txt") == 0 && rb[1].size == 12);
  CHECK(parseDirListing(json, rb, 1) == 1);
  CHECK(parseDirListing("[]", rb, 4) == 0);

  char url[96];
  CHECK(urlEncodePath("kitaplar/Bülten #8.epub", url, sizeof url));
  CHECK(strcmp(url, "kitaplar/B%C3%BClten%20%238.epub") == 0);
  CHECK(!urlEncodePath("kitaplar/Bülten #8.epub", url, 12));

  const char* man = "b1 2026-06-06 Bülten #8.epub\nzz Eski bülten.epub\r\nbozuk satır yok\n";
  char sha[44], name[80];
  CHECK(manifestFind(man, "2026-06-06 Bülten #8.epub", sha, sizeof sha) && strcmp(sha, "b1") == 0);
  CHECK(manifestFind(man, "Eski bülten.epub", sha, sizeof sha) && strcmp(sha, "zz") == 0);
  CHECK(!manifestFind(man, "Yok.epub", sha, sizeof sha));
  const char* c = man;
  int lines = 0;
  while (manifestNext(c, name, sizeof name, sha, sizeof sha)) ++lines;
  CHECK(lines == 3);  // "bozuk satır yok" parses as sha "bozuk" + name

  Library* lib = new Library();
  bookFromFileName("Dubliners.epub", 1000, lib->books[0]);
  lib->books[0].lastRead = 202611110702ull;
  lib->books[0].percent = 42;
  bookFromFileName("2026-06-06 Bülten #8.epub", 3587, lib->books[1]);
  lib->books[1].lastRead = 202611101830ull;
  lib->books[1].finished = true;
  lib->books[1].percent = 97;
  bookFromFileName("Yeni.txt", 10, lib->books[2]);
  lib->count = 3;
  TaskList tl;
  tl.parse("[ ] 09:00 | Toplantı\n", 22);
  char state[512];
  const size_t n = buildDeviceStateFile(tl, lib, "2026-11-11 07:05", state, sizeof state);
  CHECK(strstr(state, "# kitap | %42 | okunuyor | 2026-11-11 07:02 | Dubliners.epub\n") != nullptr);
  CHECK(strstr(state, "# kitap | %100 | bitti | 2026-11-10 18:30 | 2026-06-06 Bülten #8.epub\n") != nullptr);
  CHECK(strstr(state, "# kitap | %0 | baslanmadi | - | Yeni.txt\n") != nullptr);
  TaskList back;
  back.parse(state, n);
  CHECK(back.count == 1);  // book lines are comments to the briefing parser
  CHECK(buildDeviceStateFile(tl, lib, "x", state, 90) > 0 && strstr(state, "Yeni.txt") == nullptr);
  delete lib;
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
  LocalTime y2k;
  y2k.year = 2000, y2k.month = 1, y2k.day = 1;
  CHECK(minutesSince2000(y2k) == 0);
  LocalTime late, early;
  late.year = 2026, late.month = 11, late.day = 10, late.hour = 23, late.minute = 50;
  early.year = 2026, early.month = 11, early.day = 11, early.hour = 0, early.minute = 20;
  CHECK(minutesSince2000(early) - minutesSince2000(late) == 30);  // across midnight
  LocalTime leap;
  leap.year = 2024, leap.month = 3, leap.day = 1;
  LocalTime feb;
  feb.year = 2024, feb.month = 2, feb.day = 28;
  CHECK(minutesSince2000(leap) - minutesSince2000(feb) == 2 * 1440);  // Feb 29 exists

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
  testWifiFlow();
  testSyncLogic();
  testBookSync();
  testPickNetwork();
  testCancel();
  testSortFinished();
  testLibraryModel();
  testShelfLayout();
  testLibraryNavigation();
  if (failures) {
    printf("%d check(s) failed\n", failures);
    return 1;
  }
  printf("all host tests passed\n");
  return 0;
}
