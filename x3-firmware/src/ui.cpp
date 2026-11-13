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
void masthead(Adafruit_GFX& g, const App& app, const char* eyebrow, const char* title, bool showCounts,
              bool showSubtitle = true, const char* dateline = nullptr) {
  label(g, eyebrow, kM, 26);

  int16_t countsLeft = kW - kM;
  if (showCounts) {
    if (app.batteryPercent >= 0) {
      char v[8];
      snprintf(v, sizeof v, "%%%d", app.batteryPercent);
      countsLeft = countCell(g, v, app.charging ? "şarj" : "pil", countsLeft) - 22;
    }
    char v[16];
    snprintf(v, sizeof v, "%d/%d", app.tasks.doneCount(), app.tasks.activeCount());
    countsLeft = countCell(g, v, "tamam", countsLeft) - 18;
  }
  text(g, &Play46, title, kM - 2, 82, countsLeft - kM);

  g.fillRect(kM, 98, kW - 2 * kM, 2, 1);

  // Dateline: date in caps, then the subtitle.
  char date[64];
  if (dateline != nullptr && dateline[0]) {
    Encoded d(dateline, true);
    snprintf(date, sizeof date, "%s", d.s);
  } else if (app.dateText[0]) {
    Encoded d(app.dateText, true);
    snprintf(date, sizeof date, "%s", d.s);
  } else {
    Encoded d("Tarih ayarlı değil", true);
    snprintf(date, sizeof date, "%s", d.s);
  }
  drawTracked(g, &InterSemi12, date, kM, 124, 1, 1);
  if (showSubtitle && app.tasks.subtitle[0]) {
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

void checkCircle(Adafruit_GFX& g, int16_t cx, int16_t cy, bool done, uint16_t ink, bool cancelled = false) {
  g.drawCircle(cx, cy, 9, ink);
  if (done) g.fillCircle(cx, cy, 4, ink);
  if (cancelled) {  // an "x" inside the ring
    g.drawLine(cx - 4, cy - 4, cx + 4, cy + 4, ink);
    g.drawLine(cx - 4, cy + 4, cx + 4, cy - 4, ink);
    g.drawLine(cx - 3, cy - 4, cx + 4, cy + 3, ink);
    g.drawLine(cx - 4, cy + 3, cx + 3, cy - 4, ink);
  }
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

  // The title is today's date ("13 Kasım") with the weekday on the dateline;
  // the briefing's own title is only the fallback while the clock is unset.
  const char* comma = strchr(app.dateText, ',');
  if (comma != nullptr) {
    const char* dayMonth = comma + 1;
    while (*dayMonth == ' ') ++dayMonth;
    char wd[24];
    masthead(g, app, t.eyebrow[0] ? t.eyebrow : "Günlük brifing", dayMonth, true, true, weekdayOf(app, wd, sizeof wd));
  } else {
    masthead(g, app, t.eyebrow[0] ? t.eyebrow : "Günlük brifing", t.title[0] ? t.title : "Bugün", true);
  }
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

    checkCircle(g, kM + 9, y + App::kRowH / 2, task.done, ink, task.cancelled);
    int16_t tx = kM + 30;
    if (anyTime) {
      if (task.time[0]) text(g, &Play18, task.time, tx, y + 29, kTimeColW - 8, ink);
      tx += kTimeColW;
    }
    Encoded e(task.text);
    fitWidth(g, &InterMed18, e.s, kW - kM - tx);
    drawEnc(g, &InterMed18, e.s, tx, y + 29, ink);
    if (task.done || task.cancelled) g.drawFastHLine(tx, y + 23, widthOf(g, &InterMed18, e.s), ink);
    if (task.cancelled) g.drawFastHLine(tx, y + 24, widthOf(g, &InterMed18, e.s), ink);  // bolder: cancelled
    if (!sel) hairline(g, kM, y + App::kRowH - 1, kW - 2 * kM);
    y += App::kRowH;
  }
  if (last < t.count) label(g, "aşağıda daha var", kW - kM, App::kListBottom + 4, 2);

  char wd[24];
  char done[24];
  snprintf(done, sizeof done, "%d / %d tamamlandı", t.doneCount(), t.activeCount());
  if (sleeping) {
    g.fillRect(0, kBarTop, kW, kH - kBarTop, 1);
    label(g, "Uyuyor", kM, kBarTop + 28, 0, 0);
    label(g, done, kW - kM, kBarTop + 28, 2, 0);
    textCentered(g, &Inter14, "Açmak için GÜÇ tuşuna bas", kBarTop + 54, 0);
    return;
  }
  const char* hint = t.count == 0 ? "GERİ tazele  ·  GÜÇ uyku"
                                  : app.syncConfigured ? "TAMAM bitti/iptal  ·  SOL eşitle  ·  SAĞ odak"
                                                    : "TAMAM bitti/iptal  ·  SOL Wi-Fi  ·  SAĞ odak";
  const char* right = !app.sdAvailable ? "SD kart yok" : app.syncNote[0] ? app.syncNote : weekdayOf(app, wd, sizeof wd);
  statusBar(g, done, right, hint);
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
  // \ ; , : " must be backslash-escaped inside the S and P fields.
  auto escape = [](const char* in, char* out, size_t n) {
    size_t o = 0;
    for (; *in && o + 2 < n; ++in) {
      if (strchr("\\;,:\"", *in)) out[o++] = '\\';
      out[o++] = *in;
    }
    out[o] = '\0';
  };
  char ssid[64], pass[40], payload[128];
  escape(app.wifiSsid, ssid, sizeof ssid);
  escape(app.wifiPass, pass, sizeof pass);
  snprintf(payload, sizeof payload, "WIFI:T:WPA;S:%s;P:%s;;", ssid, pass);
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

  statusBar(g, "Geri · çık", app.syncConfigured ? "Tamam · ev ağıyla eşitle" : "WI-FI açık",
            app.syncConfigured ? "Eşitleme ayarlı · 10 dk kullanılmazsa kapanır"
                               : "Eşitleme için sayfadaki Ayarlar · 10 dk sonra kapanır");
}

namespace {

// Draws encoded text rotated 90° counter-clockwise (reading bottom to top, as
// on Turkish and most European book spines). (x, y) is the start of the
// baseline: text runs upwards from y, glyph tops point left of x.
void drawRotatedEnc(Adafruit_GFX& g, const GFXfont* f, const char* enc, int16_t x, int16_t y, uint16_t color) {
  for (const auto* p = reinterpret_cast<const unsigned char*>(enc); *p; ++p) {
    if (*p < f->first || *p > f->last) continue;
    const GFXglyph& gl = f->glyph[*p - f->first];
    const uint8_t* bits = f->bitmap + gl.bitmapOffset;
    uint16_t bit = 0;
    for (int gy = 0; gy < gl.height; ++gy) {
      for (int gx = 0; gx < gl.width; ++gx, ++bit) {
        if (!(bits[bit >> 3] & (0x80 >> (bit & 7)))) continue;
        const int tx = gl.xOffset + gx;  // along the text
        const int ty = gl.yOffset + gy;  // from the baseline (negative = up)
        g.drawPixel(x + ty, y - tx, color);
      }
    }
    y -= gl.xAdvance;
  }
}

uint16_t advanceWidth(const GFXfont* f, const char* enc) {
  uint16_t w = 0;
  for (const auto* p = reinterpret_cast<const unsigned char*>(enc); *p; ++p)
    if (*p >= f->first && *p <= f->last) w += f->glyph[*p - f->first].xAdvance;
  return w;
}

// Truncates by advance width (rotated text cannot use getTextBounds).
void fitAdvance(const GFXfont* f, char* enc, uint16_t maxW) {
  if (advanceWidth(f, enc) <= maxW) return;
  size_t n = strlen(enc);
  while (n > 0) {
    enc[--n] = '\0';
    char tmp[210];
    snprintf(tmp, sizeof tmp, "%s...", enc);
    if (advanceWidth(f, tmp) <= maxW) {
      strcpy(enc, tmp);
      return;
    }
  }
}

const char* const kMonthAbbr[] = {"Oca", "Şub", "Mar", "Nis", "May", "Haz", "Tem", "Ağu", "Eyl", "Eki", "Kas", "Ara"};

void formatStamp(const DateStamp& d, char* out, size_t n) {
  if (!d.known()) {
    snprintf(out, n, "–");
    return;
  }
  snprintf(out, n, "%u %s %u", d.day, kMonthAbbr[(d.month + 11) % 12], d.year);
}

void spine(Adafruit_GFX& g, const BookInfo& b, const SpineBox& box, bool selected) {
  const uint16_t ink = selected ? 0 : 1;
  if (selected) {
    g.fillRect(box.x, box.y, box.w, box.h, 1);
  } else {
    g.drawRect(box.x, box.y, box.w, box.h, 1);
    g.drawRect(box.x + 1, box.y + 1, box.w - 2, box.h - 2, 1);
  }
  // Classic spine bands near the head and the foot.
  g.drawFastHLine(box.x + 4, box.y + 10, box.w - 8, ink);
  g.drawFastHLine(box.x + 4, box.y + 13, box.w - 8, ink);
  g.drawFastHLine(box.x + 4, box.y + box.h - 14, box.w - 8, ink);
  g.drawFastHLine(box.x + 4, box.y + box.h - 11, box.w - 8, ink);
  // Bookmark ribbon on books being read.
  if (b.lastRead != 0 && !b.finished) {
    const int16_t rx = box.x + box.w - 12;
    g.fillRect(rx, box.y - 2, 6, 16, ink == 1 ? 1 : 0);
    g.fillTriangle(rx, box.y + 14, rx + 5, box.y + 14, rx + 2, box.y + 10, selected ? 1 : 0);
  }

  // Title along the spine: serif if it fits, then sans, then two lines on a
  // thick spine, then truncated.
  const int16_t room = box.h - 32;
  Encoded e(b.title);
  const GFXfont* f = nullptr;
  int16_t capH = 0;
  if (box.w >= 34 && advanceWidth(&Play18, e.s) <= room) f = &Play18, capH = 12;
  else if (box.w >= 34 && advanceWidth(&InterMed18, e.s) <= room) f = &InterMed18, capH = 13;
  else if (advanceWidth(&Inter14, e.s) <= room || box.w < 44) f = &Inter14, capH = 10;
  if (f != nullptr) {
    fitAdvance(f, e.s, room);
    const int16_t len = advanceWidth(f, e.s);
    drawRotatedEnc(g, f, e.s, box.x + box.w / 2 + capH / 2, box.y + 16 + (room + len) / 2, ink);
    return;
  }
  // Two lines: split at the space closest to the middle.
  const size_t n = strlen(e.s);
  size_t cut = 0;
  for (size_t i = 1; i < n; ++i)
    if (e.s[i] == ' ' && (cut == 0 || (i > n / 2 ? i - n / 2 : n / 2 - i) < (cut > n / 2 ? cut - n / 2 : n / 2 - cut)))
      cut = i;
  if (cut == 0) {  // one long word: never break it, truncate instead
    fitAdvance(&Inter14, e.s, room);
    drawRotatedEnc(g, &Inter14, e.s, box.x + box.w / 2 + 5, box.y + 16 + (room + advanceWidth(&Inter14, e.s)) / 2, ink);
    return;
  }
  char a[210], c[210];
  snprintf(a, sizeof a, "%.*s", static_cast<int>(cut), e.s);
  snprintf(c, sizeof c, "%s", e.s + cut + (e.s[cut] == ' ' ? 1 : 0));
  fitAdvance(&Inter14, a, room);
  fitAdvance(&Inter14, c, room);
  const int16_t mid = box.x + box.w / 2;
  drawRotatedEnc(g, &Inter14, a, mid - 2, box.y + 16 + (room + advanceWidth(&Inter14, a)) / 2, ink);
  drawRotatedEnc(g, &Inter14, c, mid + 14, box.y + 16 + (room + advanceWidth(&Inter14, c)) / 2, ink);
}

void addSpine(Adafruit_GFX& g, const SpineBox& box, bool selected) {
  const uint16_t ink = selected ? 0 : 1;
  if (selected) {
    g.fillRect(box.x, box.y, box.w, box.h, 1);
  } else {
    for (int16_t i = 0; i < box.h; i += 6) {  // dashed outline
      g.drawFastVLine(box.x, box.y + i, 3, 1);
      g.drawFastVLine(box.x + box.w - 1, box.y + i, 3, 1);
    }
    for (int16_t i = 0; i < box.w; i += 6) {
      g.drawFastHLine(box.x + i, box.y, 3, 1);
      g.drawFastHLine(box.x + i, box.y + box.h - 1, 3, 1);
    }
  }
  Encoded e("+ Kitap ekle", true);
  const int16_t len = trackedWidth(&InterSemi12, e.s, 2);
  // Tracked rotated label: draw char by char.
  int16_t y = box.y + (box.h + len) / 2;
  const int16_t x = box.x + box.w / 2 + 4;
  for (const auto* p = reinterpret_cast<const unsigned char*>(e.s); *p; ++p) {
    char one[2] = {static_cast<char>(*p), 0};
    drawRotatedEnc(g, &InterSemi12, one, x, y, ink);
    if (*p >= InterSemi12.first && *p <= InterSemi12.last) y -= InterSemi12.glyph[*p - InterSemi12.first].xAdvance + 2;
  }
}

}  // namespace

void drawLibrary(Adafruit_GFX& g, const App& app) {
  g.fillScreen(0);
  const Library& lib = app.lib;
  const BookInfo* reading[Library::kMax];
  const int readingCount = lib.inProgress(reading, Library::kMax);

  char eyebrow[48];
  snprintf(eyebrow, sizeof eyebrow, "GK'nin kitaplığı · %d okunuyor", readingCount);
  masthead(g, app, eyebrow, "Kitaplık", false, false);
  {
    char v[8];
    int16_t left = kW - kM;
    if (app.batteryPercent >= 0) {
      snprintf(v, sizeof v, "%%%d", app.batteryPercent);
      left = countCell(g, v, app.charging ? "şarj" : "pil", left) - 22;
    }
    snprintf(v, sizeof v, "%d", lib.count);
    countCell(g, v, "kitap", left);
  }

  SpineBox boxes[Library::kMax + 1];
  ShelfLayout::place(lib, boxes, Library::kMax + 1);
  const int items = lib.count + 1;
  int lastShelf = 0;
  for (int i = 0; i < items; ++i) lastShelf = boxes[i].shelf > lastShelf ? boxes[i].shelf : lastShelf;
  for (int s = 0; s < ShelfLayout::kShelves; ++s) {
    // Board under every visible shelf, even an empty one: it reads as a bookcase.
    const int16_t boardY = ShelfLayout::kTop + s * ShelfLayout::kShelfH + ShelfLayout::kMaxSpineH;
    g.fillRect(ShelfLayout::kLeft - 10, boardY, ShelfLayout::kRight - ShelfLayout::kLeft + 20, ShelfLayout::kBoard, 1);
    hairline(g, ShelfLayout::kLeft - 10, boardY + ShelfLayout::kBoard + 2, ShelfLayout::kRight - ShelfLayout::kLeft + 20);
  }
  for (int i = 0; i < items; ++i) {
    const int s = boxes[i].shelf - app.shelfScroll;
    if (s < 0 || s >= ShelfLayout::kShelves) continue;
    SpineBox b = boxes[i];
    b.y = static_cast<int16_t>(b.y + (s - boxes[i].shelf % ShelfLayout::kShelves) * ShelfLayout::kShelfH);
    const bool sel = i == app.libSelected;
    if (i == lib.count) addSpine(g, b, sel);
    else spine(g, lib.books[i], b, sel);
  }
  if (app.shelfScroll > 0) label(g, "yukarıda daha var", kW - kM, ShelfLayout::kTop - 6, 2);
  if (lastShelf >= app.shelfScroll + ShelfLayout::kShelves) {
    label(g, "aşağıda daha var", kW - kM, ShelfLayout::kTop + ShelfLayout::kShelves * ShelfLayout::kShelfH - 2, 2);
  }

  // Selected book details.
  const int16_t dy = ShelfLayout::kTop + ShelfLayout::kShelves * ShelfLayout::kShelfH + 6;
  if (app.libSelected < lib.count) {
    const BookInfo& b = lib.books[app.libSelected];
    sectionHeading(g, "Seçili kitap", dy + 10);
    text(g, &Play26, b.title, kM, dy + 44, kW - 2 * kM);
    char line[160];
    char stamp[24];
    formatStamp(b.started, stamp, sizeof stamp);
    if (app.libStatus[0]) snprintf(line, sizeof line, "%s", app.libStatus);  // e.g. why it did not open
    else if (b.finished) snprintf(line, sizeof line, "%s%sbitti", b.author, b.author[0] ? " · " : "");
    else if (b.lastRead) snprintf(line, sizeof line, "%s%s%%%u · başladı %s", b.author, b.author[0] ? " · " : "", b.percent, stamp);
    else snprintf(line, sizeof line, "%s%shenüz açılmadı", b.author, b.author[0] ? " · " : "");
    text(g, app.libStatus[0] ? &InterMed18 : &Inter14, line, kM, dy + 68, kW - 2 * kM);
  } else {
    sectionHeading(g, "Kitap ekle", dy + 10);
    text(g, &Inter14, "TAMAM: Wi-Fi'ı açar; telefondan EPUB ya da TXT yükle.", kM, dy + 40, kW - 2 * kM);
    if (lib.count == 0) text(g, &Inter14, "Raflar şimdilik boş.", kM, dy + 60, kW - 2 * kM);
  }
  if (app.libStatus[0] && app.libSelected >= lib.count) text(g, &InterMed18, app.libStatus, kM, dy + 84, kW - 2 * kM);

  char left[24];
  snprintf(left, sizeof left, "%d kitap", lib.count);
  char wd[24];
  statusBar(g, left, weekdayOf(app, wd, sizeof wd), "SOL/SAĞ kitap  ·  YUKARI/AŞAĞI raf  ·  TAMAM oku  ·  GERİ brifing");
}

void drawLendingCard(Adafruit_GFX& g, const App& app) {
  g.fillScreen(0);
  // The card: double rule frame with a punched hole at the top.
  const int16_t cx = 20, cy = 20, cw = kW - 40, ch = kH - 92;
  g.drawRect(cx, cy, cw, ch, 1);
  g.drawRect(cx + 4, cy + 4, cw - 8, ch - 8, 1);
  g.fillCircle(kW / 2, cy + 22, 7, 1);
  g.fillCircle(kW / 2, cy + 22, 4, 0);

  label(g, "Kütüphane ödünç kartı", 0, cy + 58, 1);
  textCentered(g, &Play46, "GK'nin Kitaplığı", cy + 112);
  g.fillRect(cx + 24, cy + 128, cw - 48, 2, 1);

  // Columns: book | started | %.
  const int16_t x0 = cx + 24, x1 = cx + cw - 196, x2 = cx + cw - 76, xEnd = cx + cw - 24;
  const int16_t headY = cy + 156;
  label(g, "Kitap", x0, headY);
  label(g, "Başladı", x1 + 10, headY);
  label(g, "%", xEnd, headY, 2);
  g.drawFastHLine(x0, headY + 10, xEnd - x0, 1);

  const BookInfo* reading[8];
  const int n = app.lib.inProgress(reading, 8);
  const int16_t rowH = 58, top = headY + 10;
  const int rows = (cy + ch - 70 - top) / rowH;
  g.drawFastVLine(x1, headY - 16, rows * rowH + 26, 1);
  g.drawFastVLine(x2, headY - 16, rows * rowH + 26, 1);
  for (int r = 0; r < rows; ++r) {
    const int16_t y = top + r * rowH;
    g.drawFastHLine(x0, y + rowH, xEnd - x0, 1);  // ruled card lines
    if (r >= n) continue;
    const BookInfo& b = *reading[r];
    text(g, &InterMed18, b.title, x0, y + 28, x1 - x0 - 10);
    if (b.author[0]) text(g, &Inter14, b.author, x0, y + 48, x1 - x0 - 10);
    char stamp[24];
    formatStamp(b.started, stamp, sizeof stamp);
    // Date as a rubber stamp: serif in a thin box.
    Encoded st(stamp);
    const uint16_t sw = widthOf(g, &Play18, st.s);
    const int16_t sx = x1 + (x2 - x1 - sw) / 2;
    g.drawRoundRect(sx - 6, y + 14, sw + 12, 28, 4, 1);
    drawEnc(g, &Play18, st.s, sx, y + 34, 1);
    char pct[8];
    snprintf(pct, sizeof pct, "%u", b.percent);
    Encoded pe(pct);
    drawEnc(g, &Play26, pe.s, xEnd - widthOf(g, &Play26, pe.s), y + 38, 1);
  }
  if (n == 0) textCentered(g, &Inter14, "Şu an okunan kitap yok.", top + 34);

  g.fillRect(0, kBarTop, kW, kH - kBarTop, 1);
  label(g, "Uyuyor", kM, kBarTop + 28, 0, 0);
  char wd[24];
  label(g, app.dateText[0] ? app.dateText : weekdayOf(app, wd, sizeof wd), kW - kM, kBarTop + 28, 2, 0);
  textCentered(g, &Inter14, "Açmak için GÜÇ tuşuna bas", kBarTop + 54, 0);
}

void drawReaderFooter(Adafruit_GFX& g, const char* chapter, uint8_t percent, uint32_t page, uint32_t pages,
                      bool turkish) {
  const int16_t y = kH - 40;
  g.fillRect(0, y, kW, 40, 0);
  hairline(g, kM, y + 4, kW - 2 * kM);
  char right[32];
  snprintf(right, sizeof right, "%lu/%lu · %%%u", static_cast<unsigned long>(page), static_cast<unsigned long>(pages),
           percent);
  Encoded r(right, true);
  const uint16_t rw = trackedWidth(&InterSemi12, r.s, 2);
  drawTracked(g, &InterSemi12, r.s, kW - kM - rw, y + 28, 2, 1);
  Encoded c(chapter[0] ? chapter : "");
  upperFont(c.s, turkish);
  fitWidth(g, &InterSemi12, c.s, kW - 2 * kM - rw - 40);
  drawTracked(g, &InterSemi12, c.s, kM, y + 28, 2, 1);
}

void drawReaderMessage(Adafruit_GFX& g, const App& app, const char* title, const char* message) {
  g.fillScreen(0);
  masthead(g, app, "Kitaplık", "Okuyucu", false, false);
  textCentered(g, &Play26, title, 380);
  textCentered(g, &Inter14, message, 414);
}

}  // namespace ui
