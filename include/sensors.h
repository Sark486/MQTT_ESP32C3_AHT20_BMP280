// AHT20 (temperature + humidity) and BMP280 (pressure) behind one interface.
#pragma once

// A single sample set, one reading from each sensor.
struct Readings {
  float temperatureC = 0.0f;
  float humidityPct = 0.0f;
  float pressureHPa = 0.0f;
};

// Initializes both sensors. Returns false when either one did not respond.
bool sensorsBegin();

// Returns the latest sample.
Readings sensorsRead();
