#include "display_ui.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSerif9pt7b.h>

#include "config.h"

namespace {

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// The panel keeps its text style between frames, so this only runs at startup.
void applyTextStyle() {
  display.setFont(&FreeSerif9pt7b);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

} // namespace

bool displayBegin() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDR)) {
    Serial.println(F("[display] SSD1306 allocation failed"));
    return false;
  }

  Serial.printf("[display] SSD1306 %ux%u ready\n", OLED_WIDTH, OLED_HEIGHT);
  display.clearDisplay();
  applyTextStyle();
  display.display();
  return true;
}

void displayShowReadings(const Readings &readings) {
  display.clearDisplay();

  display.setCursor(0, 20);
  display.printf("Temp: %.2f", readings.temperatureC);
  display.setCursor(0, 40);
  display.printf("Humidity: %.2f", readings.humidityPct);
  display.setCursor(0, 60);
  display.printf("Pressure: %.2f", readings.pressureHPa);

  display.display();
}
