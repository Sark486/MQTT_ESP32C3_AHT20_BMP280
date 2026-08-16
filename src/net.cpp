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

void onWifiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print(F("[net] Wi-Fi connected, IP address: "));
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println(F("[net] Wi-Fi lost connection"));
      // Don't chase MQTT while the link itself is down.
      xTimerStop(mqttReconnectTimer, 0);
      xTimerStart(wifiReconnectTimer, 0);
      break;

    default:
      // Other events (scan done, auth mode change, ...) need no action here.
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.printf("[net] connected to MQTT (session present: %d)\n", sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println(F("[net] disconnected from MQTT"));
  if (wifiConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.printf("[net] publish acknowledged, packetId: %u\n", packetId);
}

} // namespace

void netBegin() {
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(RECONNECT_DELAY_MS), pdFALSE,
                                    (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(RECONNECT_DELAY_MS), pdFALSE,
                                    (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));

  WiFi.onEvent(onWifiEvent);

  deviceId = WiFi.macAddress();
  telemetryTopic = MQTT_TOPIC_PREFIX + deviceId + "/telemetry";
  Serial.printf("[net] device id: %s\n", deviceId.c_str());
  Serial.printf("[net] telemetry topic: %s\n", telemetryTopic.c_str());

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(SECRET_MQTT_HOST, SECRET_MQTT_PORT);

  connectToWifi();
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
