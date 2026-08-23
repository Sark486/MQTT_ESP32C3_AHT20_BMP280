// Wi-Fi + MQTT lifecycle.
//
// Connections are driven by events and FreeRTOS timers, so the main loop never
// blocks on the network. Reconnect attempts back off exponentially, and the device
// reboots if it stays offline for OFFLINE_REBOOT_MS.
#pragma once

// Registers callbacks, derives the device id and topics, starts connecting.
// Returns immediately; connecting happens in the background.
void netBegin();

// Call once per main-loop iteration; drives the stay-offline reboot watchdog.
void netLoop();

// Publishes to the telemetry topic.
void netPublishTelemetry(const char *payload);

// Logs RSSI with a plain-language interpretation.
void netLogSignalQuality();
