// Wi-Fi + MQTT lifecycle.
//
// Connections are driven by events and FreeRTOS timers, so the main loop never
// blocks on the network.
#pragma once

// Registers callbacks, derives the device id and topics, starts connecting.
// Returns immediately; connecting happens in the background.
void netBegin();

// Publishes to the telemetry topic.
void netPublishTelemetry(const char *payload);

// Logs RSSI with a plain-language interpretation.
void netLogSignalQuality();
