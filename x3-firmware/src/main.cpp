// Xteink X3 — "Bugün" day planner + 25-minute focus timer.
//
// Tasks come from /gorevler.txt on the SD card (created with sample tasks if
// missing). Hardware glue only: the app logic lives in app.cpp, drawing in
// ui.cpp. Button layout: docs-hardware.md.

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <InputManager.h>
#include <PowerManager.h>
#include <SDCardManager.h>
#include <SPI.h>
#include <XteinkDetect.h>
#include <driver/gpio.h>

#include "app.h"
#include "ui.h"

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

constexpr const char* kTasksPath = "/gorevler.txt";
constexpr int kFastRefreshesBeforeFull = 20;

EInkDisplay epd(PIN_SCLK, PIN_MOSI, PIN_CS, PIN_DC, PIN_RST, PIN_BUSY);
InputManager input;
App app;
int fastRefreshes = 0;
bool waitingForRelease = true;  // ignore the press that woke the device

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
  FsFile f = SdMan.open(kTasksPath, O_RDONLY);
  if (!f) {
    Serial.println("[x3] creating sample /gorevler.txt");
    SdMan.writeFile(kTasksPath, String(kSampleTasksFile));
    app.tasks.loadSample();
    return;
  }
  static char buf[4096];
  const int n = f.read(buf, sizeof buf);
  f.close();
  app.tasks.parse(buf, n > 0 ? static_cast<size_t>(n) : 0);
  Serial.printf("[x3] loaded %d tasks\n", app.tasks.count);
}

void saveTasks() {
  if (!app.sdAvailable) return;
  static char buf[4096];
  app.tasks.serialize(buf, sizeof buf);
  if (!SdMan.writeFile(kTasksPath, String(buf))) Serial.println("[x3] save failed");
}

// screenChange -> HALF refresh; firstFrame -> FULL refresh (clears all ghosting).
void render(bool screenChange, bool firstFrame = false) {
  switch (app.screen) {
    case Screen::List: ui::drawList(screen, app, false); break;
    case Screen::Focus: ui::drawFocus(screen, app, millis()); break;
    case Screen::FocusDone: ui::drawFocusDone(screen, app); break;
  }
  if (firstFrame) {
    fastRefreshes = 0;
    epd.displayBuffer(EInkDisplay::FULL_REFRESH);
  } else if (screenChange || ++fastRefreshes >= kFastRefreshesBeforeFull) {
    fastRefreshes = 0;
    epd.displayBuffer(EInkDisplay::HALF_REFRESH);
  } else {
    epd.displayBuffer(EInkDisplay::FAST_REFRESH);
  }
}

[[noreturn]] void goToSleep() {
  Serial.println("[x3] sleep");
  ui::drawList(screen, app, true);
  // FULL refresh: this image stays on the panel while off, so leave no ghosts.
  epd.displayBuffer(EInkDisplay::FULL_REFRESH);
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

  loadTasks();
  app.lastInputMs = millis();
  render(true, /*firstFrame=*/true);
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
      if (fx.save) saveTasks();
      if (fx.redraw) render(fx.fullRefresh || app.screen != before);
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
  delay(10);
}
