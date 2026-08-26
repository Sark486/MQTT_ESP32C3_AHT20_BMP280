#include "net.h"

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMqttClient.h>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}

#include "config.h"

namespace {

AsyncMqttClient mqttClient;
TimerHandle_t wifiReconnectTimer = nullptr;
TimerHandle_t mqttReconnectTimer = nullptr;

String deviceId;
String telemetryTopic;

int wifiAttempts = 0;
int mqttAttempts = 0;
uint32_t lastMqttConnectMs = 0;

// Doubles the base delay per failed attempt, capped at RECONNECT_MAX_MS.
uint32_t backoffMs(int attempts) {
  uint32_t delayMs = RECONNECT_BASE_MS;
  for (int i = 0; i < attempts; i++) {
    delayMs *= 2;
    if (delayMs >= RECONNECT_MAX_MS) {
      return RECONNECT_MAX_MS;
    }
  }
  return delayMs;
}

void scheduleRetry(TimerHandle_t timer, int &attempts) {
  const uint32_t delayMs = backoffMs(attempts);
  attempts++;
  xTimerChangePeriod(timer, pdMS_TO_TICKS(delayMs), 0);
  xTimerStart(timer, 0);
  Serial.printf("[net] retrying in %u ms (attempt %d)\n", delayMs, attempts);
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void connectToWifi() {
  Serial.println(F("[net] connecting to Wi-Fi..."));
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_TX_POWER);
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println(F("[net] connecting to MQTT..."));
  mqttClient.connect();
}

// FreeRTOS passes the timer handle to its callback, so the signatures have to match.
// The example I started from cast connectToWifi() straight into the callback type,
// which works in practice but is undefined behavior.
void onWifiTimer(TimerHandle_t) { connectToWifi(); }
void onMqttTimer(TimerHandle_t) { connectToMqtt(); }

void onWifiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      wifiAttempts = 0;
      Serial.print(F("[net] Wi-Fi connected, IP address: "));
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println(F("[net] Wi-Fi lost connection"));
      // Don't chase MQTT while the link itself is down.
      xTimerStop(mqttReconnectTimer, 0);
      scheduleRetry(wifiReconnectTimer, wifiAttempts);
      break;

    default:
      // Other events (scan done, auth mode change, ...) need no action here.
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  mqttAttempts = 0;
  lastMqttConnectMs = millis();
  Serial.printf("[net] connected to MQTT (session present: %d)\n", sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.printf("[net] disconnected from MQTT (reason: %d)\n", (int)reason);
  if (!wifiConnected()) {
    return;
  }

  if (mqttAttempts >= MQTT_BOUNCE_WIFI_AFTER_ATTEMPTS) {
    // Wi-Fi says it is up but MQTT still gets nothing back, so the TCP stack is
    // probably stuck. Turning the radio off and on again clears it, and the
    // ARDUINO_EVENT_WIFI_STA_DISCONNECTED handler then reconnects as usual.
    Serial.println(F("[net] MQTT unreachable despite Wi-Fi being up, bouncing Wi-Fi"));
    mqttAttempts = 0;
    WiFi.disconnect(true);  // true = power the radio down too
    return;
  }

  scheduleRetry(mqttReconnectTimer, mqttAttempts);
}

void onMqttPublish(uint16_t packetId) {
  Serial.printf("[net] publish acknowledged, packetId: %u\n", packetId);
}

} // namespace

void netBegin() {
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(RECONNECT_BASE_MS),
                                    pdFALSE, nullptr, onWifiTimer);
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(RECONNECT_BASE_MS),
                                    pdFALSE, nullptr, onMqttTimer);

  WiFi.onEvent(onWifiEvent);

  deviceId = WiFi.macAddress();
  telemetryTopic = MQTT_TOPIC_PREFIX + deviceId + "/telemetry";
  Serial.printf("[net] device id: %s\n", deviceId.c_str());
  Serial.printf("[net] telemetry topic: %s\n", telemetryTopic.c_str());

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(SECRET_MQTT_HOST, SECRET_MQTT_PORT);

  lastMqttConnectMs = millis();
  connectToWifi();
}

void netLoop() {
  if (mqttClient.connected()) {
    lastMqttConnectMs = millis();
    return;
  }
  if (millis() - lastMqttConnectMs >= OFFLINE_REBOOT_MS) {
    Serial.println(F("[net] offline too long, rebooting"));
    Serial.flush();
    ESP.restart();
  }
}

void netPublishTelemetry(const char *payload) {
  const uint16_t packetId = mqttClient.publish(telemetryTopic.c_str(), MQTT_QOS, false, payload);
  Serial.printf("[net] published to %s (packetId %u): %s\n", telemetryTopic.c_str(), packetId, payload);
}

void netLogSignalQuality() {
  if (!wifiConnected()) {
    Serial.println("[net] Wi-Fi not connected");
    return;
  }

  long rssi = WiFi.RSSI();
  const char *quality;
  if (rssi >= -60) {
    quality = "excellent";
  } else if (rssi >= -75) {
    quality = "workable, starting to struggle";
  } else {
    quality = "critical, expect packet loss";
  }
  Serial.printf("[net] RSSI %ld dBm (%s)\n", rssi, quality);
}
