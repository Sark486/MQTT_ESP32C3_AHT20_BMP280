#include "display_ui.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSerif9pt7b.h>

#include "config.h"

namespace {

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
bool present = false;

// The panel keeps its text style between frames, so this only runs at startup.
void applyTextStyle() {
  display.setFont(&FreeSerif9pt7b);
  display.setTextSize(1);
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

  Serial.printf("[display] SSD1306 %ux%u ready\n", OLED_WIDTH, OLED_HEIGHT);
  display.clearDisplay();
  applyTextStyle();
  display.display();
}

void displayShowStatus(const char *message) {
  if (!present) {
    return;
  }
  display.clearDisplay();
  display.setCursor(0, 36);
  display.print(message);
  display.display();
}

void displayShowReadings(const Readings &readings) {
  if (!present) {
    return;
  }
  display.clearDisplay();

  printReading(20, "Temp: ", readings.temperatureC, readings.temperatureValid, 2, " C");
  printReading(40, "Hum: ", readings.humidityPct, readings.humidityValid, 1, " %");
  printReading(60, "Press: ", readings.pressureHPa, readings.pressureValid, 0, " hPa");

  display.display();
}
