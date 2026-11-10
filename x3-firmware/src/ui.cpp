#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "fonts/Inter14.h"
#include "fonts/InterMed18.h"
#include "fonts/InterSemi12.h"
#include "fonts/Play160Digits.h"
#include "fonts/Play18.h"
#include "fonts/Play26.h"
#include "fonts/Play46.h"
#include "text.h"

#include <qrcode.h>

// Visual language of the "Sabah Brifingi" artifact, translated to 1-bit
// e-ink: Playfair Display for titles, numbers and times; Inter for body text;
// letter-spaced uppercase labels; a 2px rule under the masthead, 1px rules
// under section headings and dotted hairlines (the artifact's light gray)
// between rows; round check marks; a status bar at the bottom.
namespace ui {
namespace {

constexpr int16_t kW = 528;
constexpr int16_t kH = 792;
constexpr int16_t kM = 28;  // side margin
constexpr int16_t kBarTop = 726;
constexpr int16_t kTimeColW = 112;

struct Encoded {
  char s[200];
  explicit Encoded(const char* utf8, bool upper = false) {
    utf8ToFont(utf8, s, sizeof s);
    if (upper) upperFont(s);
  }
};

uint16_t widthOf(Adafruit_GFX& g, const GFXfont* f, const char* enc) {
  g.setFont(f);
  int16_t bx, by;
  uint16_t bw, bh;
  g.getTextBounds(enc, 0, 100, &bx, &by, &bw, &bh);
  return bw + (bx > 0 ? bx : 0);
}

void drawEnc(Adafruit_GFX& g, const GFXfont* f, const char* enc, int16_t x, int16_t y, uint16_t color) {
  g.setFont(f);
  g.setTextColor(color);
  g.setCursor(x, y);
  g.print(enc);
}

// Letter-spaced text (the artifact's uppercase labels).
uint16_t trackedWidth(const GFXfont* f, const char* enc, int16_t tracking) {
  uint16_t w = 0;
  for (const auto* p = reinterpret_cast<const unsigned char*>(enc); *p; ++p) {
    if (*p < f->first || *p > f->last) continue;
    w += f->glyph[*p - f->first].xAdvance + tracking;
  }
  return w > 0 ? w - tracking : 0;
}

void drawTracked(Adafruit_GFX& g, const GFXfont* f, const char* enc, int16_t x, int16_t y, int16_t tracking,
                 uint16_t color) {
  g.setFont(f);
  g.setTextColor(color);
  for (const auto* p = reinterpret_cast<const unsigned char*>(enc); *p; ++p) {
    if (*p < f->first || *p > f->last) continue;
    g.setCursor(x, y);
    g.write(*p);
    x += f->glyph[*p - f->first].xAdvance + tracking;
  }
}

// Uppercase, letter-spaced label. align: 0 left, 1 center, 2 right (x = edge).
void label(Adafruit_GFX& g, const char* utf8, int16_t x, int16_t y, int align = 0, uint16_t color = 1) {
  Encoded e(utf8, true);
  const uint16_t w = trackedWidth(&InterSemi12, e.s, 2);
  if (align == 1) x = (kW - w) / 2;
  if (align == 2) x -= w;
  drawTracked(g, &InterSemi12, e.s, x, y, 2, color);
}

// Truncates an encoded string in place with "..." to fit maxW.
void fitWidth(Adafruit_GFX& g, const GFXfont* f, char* enc, uint16_t maxW) {
  if (widthOf(g, f, enc) <= maxW) return;
  size_t n = strlen(enc);
  while (n > 0) {
    enc[--n] = '\0';
    char tmp[210];
    snprintf(tmp, sizeof tmp, "%s...", enc);
    if (widthOf(g, f, tmp) <= maxW) {
      strcpy(enc, tmp);
      return;
    }
  }
}

void text(Adafruit_GFX& g, const GFXfont* f, const char* utf8, int16_t x, int16_t y, uint16_t maxW = 0,
          uint16_t color = 1) {
  Encoded e(utf8);
  if (maxW) fitWidth(g, f, e.s, maxW);
  drawEnc(g, f, e.s, x, y, color);
}

void textCentered(Adafruit_GFX& g, const GFXfont* f, const char* utf8, int16_t y, uint16_t color = 1) {
  Encoded e(utf8);
  drawEnc(g, f, e.s, (kW - widthOf(g, f, e.s)) / 2, y, color);
}

// Word-wraps into at most maxLines lines starting at (x, y); returns lines used.
int wrap(Adafruit_GFX& g, const GFXfont* f, const char* utf8, int16_t x, int16_t y, int16_t lineH, int maxLines,
         uint16_t maxW, bool center = false) {
  Encoded e(utf8);
  const char* p = e.s;
  int line = 0;
  for (; line < maxLines && *p; ++line) {
    char buf[210] = "";
    const char* best = nullptr;
    const char* q = p;
    while (true) {
      const char* wordEnd = q;
      while (*wordEnd && *wordEnd != ' ') ++wordEnd;
      char trial[210];
      snprintf(trial, sizeof trial, "%.*s", static_cast<int>(wordEnd - p), p);
      if (widthOf(g, f, trial) > maxW && best) break;
      best = wordEnd;
      strcpy(buf, trial);
      if (!*wordEnd) break;
      q = wordEnd + 1;
    }
    p = best;
    while (*p == ' ') ++p;
    if (line == maxLines - 1 && *p) {
      char rest[420];
      snprintf(rest, sizeof rest, "%s %s", buf, p);
      strncpy(buf, rest, sizeof buf - 1);
      buf[sizeof buf - 1] = '\0';
    }
    fitWidth(g, f, buf, maxW);
    const int16_t lx = center ? (kW - widthOf(g, f, buf)) / 2 : x;
    drawEnc(g, f, buf, lx, y + line * lineH, 1);
  }
  return line;
}

// The artifact's light-gray hairline, as a dotted line.
void hairline(Adafruit_GFX& g, int16_t x, int16_t y, int16_t w) {
  for (int16_t i = 0; i < w; i += 3) g.drawPixel(x + i, y, 1);
}

void vHairline(Adafruit_GFX& g, int16_t x, int16_t y, int16_t h) {
  for (int16_t i = 0; i < h; i += 3) g.drawPixel(x, y + i, 1);
}

// Section heading: uppercase label + 1px rule.
void sectionHeading(Adafruit_GFX& g, const char* utf8, int16_t y) {
  label(g, utf8, kM, y);
  g.drawFastHLine(kM, y + 7, kW - 2 * kM, 1);
}

// Big number with an uppercase caption under it, right-aligned at `right`.
int16_t countCell(Adafruit_GFX& g, const char* value, const char* caption, int16_t right) {
  Encoded v(value);
  Encoded c(caption, true);
  const uint16_t vw = widthOf(g, &Play26, v.s);
  const uint16_t cw = trackedWidth(&InterSemi12, c.s, 2);
  const uint16_t w = vw > cw ? vw : cw;
  const int16_t x = right - w;
  drawEnc(g, &Play26, v.s, x, 62, 1);
  drawTracked(g, &InterSemi12, c.s, x, 82, 2, 1);
  return x;
}

// Masthead: eyebrow, serif title, counts on the right, 2px rule, dateline.
void masthead(Adafruit_GFX& g, const App& app, const char* eyebrow, const char* title, bool showCounts) {
  label(g, eyebrow, kM, 26);

  int16_t countsLeft = kW - kM;
  if (showCounts) {
    if (app.batteryPercent >= 0) {
      char v[8];
      snprintf(v, sizeof v, "%%%d", app.batteryPercent);
      countsLeft = countCell(g, v, app.charging ? "şarj" : "pil", countsLeft) - 22;
    }
    char v[16];
    snprintf(v, sizeof v, "%d/%d", app.tasks.doneCount(), app.tasks.count);
    countsLeft = countCell(g, v, "tamam", countsLeft) - 18;
  }
  text(g, &Play46, title, kM - 2, 82, countsLeft - kM);

  g.fillRect(kM, 98, kW - 2 * kM, 2, 1);

  // Dateline: date in caps, then the subtitle.
  char date[64];
  if (app.dateText[0]) {
    Encoded d(app.dateText, true);
    snprintf(date, sizeof date, "%s", d.s);
  } else {
    Encoded d("Tarih ayarlı değil", true);
    snprintf(date, sizeof date, "%s", d.s);
  }
  drawTracked(g, &InterSemi12, date, kM, 124, 1, 1);
  if (app.tasks.subtitle[0]) {
    const int16_t x = kM + trackedWidth(&InterSemi12, date, 1) + 14;
    text(g, &Inter14, app.tasks.subtitle, x, 124, kW - kM - x);
  }
}

// Bottom status bar: a hairline, two uppercase labels and an optional hint line.
void statusBar(Adafruit_GFX& g, const char* left, const char* right, const char* hint) {
  g.drawFastHLine(0, kBarTop, kW, 1);
  label(g, left, kM, kBarTop + 28);
  label(g, right, kW - kM, kBarTop + 28, 2);
  if (hint) textCentered(g, &Inter14, hint, kBarTop + 54);
}

void indicatorGrid(Adafruit_GFX& g, const TaskList& t) {
  sectionHeading(g, "Göstergeler", 166);
  const int16_t top = 174, bottom = 284;
  const int16_t cellW = (kW - 2 * kM) / t.kMaxIndicators;
  for (int i = 0; i < t.indicatorCount; ++i) {
    const Indicator& ind = t.indicators[i];
    const int16_t x = kM + i * cellW;
    const int16_t pad = i == 0 ? 0 : 12;
    const uint16_t innerW = cellW - pad - 8;
    if (i > 0) vHairline(g, x, top + 4, bottom - top - 8);
    {
      Encoded l(ind.label, true);
      fitWidth(g, &InterSemi12, l.s, innerW);
      drawTracked(g, &InterSemi12, l.s, x + pad, top + 22, 2, 1);
    }
    {
      // Serif value; drop to the smaller size before truncating.
      Encoded v(ind.value);
      const GFXfont* vf = widthOf(g, &Play26, v.s) <= innerW ? &Play26 : &Play18;
      fitWidth(g, vf, v.s, innerW);
      drawEnc(g, vf, v.s, x + pad, top + 56, 1);
    }
    wrap(g, &Inter14, ind.detail, x + pad, top + 78, 18, 2, innerW);
  }
  hairline(g, kM, bottom, kW - 2 * kM);
}

void checkCircle(Adafruit_GFX& g, int16_t cx, int16_t cy, bool done, uint16_t ink) {
  g.drawCircle(cx, cy, 9, ink);
  if (done) g.fillCircle(cx, cy, 4, ink);
}

const char* weekdayOf(const App& app, char* buf, size_t n) {
  // dateText is "Salı, 10 Kasım": the weekday is everything before the comma.
  const char* comma = strchr(app.dateText, ',');
  if (!comma) return "";
  snprintf(buf, n, "%.*s", static_cast<int>(comma - app.dateText), app.dateText);
  return buf;
}

}  // namespace

void drawList(Adafruit_GFX& g, const App& app, bool sleeping) {
  g.fillScreen(0);
  const TaskList& t = app.tasks;

  masthead(g, app, t.eyebrow[0] ? t.eyebrow : "Günlük brifing", t.title[0] ? t.title : "Bugün", true);
  if (t.indicatorCount > 0) indicatorGrid(g, t);

  if (t.count == 0) {
    textCentered(g, &Play26, "Görev yok", 400);
    textCentered(g, &Inter14, "SD kartın köküne brifing.txt koy:", 440);
    textCentered(g, &Inter14, "her satıra bir görev.", 460);
  }

  bool anyTime = false;
  for (int i = 0; i < t.count; ++i) anyTime |= t.items[i].time[0] != '\0';

  int16_t y = app.listTop();
  const int shown = app.rowsFrom(app.scroll);
  const int last = app.scroll + shown < t.count ? app.scroll + shown : t.count;
  if (app.scroll > 0) label(g, "yukarıda daha var", kW - kM, y - 4, 2);
  for (int i = app.scroll; i < last; ++i) {
    const Task& task = t.items[i];
    if (app.needsHeading(i, app.scroll)) {
      sectionHeading(g, t.sections[task.section], y + 26);
      y += App::kSectionH;
    }
    const bool sel = !sleeping && i == app.selected;
    const uint16_t ink = sel ? 0 : 1;
    if (sel) g.fillRect(kM - 10, y + 1, kW - 2 * kM + 20, App::kRowH - 2, 1);

    checkCircle(g, kM + 9, y + App::kRowH / 2, task.done, ink);
    int16_t tx = kM + 30;
    if (anyTime) {
      if (task.time[0]) text(g, &Play18, task.time, tx, y + 29, kTimeColW - 8, ink);
      tx += kTimeColW;
    }
    Encoded e(task.text);
    fitWidth(g, &InterMed18, e.s, kW - kM - tx);
    drawEnc(g, &InterMed18, e.s, tx, y + 29, ink);
    if (task.done) g.drawFastHLine(tx, y + 23, widthOf(g, &InterMed18, e.s), ink);
    if (!sel) hairline(g, kM, y + App::kRowH - 1, kW - 2 * kM);
    y += App::kRowH;
  }
  if (last < t.count) label(g, "aşağıda daha var", kW - kM, App::kListBottom + 4, 2);

  char wd[24];
  char done[24];
  snprintf(done, sizeof done, "%d / %d tamamlandı", t.doneCount(), t.count);
  if (sleeping) {
    g.fillRect(0, kBarTop, kW, kH - kBarTop, 1);
    label(g, "Uyuyor", kM, kBarTop + 28, 0, 0);
    label(g, done, kW - kM, kBarTop + 28, 2, 0);
    textCentered(g, &Inter14, "Açmak için GÜÇ tuşuna bas", kBarTop + 54, 0);
    return;
  }
  const char* hint = t.count == 0 ? "GERİ tazele  ·  GÜÇ uyku"
                                  : "TAMAM işaretle  ·  SAĞ odak  ·  GERİ tazele  ·  GÜÇ uyku";
  statusBar(g, done, app.sdAvailable ? weekdayOf(app, wd, sizeof wd) : "SD kart yok", hint);
}

void drawFocus(Adafruit_GFX& g, const App& app, uint32_t nowMs) {
  g.fillScreen(0);
  char elapsed[16];
  snprintf(elapsed, sizeof elapsed, "%lu", static_cast<unsigned long>(app.focusElapsed(nowMs) / 60000));
  masthead(g, app, "Odak seansı · 25 dk", "Odak", false);
  countCell(g, elapsed, "dk geçti", kW - kM);

  sectionHeading(g, "Şu anki görev", 166);
  if (app.focusTask >= 0 && app.focusTask < app.tasks.count) {
    const Task& task = app.tasks.items[app.focusTask];
    int16_t y = 200;
    if (task.time[0]) {
      text(g, &Play18, task.time, kM, y);
      y += 28;
    }
    wrap(g, &InterMed18, task.text, kM, y, 26, 2, kW - 2 * kM);
  }

  char minutes[8];
  snprintf(minutes, sizeof minutes, "%d", app.focusMinutesLeft(nowMs));
  g.setFont(&Play160Digits);
  g.setTextColor(1);
  g.setCursor((kW - widthOf(g, &Play160Digits, minutes)) / 2, 470);
  g.print(minutes);
  label(g, "dakika kaldı", 0, 540, 1);

  const int total = static_cast<int>(App::kFocusMs / 1000);
  const int done = static_cast<int>(app.focusElapsed(nowMs) / 1000);
  const int16_t bx = kM + 40, bw = kW - 2 * kM - 80;
  g.drawRect(bx, 572, bw, 8, 1);
  if (done > 0) g.fillRect(bx, 572, static_cast<int16_t>(static_cast<int32_t>(bw) * done / total), 8, 1);

  if (app.focusPaused) {
    // The artifact's pill, as an outlined capsule.
    Encoded p("Duraklatıldı", true);
    const uint16_t w = trackedWidth(&InterSemi12, p.s, 2) + 24;
    g.drawRoundRect((kW - w) / 2, 610, w, 26, 13, 1);
    drawTracked(g, &InterSemi12, p.s, (kW - w) / 2 + 12, 628, 2, 1);
  }

  statusBar(g, app.focusPaused ? "Tamam · devam" : "Tamam · duraklat", "Geri · iptal", nullptr);
}

void drawFocusDone(Adafruit_GFX& g, const App& app) {
  g.fillScreen(0);
  masthead(g, app, "Odak seansı bitti", "Süre doldu", false);
  countCell(g, "25", "dk odak", kW - kM);

  sectionHeading(g, "Görev", 166);
  if (app.focusTask >= 0 && app.focusTask < app.tasks.count) {
    const Task& task = app.tasks.items[app.focusTask];
    int16_t y = 200;
    if (task.time[0]) {
      text(g, &Play18, task.time, kM, y);
      y += 28;
    }
    wrap(g, &InterMed18, task.text, kM, y, 26, 3, kW - 2 * kM);
  }
  textCentered(g, &Play26, "Bu görevi bitirdin mi?", 420);
  statusBar(g, "Tamam · evet, işaretle", "Geri · hayır", nullptr);
}

void drawWifi(Adafruit_GFX& g, const App& app) {
  g.fillScreen(0);
  masthead(g, app, "WI-FI aktarım", "Bağlan", false);

  sectionHeading(g, "1 · Telefon kamerasıyla okut", 166);
  // Standard Wi-Fi QR payload: phones offer to join the network directly.
  char payload[96];
  snprintf(payload, sizeof payload, "WIFI:T:WPA;S:%s;P:%s;;", app.wifiSsid, app.wifiPass);
  QRCode qr;
  uint8_t qrData[qrcode_getBufferSize(4)];
  if (qrcode_initText(&qr, qrData, 4, ECC_LOW, payload) == 0) {
    const int16_t scale = 5;
    const int16_t size = qr.size * scale;
    const int16_t x0 = (kW - size) / 2, y0 = 186;
    for (uint8_t y = 0; y < qr.size; ++y)
      for (uint8_t x = 0; x < qr.size; ++x)
        if (qrcode_getModule(&qr, x, y)) g.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, 1);
  }

  sectionHeading(g, "2 · Ya da elle bağlan", 392);
  label(g, "Ağ", kM, 428);
  text(g, &Play26, app.wifiSsid, kM + 110, 432);
  hairline(g, kM, 446, kW - 2 * kM);
  label(g, "Şifre", kM, 476);
  text(g, &Play26, app.wifiPass, kM + 110, 480);

  sectionHeading(g, "3 · Tarayıcıda aç", 532);
  text(g, &Play26, app.wifiUrl, kM, 576);
  text(g, &Inter14, "Sayfa açılınca cihazın saati de ayarlanır.", kM, 602);

  if (app.wifiStatus[0]) {
    Encoded e(app.wifiStatus, true);
    fitWidth(g, &InterSemi12, e.s, kW - 2 * kM - 30);
    const uint16_t w = trackedWidth(&InterSemi12, e.s, 2) + 24;
    g.drawRoundRect((kW - w) / 2, 650, w, 26, 13, 1);
    drawTracked(g, &InterSemi12, e.s, (kW - w) / 2 + 12, 668, 2, 1);
  }

  statusBar(g, "Geri · çık", "WI-FI açık", "10 dk kullanılmazsa kapanır");
}

}  // namespace ui
