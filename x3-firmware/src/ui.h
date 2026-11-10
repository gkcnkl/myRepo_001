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
// Hotspot details, a Wi-Fi QR code for phones and the page address.
void drawWifi(Adafruit_GFX& g, const App& app);
// The bookshelf: spines side by side on three shelves, details below.
void drawLibrary(Adafruit_GFX& g, const App& app);
// Sleep screen: a library lending card listing the books being read.
void drawLendingCard(Adafruit_GFX& g, const App& app);
// Status line under a book page (the page itself is drawn by the reader).
// `turkish` picks the uppercase rule for the chapter title (i -> İ or I).
void drawReaderFooter(Adafruit_GFX& g, const char* chapter, uint8_t percent, uint32_t page, uint32_t pages,
                      bool turkish = true);
// Full-screen message while a book opens / when it fails.
void drawReaderMessage(Adafruit_GFX& g, const App& app, const char* title, const char* message);

}  // namespace ui
