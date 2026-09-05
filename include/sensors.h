// AHT20 (temperature + humidity) and BMP280 (pressure) behind one interface.
//
// Neither sensor is required to boot: if one is missing the firmware keeps
// running and reports only what it can actually measure.
#pragma once

// A single sample set. Each field is only meaningful when its *Valid flag is set:
// a missing sensor or an implausible reading clears the flag.
struct Readings {
  float temperatureC = 0.0f;
  float humidityPct = 0.0f;
  float pressureHPa = 0.0f;
  bool temperatureValid = false;
  bool humidityValid = false;
  bool pressureValid = false;

  // True when at least one field is usable, i.e. worth publishing.
  bool any() const { return temperatureValid || humidityValid || pressureValid; }
};

// Initializes both sensors. Returns false only when neither sensor responded.
bool sensorsBegin();

// Returns the latest sample. Every field carries its own *Valid flag; use
// any() to tell a fully failed cycle from a partial one.
Readings sensorsRead();
