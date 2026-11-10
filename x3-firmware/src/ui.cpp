#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "fonts/Inter17.h"
#include "fonts/Inter22.h"
#include "fonts/InterBold150Digits.h"
#include "fonts/InterBold34.h"
#include "fonts/InterSemi22.h"
#include "text.h"

namespace ui {
namespace {

constexpr int16_t kMargin = 32;
constexpr int16_t kHeaderBottom = 104;
constexpr int16_t kRowsTop = 128;
constexpr int16_t kRowH = 66;
constexpr int16_t kFooterTop = 704;
constexpr int16_t kCheck = 26;

struct Encoded {
  char s[160];
  explicit Encoded(const char* utf8) { utf8ToFont(utf8, s, sizeof s); }
};

uint16_t widthOf(Adafruit_GFX& g, const char* enc) {
  int16_t bx, by;
  uint16_t bw, bh;
  g.getTextBounds(enc, 0, 100, &bx, &by, &bw, &bh);
  return bw + (bx > 0 ? bx : 0);
}

void text(Adafruit_GFX& g, const GFXfont* f, const char* utf8, int16_t x, int16_t y, uint16_t color = 1) {
  Encoded e(utf8);
  g.setFont(f);
  g.setTextColor(color);
  g.setCursor(x, y);
  g.print(e.s);
}

void textCentered(Adafruit_GFX& g, const GFXfont* f, const char* utf8, int16_t y, uint16_t color = 1) {
  Encoded e(utf8);
  g.setFont(f);
  g.setTextColor(color);
  g.setCursor((g.width() - widthOf(g, e.s)) / 2, y);
  g.print(e.s);
}

void textRight(Adafruit_GFX& g, const GFXfont* f, const char* utf8, int16_t right, int16_t y) {
  Encoded e(utf8);
  g.setFont(f);
  g.setTextColor(1);
  g.setCursor(right - widthOf(g, e.s), y);
  g.print(e.s);
}

// Truncates an already-encoded string in place with "..." to fit maxW.
void fitWidth(Adafruit_GFX& g, char* enc, uint16_t maxW) {
  if (widthOf(g, enc) <= maxW) return;
  size_t n = strlen(enc);
  while (n > 0) {
    enc[--n] = '\0';
    char tmp[170];
    snprintf(tmp, sizeof tmp, "%s...", enc);
    if (widthOf(g, tmp) <= maxW) {
      strcpy(enc, tmp);
      return;
    }
  }
}

// Word-wraps an encoded string into at most maxLines centered lines.
void wrapCentered(Adafruit_GFX& g, const GFXfont* f, const char* utf8, int16_t y, int16_t lineH, int maxLines,
                  uint16_t maxW) {
  Encoded e(utf8);
  g.setFont(f);
  g.setTextColor(1);
  const char* p = e.s;
  for (int line = 0; line < maxLines && *p; ++line) {
    char buf[170] = "";
    const char* best = nullptr;  // end of the last word that fits
    const char* q = p;
    while (true) {
      const char* wordEnd = q;
      while (*wordEnd && *wordEnd != ' ') ++wordEnd;
      char trial[170];
      snprintf(trial, sizeof trial, "%.*s", static_cast<int>(wordEnd - p), p);
      if (widthOf(g, trial) > maxW && best) break;
      best = wordEnd;
      strcpy(buf, trial);
      if (!*wordEnd) break;
      q = wordEnd + 1;
    }
    p = best;
    while (*p == ' ') ++p;
    if (line == maxLines - 1 && *p) {
      // Out of lines: append the rest and let fitWidth add "...".
      char rest[340];
      snprintf(rest, sizeof rest, "%s %s", buf, p);
      strncpy(buf, rest, sizeof buf - 1);
      buf[sizeof buf - 1] = '\0';
      fitWidth(g, buf, maxW);
    }
    g.setCursor((g.width() - widthOf(g, buf)) / 2, y + line * lineH);
    g.print(buf);
  }
}

void checkbox(Adafruit_GFX& g, int16_t x, int16_t y, bool checked, uint16_t ink) {
  g.drawRect(x, y, kCheck, kCheck, ink);
  g.drawRect(x + 1, y + 1, kCheck - 2, kCheck - 2, ink);
  if (!checked) return;
  g.fillRect(x, y, kCheck, kCheck, ink);
  const uint16_t paper = ink ? 0 : 1;
  for (int d = -1; d <= 1; ++d) {
    g.drawLine(x + 6, y + 13 + d, x + 11, y + 18 + d, paper);
    g.drawLine(x + 11, y + 18 + d, x + 20, y + 8 + d, paper);
  }
}

void progressBar(Adafruit_GFX& g, int16_t x, int16_t y, int16_t w, int16_t h, int num, int den) {
  g.drawRect(x, y, w, h, 1);
  if (den > 0 && num > 0) g.fillRect(x, y, static_cast<int16_t>(static_cast<int32_t>(w) * num / den), h, 1);
}

void footer(Adafruit_GFX& g, const char* line1, const char* line2) {
  g.drawFastHLine(kMargin, kFooterTop, g.width() - 2 * kMargin, 1);
  textCentered(g, &Inter17, line1, kFooterTop + 36);
  textCentered(g, &Inter17, line2, kFooterTop + 66);
}

}  // namespace

void drawList(Adafruit_GFX& g, const App& app, bool sleeping) {
  g.fillScreen(0);
  const TaskList& t = app.tasks;

  text(g, &InterBold34, "Bugün", kMargin, 62);
  char count[24];
  snprintf(count, sizeof count, "%d / %d", t.doneCount(), t.count);
  textRight(g, &InterSemi22, count, g.width() - kMargin, 60);
  progressBar(g, kMargin, 82, g.width() - 2 * kMargin, 10, t.doneCount(), t.count);

  if (t.count == 0) {
    textCentered(g, &InterSemi22, "Görev yok", 330);
    textCentered(g, &Inter17, "SD karttaki gorevler.txt dosyasına", 380);
    textCentered(g, &Inter17, "her satıra bir görev yaz.", 408);
  }

  const int last = t.count < app.scroll + App::kVisibleRows ? t.count : app.scroll + App::kVisibleRows;
  for (int i = app.scroll; i < last; ++i) {
    const int16_t y = kRowsTop + (i - app.scroll) * kRowH;
    const bool sel = !sleeping && i == app.selected;
    const uint16_t ink = sel ? 0 : 1;
    if (sel) g.fillRoundRect(kMargin - 12, y, g.width() - 2 * kMargin + 24, kRowH - 8, 10, 1);

    const int16_t cy = y + (kRowH - 8 - kCheck) / 2;
    checkbox(g, kMargin, cy, t.items[i].done, ink);

    const int16_t tx = kMargin + kCheck + 18;
    const uint16_t maxW = g.width() - kMargin - tx;
    Encoded e(t.items[i].text);
    g.setFont(&Inter22);
    fitWidth(g, e.s, maxW);
    g.setTextColor(ink);
    const int16_t baseline = y + (kRowH - 8) / 2 + 8;
    g.setCursor(tx, baseline);
    g.print(e.s);
    if (t.items[i].done) {
      const int16_t w = widthOf(g, e.s);
      g.drawFastHLine(tx, baseline - 8, w, ink);
      g.drawFastHLine(tx, baseline - 7, w, ink);
    }
  }

  // Scroll hints when the list is longer than the screen.
  if (app.scroll > 0) textRight(g, &Inter17, "yukarıda daha var", g.width() - kMargin, kRowsTop - 6);
  if (last < t.count) textRight(g, &Inter17, "aşağıda daha var", g.width() - kMargin, kFooterTop - 12);

  if (sleeping) {
    g.fillRect(0, kFooterTop, g.width(), g.height() - kFooterTop, 1);
    textCentered(g, &InterSemi22, "Uyuyor", kFooterTop + 40, 0);
    textCentered(g, &Inter17, "Açmak için GÜÇ tuşuna bas", kFooterTop + 72, 0);
  } else if (t.count == 0) {
    footer(g, "GERİ: ekranı tazele", "GÜÇ: uyku");
  } else {
    footer(g, "YUKARI / AŞAĞI: seç     TAMAM: bitti", "SAĞ: odaklan (25 dk)     GÜÇ: uyku");
  }
  if (!app.sdAvailable && !sleeping) textCentered(g, &Inter17, "SD kart yok: değişiklikler kaydedilmez", kFooterTop - 12);
}

void drawFocus(Adafruit_GFX& g, const App& app, uint32_t nowMs) {
  g.fillScreen(0);
  text(g, &InterBold34, "Odak", kMargin, 62);
  textRight(g, &InterSemi22, app.focusPaused ? "duraklatıldı" : "25 dk", g.width() - kMargin, 60);
  g.drawFastHLine(kMargin, 84, g.width() - 2 * kMargin, 1);

  if (app.focusTask >= 0 && app.focusTask < app.tasks.count) {
    wrapCentered(g, &InterSemi22, app.tasks.items[app.focusTask].text, 160, 34, 3, g.width() - 2 * kMargin);
  }

  char minutes[8];
  snprintf(minutes, sizeof minutes, "%d", app.focusMinutesLeft(nowMs));
  g.setFont(&InterBold150Digits);
  g.setTextColor(1);
  g.setCursor((g.width() - widthOf(g, minutes)) / 2, 470);
  g.print(minutes);
  textCentered(g, &Inter22, "dakika kaldı", 520);

  const int total = static_cast<int>(App::kFocusMs / 1000);
  const int done = static_cast<int>(app.focusElapsed(nowMs) / 1000);
  progressBar(g, kMargin + 40, 570, g.width() - 2 * kMargin - 80, 14, done, total);

  footer(g, app.focusPaused ? "TAMAM: devam et" : "TAMAM: duraklat", "GERİ: iptal, listeye dön");
}

void drawFocusDone(Adafruit_GFX& g, const App& app) {
  g.fillScreen(0);
  textCentered(g, &InterBold34, "Süre doldu!", 230);
  if (app.focusTask >= 0 && app.focusTask < app.tasks.count) {
    wrapCentered(g, &InterSemi22, app.tasks.items[app.focusTask].text, 310, 34, 3, g.width() - 2 * kMargin);
  }
  textCentered(g, &Inter22, "Bu görevi bitirdin mi?", 460);
  footer(g, "TAMAM: evet, işaretle", "GERİ: hayır, listeye dön");
}

}  // namespace ui
