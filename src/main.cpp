#include <Arduino.h>

#include <WiFi.h>
#include <ArduinoJson.h>
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

#include "config.h"
#include "sensors.h"
#include "display_ui.h"

String deviceId = "";

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

unsigned long previousMillis = 0;   // Stores last time temperature was published

void connectToWifi() {
  Serial.println(F("[net] connecting to Wi-Fi..."));
  // Explicitly set the mode
  WiFi.mode(WIFI_STA);
  // Lowering WiFI power due to cheap ESP32C3 mini used; They can't handle power spikes well; Works without it on better board
  WiFi.setTxPower(WIFI_TX_POWER);
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println(F("[net] connecting to MQTT..."));
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print(F("[net] Wi-Fi connected, IP address: "));
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println(F("[net] Wi-Fi lost connection"));
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.printf("[net] connected to MQTT (session present: %d)\n", sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println(F("[net] disconnected from MQTT"));
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

/*void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}
void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}*/

void onMqttPublish(uint16_t packetId) {
  Serial.printf("[net] publish acknowledged, packetId: %u\n", packetId);
}

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

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(RECONNECT_DELAY_MS), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(RECONNECT_DELAY_MS), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);
  deviceId = WiFi.macAddress();

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  //mqttClient.onSubscribe(onMqttSubscribe);
  //mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(SECRET_MQTT_HOST, SECRET_MQTT_PORT);
  // If your broker requires authentication (username and password), set them below
  //mqttClient.setCredentials("REPlACE_WITH_YOUR_USER", "REPLACE_WITH_YOUR_PASSWORD");
  connectToWifi();
}

void loop() {
  unsigned long currentMillis = millis();
  // it publishes a new MQTT message
  if (currentMillis - previousMillis >= PUBLISH_INTERVAL_MS || previousMillis == 0) {

    if (WiFi.status() == WL_CONNECTED) {
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
    } else {
        Serial.println("[net] Wi-Fi not connected");
    }


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
    
    // Publish an MQTT message on topic esp32/dht/temperature
    String topic = MQTT_TOPIC_PREFIX + deviceId + "/telemetry";
    uint16_t packetIdPub1 = mqttClient.publish(topic.c_str(), MQTT_QOS, false, payload);                            
    Serial.printf("[net] published to %s (packetId %u): %s\n", topic.c_str(), packetIdPub1, payload);
  }
}
