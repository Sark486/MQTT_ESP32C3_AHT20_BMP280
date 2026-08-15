// SSD1306 OLED rendering.
//
// The display is optional: if it is absent every call here becomes a no-op and
// the firmware carries on.
#pragma once

#include "sensors.h"

// Initializes the panel. A panel that does not respond is logged, not fatal.
void displayBegin();

// A single status line, roughly centered vertically: booting, connecting, errors.
void displayShowStatus(const char *message);

// The three telemetry lines. Invalid readings render as "--".
void displayShowReadings(const Readings &readings);
