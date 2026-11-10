#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

// Screen layouts. Pure drawing on an Adafruit_GFX target (color 1 = black ink,
// 0 = white), so the same code runs on the device and in the host preview.
namespace ui {

void drawHello(Adafruit_GFX& g, const char* lastButton, uint32_t pressCount, const char* panelName);

}  // namespace ui
