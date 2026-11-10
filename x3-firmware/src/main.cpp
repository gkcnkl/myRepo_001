// Xteink X3 — step 1: hello screen + button test.
//
// Draws a portrait "Merhaba" screen, then shows the name of each button you
// press so we can map the physical buttons. No deep sleep yet: unplug and
// hold power (or reflash the stock backup) to stop it.

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <InputManager.h>
#include <SPI.h>
#include <XteinkDetect.h>

namespace {

constexpr int PIN_SCLK = 8;
constexpr int PIN_MOSI = 10;
constexpr int PIN_MISO = 7;
constexpr int PIN_CS = 21;
constexpr int PIN_DC = 4;
constexpr int PIN_RST = 5;
constexpr int PIN_BUSY = 6;
constexpr int PIN_SD_POWER = 13;

EInkDisplay epd(PIN_SCLK, PIN_MOSI, PIN_CS, PIN_DC, PIN_RST, PIN_BUSY);
InputManager input;

// Adafruit_GFX canvas that draws straight into the SDK framebuffer
// (792x528 native, row-major, MSB-first, 1 = white).
class Screen : public Adafruit_GFX {
 public:
  Screen() : Adafruit_GFX(EInkDisplay::X3_DISPLAY_WIDTH, EInkDisplay::X3_DISPLAY_HEIGHT) {}

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= width() || y >= height()) return;
    int16_t px = x, py = y;
    switch (getRotation()) {
      case 1: px = WIDTH - 1 - y; py = x; break;
      case 2: px = WIDTH - 1 - x; py = HEIGHT - 1 - y; break;
      case 3: px = y; py = HEIGHT - 1 - x; break;
      default: break;
    }
    uint8_t* fb = epd.getFrameBuffer();
    uint8_t& cell = fb[py * epd.getDisplayWidthBytes() + (px >> 3)];
    const uint8_t mask = 0x80 >> (px & 7);
    if (color) cell &= ~mask;  // color 1 = black ink
    else cell |= mask;
  }

  void clear() { epd.clearScreen(0xFF); }

  void centered(const char* text, int16_t y) {
    int16_t bx, by;
    uint16_t bw, bh;
    getTextBounds(text, 0, y, &bx, &by, &bw, &bh);
    setCursor((width() - bw) / 2 - bx, y);
    print(text);
  }
};

Screen screen;
uint32_t pressCount = 0;

const char* buttonName(uint8_t b) {
  switch (b) {
    case InputManager::BTN_BACK: return "GERI (BACK)";
    case InputManager::BTN_CONFIRM: return "TAMAM (CONFIRM)";
    case InputManager::BTN_LEFT: return "SOL (LEFT)";
    case InputManager::BTN_RIGHT: return "SAG (RIGHT)";
    case InputManager::BTN_UP: return "YUKARI (UP)";
    case InputManager::BTN_DOWN: return "ASAGI (DOWN)";
    case InputManager::BTN_POWER: return "GUC (POWER)";
    default: return "?";
  }
}

void drawFrame(const char* lastButton) {
  screen.clear();
  screen.setTextColor(1);
  screen.drawRect(8, 8, screen.width() - 16, screen.height() - 16, 1);
  screen.fillRect(8, 8, screen.width() - 16, 40, 1);  // marks the TOP edge

  screen.setFont(&FreeSansBold18pt7b);
  screen.centered("Merhaba!", 180);
  screen.setFont(&FreeSans12pt7b);
  screen.centered("Ozel firmware calisiyor", 230);
  screen.centered("Bir tusa bas:", 360);

  screen.setFont(&FreeSansBold18pt7b);
  screen.centered(lastButton, 430);

  char line[40];
  snprintf(line, sizeof line, "Basma sayisi: %lu", static_cast<unsigned long>(pressCount));
  screen.setFont(&FreeSans12pt7b);
  screen.centered(line, 520);

  const char* panel = BoardConfig::ACTIVE.displayController == BoardConfig::DisplayController::UC8279
                          ? "Ekran: UC8279"
                          : "Ekran: UC8253";
  screen.centered(panel, screen.height() - 40);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("[x3] boot");

  pinMode(PIN_SD_POWER, OUTPUT);
  digitalWrite(PIN_SD_POWER, HIGH);

  // This build only targets the X3; pick its profile, then probe which panel
  // controller this unit has (older UC8253 vs newer UC8279).
  BoardConfig::selectDevice(BoardConfig::Board::XteinkX3);
  freeink::applyXteinkDisplayController();
  if (BoardConfig::ACTIVE.displayController == BoardConfig::DisplayController::UC8279) {
    BoardConfig::selectDevice(BoardConfig::Board::XteinkX3Uc8279);
  }
  Serial.printf("[x3] panel=%s\n",
                BoardConfig::ACTIVE.displayController == BoardConfig::DisplayController::UC8279 ? "UC8279" : "UC8253");

  SPI.begin(PIN_SCLK, PIN_MISO, PIN_MOSI, PIN_CS);
  input.begin();

  epd.setDisplayX3();
  epd.begin();
  epd.requestResync();

  screen.setRotation(1);  // portrait, 528x792
  drawFrame("-");
  epd.displayBuffer(EInkDisplay::FULL_REFRESH);
  Serial.println("[x3] first frame done");
}

void loop() {
  input.update();
  for (uint8_t b = InputManager::BTN_BACK; b <= InputManager::BTN_POWER; ++b) {
    if (input.wasPressed(b)) {
      ++pressCount;
      Serial.printf("[x3] button %u %s\n", b, buttonName(b));
      drawFrame(buttonName(b));
      // Fast refresh for snappy feedback; full refresh every 10 presses to clear ghosting.
      epd.displayBuffer(pressCount % 10 == 0 ? EInkDisplay::FULL_REFRESH : EInkDisplay::FAST_REFRESH);
      break;
    }
  }
  delay(10);
}
