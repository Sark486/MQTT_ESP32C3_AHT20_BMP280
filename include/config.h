// Non-secret, compile-time tunables for the whole firmware.
// Credentials live in secrets.h (gitignored); see secrets.h.example.
#pragma once

#include <stdint.h>

#include "secrets.h"

// --- Serial ---
static const uint32_t SERIAL_BAUD = 115200;
// USB CDC only enumerates a moment after boot, so without this wait the first
// prints are lost. Bounded so a device running on external power never stalls here.
static const uint32_t SERIAL_READY_TIMEOUT_MS = 2000;

// --- Telemetry ---
static const uint32_t PUBLISH_INTERVAL_MS = 60000;
static const char MQTT_TOPIC_PREFIX[] = "sentinel/devices/";
static const uint8_t MQTT_QOS = 1;

// --- Networking ---
static const uint32_t RECONNECT_DELAY_MS = 2000;

// My cheap ESP32-C3 mini browns out on TX power spikes. Lowering the TX power keeps
// it stable; a better board works at full power without this.
#define WIFI_TX_POWER WIFI_POWER_8_5dBm

// --- Sensors ---
// 0x77 is the BMP280 default; some breakouts strap the alternate 0x76.
static const uint8_t BMP280_I2C_ADDR = 0x77;

// Plausibility bounds. Readings outside these are treated as sensor faults and are
// neither published nor displayed.
static const float TEMP_MIN_C = -40.0f;
static const float TEMP_MAX_C = 85.0f;
static const float HUMIDITY_MIN = 0.0f;
static const float HUMIDITY_MAX = 100.0f;
static const float PRESSURE_MIN_H = 300.0f;
static const float PRESSURE_MAX_H = 1100.0f;

// If every sensor fails to initialize there is nothing to report, so reboot
// after showing the error rather than sitting there dark.
static const uint32_t NO_SENSOR_REBOOT_MS = 60000;

// --- Display ---
static const uint8_t SSD1306_I2C_ADDR = 0x3C;
static const uint8_t OLED_WIDTH = 128;
// OLED_HEIGHT comes from platformio.ini, one value per build environment.
