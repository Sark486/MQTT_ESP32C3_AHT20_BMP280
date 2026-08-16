#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"
#include "sensors.h"
#include "display_ui.h"
#include "net.h"

unsigned long previousMillis = 0;   // Stores last time temperature was published

void setup() {
  Serial.begin(SERIAL_BAUD);
  const uint32_t deadline = millis() + SERIAL_READY_TIMEOUT_MS;
  while (!Serial && millis() < deadline) {
    delay(10);
  }
  Serial.println();
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
  unsigned long currentMillis = millis();
  // it publishes a new MQTT message
  if (currentMillis - previousMillis >= PUBLISH_INTERVAL_MS || previousMillis == 0) {

    netLogSignalQuality(); // for Wi-Fi signal debugging only

    // Save the last time a new reading was published
    previousMillis = currentMillis;

    Readings readings = sensorsRead();
    displayShowReadings(readings);  // renders "--" for invalid fields
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

    // 3. Serialize to a buffer
    char payload[128];
    serializeJson(doc, payload);

    netPublishTelemetry(payload);
  }
}
