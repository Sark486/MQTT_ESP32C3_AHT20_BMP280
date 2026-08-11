// SSD1306 OLED rendering.
#pragma once

#include "sensors.h"

// Initializes the panel. Returns false when the panel did not respond.
bool displayBegin();

// The three telemetry lines.
void displayShowReadings(const Readings &readings);
