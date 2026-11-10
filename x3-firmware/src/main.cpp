// Xteink X3 — daily briefing + 25-minute focus timer.
//
// The briefing comes from /brifing.txt on the SD card (created with a sample
// if missing) or from the Wi-Fi transfer page. Hardware glue only: the app
// logic lives in app.cpp, drawing in ui.cpp. Button layout: docs-hardware.md.

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <BatteryMonitor.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <InputManager.h>
#include <PowerManager.h>
#include <Rtc.h>
#include <SDCardManager.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <XteinkDetect.h>
#include <driver/gpio.h>
#include <esp_random.h>

#include "app.h"
#include "clock.h"
#include "ui.h"
#include "webpage.h"

namespace {

constexpr int PIN_SCLK = 8;
constexpr int PIN_MOSI = 10;
constexpr int PIN_MISO = 7;
constexpr int PIN_CS = 21;
constexpr int PIN_DC = 4;
constexpr int PIN_RST = 5;
constexpr int PIN_BUSY = 6;
// GPIO13 gates the battery/SD power path on the C3 Xteink boards: HIGH keeps
// the device on, LOW (held through sleep) powers it off.
constexpr gpio_num_t PIN_POWER_LATCH = GPIO_NUM_13;

// /brifing.txt is preferred; /gorevler.txt is the older name.
constexpr const char* kBriefingPath = "/brifing.txt";
constexpr const char* kLegacyTasksPath = "/gorevler.txt";
const char* tasksPath = kBriefingPath;  // the file we loaded and write back to
constexpr const char* kSetTimePath = "/saat.txt";  // "10.11.2026 15:30": sets the RTC once
constexpr uint32_t kBatteryPollMs = 30000;
constexpr int kFastRefreshesBeforeFull = 20;
constexpr const char* kHotspotName = "X3-Brifing";
constexpr size_t kMaxUpload = 8000;

EInkDisplay epd(PIN_SCLK, PIN_MOSI, PIN_CS, PIN_DC, PIN_RST, PIN_BUSY);
InputManager input;
freeink::Rtc rtc;
App app;
int fastRefreshes = 0;
bool waitingForRelease = true;  // ignore the press that woke the device
WebServer server(80);
bool wifiOn = false;
bool webRedraw = false;  // the web page changed something on screen

// Adafruit_GFX canvas that draws straight into the SDK framebuffer
// (792x528 native, row-major, MSB-first, 1 = white).
class Canvas : public Adafruit_GFX {
 public:
  Canvas() : Adafruit_GFX(EInkDisplay::X3_DISPLAY_WIDTH, EInkDisplay::X3_DISPLAY_HEIGHT) {}

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= width() || y >= height()) return;
    int16_t px = x, py = y;
    switch (getRotation()) {
      case 1: px = WIDTH - 1 - y; py = x; break;
      case 2: px = WIDTH - 1 - x; py = HEIGHT - 1 - y; break;
      case 3: px = y; py = HEIGHT - 1 - x; break;
      default: break;
    }
    uint8_t& cell = epd.getFrameBuffer()[py * epd.getDisplayWidthBytes() + (px >> 3)];
    const uint8_t mask = 0x80 >> (px & 7);
    if (color) cell &= ~mask;  // 1 = black ink
    else cell |= mask;
  }

  void fillScreen(uint16_t color) override { epd.clearScreen(color ? 0x00 : 0xFF); }
};

Canvas screen;

void loadTasks() {
  app.sdAvailable = SdMan.begin();
  if (!app.sdAvailable) {
    Serial.println("[x3] no SD card, using sample tasks");
    app.tasks.loadSample();
    return;
  }
  tasksPath = SdMan.exists(kBriefingPath) || !SdMan.exists(kLegacyTasksPath) ? kBriefingPath : kLegacyTasksPath;
  FsFile f = SdMan.open(tasksPath, O_RDONLY);
  if (!f) {
    Serial.println("[x3] creating sample /brifing.txt");
    SdMan.writeFile(kBriefingPath, String(kSampleTasksFile));
    app.tasks.loadSample();
    return;
  }
  static char buf[8192];
  const int n = f.read(buf, sizeof buf);
  f.close();
  app.tasks.parse(buf, n > 0 ? static_cast<size_t>(n) : 0);
  Serial.printf("[x3] loaded %d tasks from %s\n", app.tasks.count, tasksPath);
}

void saveTasks() {
  if (!app.sdAvailable) return;
  static char buf[8192];
  app.tasks.serialize(buf, sizeof buf);
  if (!SdMan.writeFile(tasksPath, String(buf))) Serial.println("[x3] save failed");
}

bool setClock(const LocalTime& t) {
  if (!rtc.present()) return false;
  freeink::Rtc::DateTime dt;
  dt.year = t.year;
  dt.month = t.month;
  dt.day = t.day;
  dt.hour = t.hour;
  dt.minute = t.minute;
  dt.second = t.second;
  dt.weekday = static_cast<uint8_t>(weekdayOf(t.year, t.month, t.day));
  if (!rtc.set(dt)) return false;
  Serial.printf("[x3] clock set to %04u-%02u-%02u %02u:%02u\n", t.year, t.month, t.day, t.hour, t.minute);
  return true;
}

