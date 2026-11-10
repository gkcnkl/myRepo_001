// Host preview: renders the device screens to PGM images using the exact
// same ui.cpp drawing code as the firmware (portrait 528x792).
#include <Adafruit_GFX.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "../../src/app.h"
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
  } else {
    app.tasks.loadSample();
    app.tasks.items[0].done = true;
    app.tasks.items[2].done = true;
  }
  app.selected = 1;
  strcpy(app.dateText, "Salı, 10 Kasım");
  app.batteryPercent = 84;

  { Canvas c; ui::drawList(c, app, false); c.save(out, "1-list"); }

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

  App empty;  // clock not set, battery charging
  empty.batteryPercent = 37;
  empty.charging = true;
  { Canvas c; ui::drawList(c, empty, false); c.save(out, "6-empty"); }
  return 0;
}
