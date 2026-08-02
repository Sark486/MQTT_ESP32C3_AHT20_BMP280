#include <Arduino.h>

#include <WiFi.h>
#include <ArduinoJson.h>
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/timers.h"
}
#include <AsyncMqttClient.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <Fonts/FreeSerif9pt7b.h>

#include "config.h"

String deviceId = "";

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

unsigned long previousMillis = 0;   // Stores last time temperature was published


Adafruit_AHTX0 aht;
Adafruit_Sensor *aht_humidity, *aht_temp;

Adafruit_BMP280 bmp; // use I2C interface
Adafruit_Sensor *bmp_temp = bmp.getTemperatureSensor();
Adafruit_Sensor *bmp_pressure = bmp.getPressureSensor();

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

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

  if(!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDR)) {
    Serial.println(F("[display] SSD1306 allocation failed"));
    for(;;);
  }

  // dht.begin();
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

  if (!aht.begin()) {
    Serial.println(F("[sensors] failed to find AHT10/AHT20 chip"));
    while (1) {
      delay(10);
    }
  }

  Serial.println(F("[sensors] AHT10/AHT20 found"));
  aht_temp = aht.getTemperatureSensor();
  aht_temp->printSensorDetails();

  aht_humidity = aht.getHumiditySensor();
  aht_humidity->printSensorDetails();

  unsigned status;
  //status = bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID);
  status = bmp.begin(BMP280_I2C_ADDR);
  if (!status) {
    Serial.println(F("[sensors] BMP280 not found. Check the wiring and the address."));
    Serial.printf("[sensors] sensorID was 0x%02X\n", bmp.sensorID());
    Serial.println(F("          0xFF -> bad address, or a BMP180/BMP085"));
    Serial.println(F("          0x56-0x58 -> BMP280, 0x60 -> BME280, 0x61 -> BME680"));
    while (1) delay(10);
  }
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
    // New DHT sensor readings
    // hum = dht.readHumidity();
    // Read temperature as Celsius (the default)
    // temp_event = dht.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    //temp_event = dht.readTemperature(true);

    // Check if any reads failed and exit early (to try again).
    // if (isnan(temp_event) || isnan(hum)) {
    //   Serial.println(F("Failed to read from DHT sensor!"));
    //   Serial.println(temp_event);
    //   Serial.println(hum);
    //   return;
    // }

    sensors_event_t humidity_event;
    sensors_event_t temp_event;
    sensors_event_t pressure_event;
    aht_humidity->getEvent(&humidity_event);
    aht_temp->getEvent(&temp_event);
    bmp_pressure->getEvent(&pressure_event);

    JsonDocument doc;

    // 2. Populate data - Ensure keys match your Python Pydantic model exactly
    doc["temperature"] = temp_event.temperature;
    doc["humidity"] = humidity_event.relative_humidity;
    doc["pressure"] = pressure_event.pressure; // Use a real sensor value if available

    // 3. Serialize to a buffer
    char payload[128];
    serializeJson(doc, payload);
    
    // Publish an MQTT message on topic esp32/dht/temperature
    String topic = MQTT_TOPIC_PREFIX + deviceId + "/telemetry";
    uint16_t packetIdPub1 = mqttClient.publish(topic.c_str(), MQTT_QOS, false, payload);                            
    Serial.printf("[net] published to %s (packetId %u): %s\n", topic.c_str(), packetIdPub1, payload);
      display.clearDisplay();

    display.setFont(&FreeSerif9pt7b);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 20);
    // Display static text
    display.printf("Temp: %.2f", temp_event.temperature);
    display.setCursor(0, 40);
    display.printf("Humidity: %.2f", humidity_event.relative_humidity);
    display.setCursor(0, 60);
    display.printf("Pressure: %.2f", pressure_event.pressure);
    display.display();
  }
}
