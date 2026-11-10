// Host preview: renders the device screens to PGM images using the exact
// same ui.cpp drawing code as the firmware (portrait 528x792).
#include <Adafruit_GFX.h>
#include <stdio.h>
#include <vector>

#include "../../src/ui.h"

class Canvas : public Adafruit_GFX {
 public:
  Canvas() : Adafruit_GFX(528, 792), px(528 * 792, 255) {}
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x < 0 || y < 0 || x >= width() || y >= height()) return;
    px[y * width() + x] = c ? 0 : 255;
  }
  void save(const char* path) {
    FILE* f = fopen(path, "wb");
    fprintf(f, "P5\n%d %d\n255\n", width(), height());
    fwrite(px.data(), 1, px.size(), f);
    fclose(f);
  }
  std::vector<uint8_t> px;
};

int main(int argc, char** argv) {
  const char* out = argc > 1 ? argv[1] : ".";
  char path[512];
  Canvas a;
  ui::drawHello(a, "-", 0, "Ekran: UC8253");
  snprintf(path, sizeof path, "%s/hello-boot.pgm", out);
  a.save(path);
  Canvas b;
  ui::drawHello(b, "TAMAM (CONFIRM)", 3, "Ekran: UC8253");
  snprintf(path, sizeof path, "%s/hello-button.pgm", out);
  b.save(path);
  return 0;
}
