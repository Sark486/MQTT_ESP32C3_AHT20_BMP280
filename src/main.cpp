// ESP32-C3 environment sensor node.
//
// Reads an AHT20 (temperature, humidity) and a BMP280 (pressure), publishes them
// as JSON over MQTT once per PUBLISH_INTERVAL_MS, and shows them on an SSD1306 OLED.
// Wi-Fi and MQTT are handled asynchronously in net.cpp.
//
// Build with -e oled_128x64 or -e oled_128x32 to match the attached panel.
// Credentials live in include/secrets.h (gitignored, example provided).

#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "sensors.h"
#include "display_ui.h"
#include "net.h"

namespace {

// Publish on the very first pass instead of waiting a full interval.
bool firstPublish = true;
uint32_t lastPublishMs = 0;

void startSerial() {
  Serial.begin(SERIAL_BAUD);
  const uint32_t deadline = millis() + SERIAL_READY_TIMEOUT_MS;
  while (!Serial && millis() < deadline) {
    delay(10);
  }
  Serial.println();
}

void publishReadings(const Readings &readings) {
  if (!readings.any()) {
    return;
  }

  JsonDocument doc;

  // Key names are part of the contract with the consuming Pydantic model
  if (readings.temperatureValid) {
    doc["temperature"] = readings.temperatureC;
  }
  if (readings.humidityValid) {
    doc["humidity"] = readings.humidityPct;
  }
  if (readings.pressureValid) {
    doc["pressure"] = readings.pressureHPa;
  }

  char payload[128];
  serializeJson(doc, payload);

  netPublishTelemetry(payload);
}

} // namespace

void setup() {
  startSerial();
  Serial.println(F("[main] booting"));

  displayBegin();
  displayShowStatus("Booting...");

  if (!sensorsBegin()) {
    Serial.println(F("[main] no sensors available, rebooting shortly"));
    displayShowStatus("No sensors!");
    delay(NO_SENSOR_REBOOT_MS);
    ESP.restart();
  }

  displayShowStatus("Connecting...");
  netBegin();
}

void loop() {
  uint32_t now = millis();
  if (!firstPublish && now - lastPublishMs < PUBLISH_INTERVAL_MS) {
    delay(10);  // yield instead of spinning between publishes
    return;
  }
  firstPublish = false;
  lastPublishMs = now;

  netLogSignalQuality(); // for Wi-Fi signal debugging only

  Readings readings = sensorsRead();
  displayShowReadings(readings);  // renders "--" for invalid fields
  publishReadings(readings);
}
