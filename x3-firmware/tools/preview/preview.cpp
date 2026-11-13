// Host preview: renders the device screens to PGM images using the exact
// same ui.cpp drawing code as the firmware (portrait 528x792).
#include <Adafruit_GFX.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "../../src/app.h"
#include "../../src/library.h"
#include "../../src/ui.h"

class Canvas : public Adafruit_GFX {
 public:
  Canvas() : Adafruit_GFX(528, 792), px(528 * 792, 255) {}
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x < 0 || y < 0 || x >= width() || y >= height()) return;
    px[y * width() + x] = c ? 0 : 255;
  }
  void save(const char* dir, const char* name) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s.pgm", dir, name);
    FILE* f = fopen(path, "wb");
    fprintf(f, "P5\n%d %d\n255\n", width(), height());
    fwrite(px.data(), 1, px.size(), f);
    fclose(f);
  }
  std::vector<uint8_t> px;
};

// Usage: x3-preview <out-dir> [briefing-file]
int main(int argc, char** argv) {
  const char* out = argc > 1 ? argv[1] : ".";
  App app;
  if (argc > 2) {
    FILE* f = fopen(argv[2], "rb");
    static char buf[8192];
    const size_t n = f ? fread(buf, 1, sizeof buf, f) : 0;
    if (f) fclose(f);
    app.tasks.parse(buf, n);
    app.tasks.sortFinishedLast();  // as the device does after loading
  } else {
    app.tasks.loadSample();
    app.tasks.items[0].done = true;
    app.tasks.items[2].done = true;
  }
  app.selected = 1;
  strcpy(app.dateText, "Salı, 10 Kasım");
  app.batteryPercent = 84;

  { Canvas c; ui::drawList(c, app, false); c.save(out, "1-list"); }
  {  // the fewest screens that show every row while paging down: walk the
     // selection down, then pick, from each saved screen, the furthest later
     // screen that still starts no lower than where the saved one ended
    static App states[64];
    int n = 0;
    App walk = app;
    walk.selected = 0;
    walk.scroll = 0;
    for (; n < 64; ++n) {
      states[n] = walk;
      if (walk.selected >= walk.tasks.count - 1) { ++n; break; }
      walk.onButton(Button::Down, 0);
    }
    int page = 1, at = 0;
    while (true) {
      Canvas c;
      ui::drawList(c, states[at], false);
      char name[32];
      snprintf(name, sizeof name, "page-%d", page++);
      c.save(out, name);
      if (at >= n - 1) break;
      const int end = states[at].scroll + states[at].rowsFrom(states[at].scroll);
      int next = at + 1;
      for (int k = at + 1; k < n; ++k)
        if (states[k].scroll <= end) next = k;
      if (states[next].scroll == states[at].scroll) next = n - 1;  // nothing new before the end
      at = next;
    }
  }
  {  // the end of the list, selection on its last row (e.g. a measures block)
    App end = app;
    end.selected = 0;
    while (end.selected < end.tasks.count - 1) end.onButton(Button::Down, 0);
    Canvas c;
    ui::drawList(c, end, false);
    c.save(out, "1b-list-end");
  }

  app.onButton(Button::Right, 0);  // focus on task 1
  { Canvas c; ui::drawFocus(c, app, 7 * 60 * 1000); c.save(out, "2-focus"); }
  app.onButton(Button::Confirm, 7 * 60 * 1000);  // pause
  { Canvas c; ui::drawFocus(c, app, 9 * 60 * 1000); c.save(out, "3-focus-paused"); }
  app.screen = Screen::FocusDone;
  { Canvas c; ui::drawFocusDone(c, app); c.save(out, "4-focus-done"); }
  app.screen = Screen::List;
  { Canvas c; ui::drawList(c, app, true); c.save(out, "5-sleep"); }

  strcpy(app.wifiSsid, "GK's Xteink X3");
  strcpy(app.wifiPass, "48213907");
  strcpy(app.wifiUrl, "http://192.168.4.1");
  strcpy(app.wifiStatus, "Brifing alındı · 12 görev");
  { Canvas c; ui::drawWifi(c, app); c.save(out, "7-wifi"); }

  // Bookshelf + lending card with a sample library.
  {
    const struct { const char* file; const char* title; const char* author; uint32_t kb; int pct; int day; } books[] = {
        {"a.epub", "Kürk Mantolu Madonna", "Sabahattin Ali", 300, 37, 2},
        {"b.epub", "Tutunamayanlar", "Oğuz Atay", 1900, 12, 28},
        {"c.epub", "Saatleri Ayarlama Enstitüsü", "Ahmet Hamdi Tanpınar", 900, 0, 0},
        {"d.epub", "Kaşağı ve Alice", "Ömer Seyfettin", 10, 64, 10},
        {"e.epub", "İnce Memed", "Yaşar Kemal", 1500, 0, 0},
        {"f.epub", "Aylak Adam", "Yusuf Atılgan", 250, 0, 0},
        {"g.epub", "Çalıkuşu", "Reşat Nuri Güntekin", 700, 0, 0},
        {"h.epub", "Martin Eden", "Jack London", 800, 0, 0},
        {"i.epub", "Dune", "Frank Herbert", 2400, 81, 15},
        {"j.epub", "Suç ve Ceza", "Dostoyevski", 2000, 0, 0},
        {"k.epub", "Beyaz Diş", "Jack London", 400, 0, 0},
        {"l.epub", "Simyacı", "Paulo Coelho", 200, 0, 0},
    };
    int n = 0;
    for (const auto& bk : books) {
      BookInfo& b = app.lib.books[n++];
      bookFromFileName(bk.file, bk.kb * 1024, b);
      strcpy(b.title, bk.title);
      strcpy(b.author, bk.author);
      if (bk.pct) {
        b.percent = bk.pct;
        b.lastRead = 202611100000ull + bk.pct;
        b.started = DateStamp{2026, static_cast<uint8_t>(bk.day > 20 ? 10 : 11), static_cast<uint8_t>(bk.day)};
      }
    }
    app.lib.count = n;
    app.lib.sortByTitle();
    app.libSelected = 6;
    { Canvas c; ui::drawLibrary(c, app); c.save(out, "8-library"); }
    { Canvas c; ui::drawLendingCard(c, app); c.save(out, "9-lending-card"); }
    App none;
    strcpy(none.dateText, app.dateText);
    { Canvas c; ui::drawLibrary(c, none); c.save(out, "10-library-empty"); }
  }

  App empty;  // clock not set, battery charging
  empty.batteryPercent = 37;
  empty.charging = true;
  { Canvas c; ui::drawList(c, empty, false); c.save(out, "6-empty"); }
  return 0;
}
