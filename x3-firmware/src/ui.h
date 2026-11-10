#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

#include "app.h"

// Screen layouts for the 528x792 portrait panel. Pure drawing on an
// Adafruit_GFX target (color 1 = black ink, 0 = white), so the same code runs
// on the device and in the host preview (tools/preview).
namespace ui {

// sleeping=true draws the list without a selection plus a "sleeping" bar;
// e-ink keeps it on screen while the device is off.
void drawList(Adafruit_GFX& g, const App& app, bool sleeping);
void drawFocus(Adafruit_GFX& g, const App& app, uint32_t nowMs);
void drawFocusDone(Adafruit_GFX& g, const App& app);

}  // namespace ui
