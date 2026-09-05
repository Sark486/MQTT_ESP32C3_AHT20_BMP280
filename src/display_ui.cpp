#include "display_ui.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSerif9pt7b.h>

#include "config.h"

namespace {

// Everything that differs between the two panels.
struct OledLayout {
  uint8_t height;
  const GFXfont *font;  // nullptr => built-in 5x7 font
  uint8_t textSize;
  int16_t lineY[3];  // cursor Y for each telemetry line
  int16_t statusY;  // cursor Y for a single status message
};

#if OLED_HEIGHT == 64
static const OledLayout LAYOUT = {
    /* height */ 64,
    /* font */ &FreeSerif9pt7b,
    /* textSize */ 1,
    /* lineY */ {20, 40, 60},
    /* statusY */ 36,
};
#elif OLED_HEIGHT == 32
static const OledLayout LAYOUT = {
    /* height */ 32,
    /* font */ nullptr,
    /* textSize */ 1,
    /* lineY */ {0, 11, 22},
    /* statusY */ 12,
};
#else
#error "Unsupported OLED_HEIGHT, expected 64 or 32"
#endif

Adafruit_SSD1306 display(OLED_WIDTH, LAYOUT.height, &Wire, -1);
bool present = false;

// The panel keeps its text style between frames, so this only runs at startup.
void applyTextStyle() {
  display.setFont(LAYOUT.font);
  display.setTextSize(LAYOUT.textSize);
  display.setTextColor(SSD1306_WHITE);
}

// Prints one telemetry line as "<label><value><unit>", or "<label>--" when the
// sensor did not produce a usable value.
void printReading(int16_t y, const char *label, float value, bool valid,
                  uint8_t decimals, const char *unit) {
  display.setCursor(0, y);
  display.print(label);
  if (!valid) {
    display.print("--");
    return;
  }
  display.print(value, decimals);
  display.print(unit);
}

} // namespace

void displayBegin() {
  present = display.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDR);
  if (!present) {
    Serial.println(F("[display] SSD1306 not found, continuing without it"));
    return;
  }

  Serial.printf("[display] SSD1306 %ux%u ready\n", OLED_WIDTH, LAYOUT.height);
  display.clearDisplay();
  applyTextStyle();
  display.display();
}

void displayShowStatus(const char *message) {
  if (!present) {
    return;
  }
  display.clearDisplay();
  display.setCursor(0, LAYOUT.statusY);
  display.print(message);
  display.display();
}

void displayShowReadings(const Readings &readings) {
  if (!present) {
    return;
  }
  display.clearDisplay();

  // Labels are kept short so the longest line still fits 128 px in either font.
  printReading(LAYOUT.lineY[0], "T  ", readings.temperatureC, readings.temperatureValid, 2, " C");
  printReading(LAYOUT.lineY[1], "RH ", readings.humidityPct, readings.humidityValid, 1, " %");
  printReading(LAYOUT.lineY[2], "P  ", readings.pressureHPa, readings.pressureValid, 0, " hPa");

  display.display();
}