// If /saat.txt exists, set the RTC from it and delete it.
void applySetTimeFile() {
  if (!app.sdAvailable || !rtc.present()) return;
  FsFile f = SdMan.open(kSetTimePath, O_RDONLY);
  if (!f) return;
  char buf[64];
  const int n = f.read(buf, sizeof buf);
  f.close();
  LocalTime t;
  if (n > 0 && parseSetTime(buf, static_cast<size_t>(n), t) && setClock(t)) {
    SdMan.remove(kSetTimePath);
    return;
  }
  Serial.println("[x3] /saat.txt not understood, left in place");
}

// Refreshes the date and battery fields shown in the header.
void updateStatus() {
  app.dateText[0] = '\0';
  freeink::Rtc::DateTime dt;
  if (rtc.present() && rtc.now(dt)) {
    LocalTime t;
    t.year = dt.year;
    t.month = dt.month;
    t.day = dt.day;
    t.hour = dt.hour;
    t.minute = dt.minute;
    t.second = dt.second;
    formatTurkishDate(t, app.dateText, sizeof app.dateText);
  }

  static uint32_t lastBatteryMs = 0;
  static bool batteryRead = false;
  const uint32_t now = millis();
  if (!batteryRead || now - lastBatteryMs >= kBatteryPollMs) {
    static const BatteryMonitor battery;
    uint16_t percent = 0;
    if (battery.readPercentageChecked(percent)) app.batteryPercent = percent;
    app.charging = battery.isCharging();
    lastBatteryMs = now;
    batteryRead = true;
  }
}

// Drives every pixel to white, then the next frame paints on a clean panel.
// The X3 driver's FULL mode assumes a white previous frame, so it can leave
// ghosts; HALF drives each pixel to its target regardless of the old image.
void cleanPanel() {
  epd.clearScreen(0xFF);
  epd.displayBuffer(EInkDisplay::HALF_REFRESH);
  fastRefreshes = 0;
}

// screenChange -> HALF refresh; deepClean -> white pass first (no ghosts).
void render(bool screenChange, bool deepClean = false) {
  if (deepClean) cleanPanel();
  updateStatus();
  switch (app.screen) {
    case Screen::List: ui::drawList(screen, app, false); break;
    case Screen::Focus: ui::drawFocus(screen, app, millis()); break;
    case Screen::FocusDone: ui::drawFocusDone(screen, app); break;
    case Screen::Wifi: ui::drawWifi(screen, app); break;
  }
  if (screenChange || ++fastRefreshes >= kFastRefreshesBeforeFull) {
    fastRefreshes = 0;
    epd.displayBuffer(EInkDisplay::HALF_REFRESH);
  } else {
    epd.displayBuffer(EInkDisplay::FAST_REFRESH);
  }
}

// --- Wi-Fi transfer: hotspot + web page --------------------------------------

void handlePage() { server.send_P(200, "text/html; charset=utf-8", kTransferPage); }

void handleGetBriefing() {
  static char buf[8192];
  app.tasks.serialize(buf, sizeof buf);
  server.send(200, "text/plain; charset=utf-8", buf);
}

void handlePostBriefing() {
  const String& body = server.arg("plain");
  if (body.length() == 0 || body.length() > kMaxUpload) {
    server.send(413, "text/plain; charset=utf-8", "Dosya boş ya da çok büyük (en çok 8 KB).");
    return;
  }
  app.tasks.parse(body.c_str(), body.length());
  app.selected = 0;
  app.scroll = 0;
  tasksPath = kBriefingPath;
  saveTasks();
  char msg[96];
  snprintf(msg, sizeof msg, "Brifing alındı · %d görev", app.tasks.count);
  snprintf(app.wifiStatus, sizeof app.wifiStatus, "%s", msg);
  app.touch(millis());
  webRedraw = true;
  if (!app.sdAvailable) {
    snprintf(msg, sizeof msg, "Ekrana yüklendi (%d görev), SD kart yok: kapanınca silinir.", app.tasks.count);
  } else {
    snprintf(msg, sizeof msg, "Kaydedildi: %d görev. Cihaz ekranı yenileniyor.", app.tasks.count);
  }
  server.send(200, "text/plain; charset=utf-8", msg);
}

void handlePostTime() {
  const String& body = server.arg("plain");
  LocalTime t;
  if (!parseSetTime(body.c_str(), body.length(), t)) {
    server.send(400, "text/plain; charset=utf-8", "Saat anlaşılamadı.");
    return;
  }
  if (!setClock(t)) {
    server.send(500, "text/plain; charset=utf-8", "Saat çipine yazılamadı.");
    return;
  }
  app.touch(millis());
  webRedraw = true;  // the header date may change
  char msg[64];
  snprintf(msg, sizeof msg, "Cihaz saati ayarlandı: %02u.%02u.%04u %02u:%02u", t.day, t.month, t.year, t.hour,
           t.minute);
  server.send(200, "text/plain; charset=utf-8", msg);
}

