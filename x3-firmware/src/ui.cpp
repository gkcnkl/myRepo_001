#include "ui.h"

#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <stdio.h>

namespace ui {
namespace {

void centered(Adafruit_GFX& g, const char* text, int16_t y) {
  int16_t bx, by;
  uint16_t bw, bh;
  g.getTextBounds(text, 0, y, &bx, &by, &bw, &bh);
  g.setCursor((g.width() - bw) / 2 - bx, y);
  g.print(text);
}

}  // namespace

void drawHello(Adafruit_GFX& g, const char* lastButton, uint32_t pressCount, const char* panelName) {
  g.fillScreen(0);
  g.setTextColor(1);
  g.drawRect(8, 8, g.width() - 16, g.height() - 16, 1);
  g.fillRect(8, 8, g.width() - 16, 40, 1);  // marks the TOP edge

  g.setFont(&FreeSansBold18pt7b);
  centered(g, "Merhaba!", 180);
  g.setFont(&FreeSans12pt7b);
  centered(g, "Ozel firmware calisiyor", 230);
  centered(g, "Bir tusa bas:", 360);

  g.setFont(&FreeSansBold18pt7b);
  centered(g, lastButton, 430);

  char line[40];
  snprintf(line, sizeof line, "Basma sayisi: %lu", static_cast<unsigned long>(pressCount));
  g.setFont(&FreeSans12pt7b);
  centered(g, line, 520);

  centered(g, panelName, g.height() - 40);
}

}  // namespace ui