void startWifi() {
  if (wifiOn) return;
  // A fresh 8-digit password each time the hotspot opens.
  snprintf(app.wifiPass, sizeof app.wifiPass, "%08lu", static_cast<unsigned long>(esp_random() % 100000000UL));
  snprintf(app.wifiSsid, sizeof app.wifiSsid, "%s", kHotspotName);
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(app.wifiSsid, app.wifiPass)) {
    snprintf(app.wifiStatus, sizeof app.wifiStatus, "Wi-Fi açılamadı");
    return;
  }
  snprintf(app.wifiUrl, sizeof app.wifiUrl, "http://%s", WiFi.softAPIP().toString().c_str());
  server.on("/", HTTP_GET, handlePage);
  server.on("/brifing.txt", HTTP_GET, handleGetBriefing);
  server.on("/brifing", HTTP_POST, handlePostBriefing);
  server.on("/saat", HTTP_POST, handlePostTime);
  server.onNotFound([] { server.send(404, "text/plain; charset=utf-8", "Yok"); });
  server.begin();
  wifiOn = true;
  Serial.printf("[x3] hotspot %s up at %s\n", app.wifiSsid, app.wifiUrl);
}

void stopWifi() {
  if (!wifiOn) return;
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiOn = false;
  Serial.println("[x3] hotspot off");
}

[[noreturn]] void goToSleep() {
  Serial.println("[x3] sleep");
  stopWifi();
  if (app.screen == Screen::Wifi) app.screen = Screen::List;
  // This image stays on the panel while off, so clean first: no ghosts.
  cleanPanel();
  updateStatus();
  ui::drawList(screen, app, true);
  epd.displayBuffer(EInkDisplay::HALF_REFRESH);
  epd.deepSleep();
  SdMan.shutdown();
  // Same order as CrossPoint: cut the battery path (no-op on USB power), then
  // deep sleep until the power button is pressed.
  gpio_hold_dis(PIN_POWER_LATCH);
  gpio_set_direction(PIN_POWER_LATCH, GPIO_MODE_OUTPUT);
  gpio_set_level(PIN_POWER_LATCH, 0);
  gpio_hold_en(PIN_POWER_LATCH);
  freeink::PowerManager::powerDownRailsForSleep();
  freeink::PowerManager::deepSleepUntilPowerButton();
}

bool anyButtonDown() {
  for (uint8_t b = InputManager::BTN_BACK; b <= InputManager::BTN_POWER; ++b) {
    if (input.isPressed(b)) return true;
  }
  return false;
}

}  // namespace

void setup() {
  // Keep power on before anything else (a previous sleep may have held it LOW).
  gpio_hold_dis(PIN_POWER_LATCH);
  pinMode(PIN_POWER_LATCH, OUTPUT);
  digitalWrite(PIN_POWER_LATCH, HIGH);

  Serial.begin(115200);
  delay(100);
  Serial.println("[x3] boot");

  BoardConfig::selectDevice(BoardConfig::Board::XteinkX3);
  freeink::applyXteinkDisplayController();
  if (BoardConfig::ACTIVE.displayController == BoardConfig::DisplayController::UC8279) {
    BoardConfig::selectDevice(BoardConfig::Board::XteinkX3Uc8279);
  }

  SPI.begin(PIN_SCLK, PIN_MISO, PIN_MOSI, PIN_CS);
  input.begin();

  epd.setDisplayX3();
  epd.begin();
  epd.requestResync();
  screen.setRotation(3);  // portrait 528x792, upright on the X3

  rtc.begin();
  loadTasks();
  applySetTimeFile();
  app.lastInputMs = millis();
  render(true, /*deepClean=*/true);
}

void loop() {
  input.update();
  const uint32_t now = millis();

  if (waitingForRelease) {
    if (!anyButtonDown()) waitingForRelease = false;
  } else {
    for (uint8_t b = InputManager::BTN_BACK; b <= InputManager::BTN_POWER; ++b) {
      if (!input.wasPressed(b)) continue;
      const Screen before = app.screen;
      const Effects fx = app.onButton(static_cast<Button>(b), now);
      if (fx.sleep) goToSleep();
      if (fx.stopWifi) stopWifi();
      if (fx.startWifi) startWifi();
      if (fx.save) saveTasks();
      if (fx.redraw) render(fx.fullRefresh || app.screen != before, fx.deepClean);
      break;
    }
  }

  static uint32_t lastTick = 0;
  if (now - lastTick >= 500) {
    lastTick = now;
    const Effects fx = app.onTick(now);
    if (fx.sleep) goToSleep();
    if (fx.redraw) render(fx.fullRefresh);
  }

  if (wifiOn) {
    server.handleClient();
    if (webRedraw) {
      webRedraw = false;
      render(false);
    }
    delay(2);
  } else {
    delay(10);
  }
}
