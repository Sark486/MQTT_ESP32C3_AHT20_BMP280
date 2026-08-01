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

#include "secrets.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// #define SCREEN_HEIGHT 32 // OLED display height, in pixels


// // Temperature MQTT Topics
// #define MQTT_PUB_TEMP "sentinel/devices/pi_internal/telemetry"

String deviceId = "";

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

unsigned long previousMillis = 0;   // Stores last time temperature was published
const long interval = 60000;        // Interval at which to publish sensor readings


Adafruit_AHTX0 aht;
Adafruit_Sensor *aht_humidity, *aht_temp;

Adafruit_BMP280 bmp; // use I2C interface
Adafruit_Sensor *bmp_temp = bmp.getTemperatureSensor();
Adafruit_Sensor *bmp_pressure = bmp.getPressureSensor();

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  // Explicitly set the mode
  WiFi.mode(WIFI_STA);
  // Lowering WiFI power due to cheap ESP32C3 mini used; They can't handle power spikes well; Works without it on better board
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
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
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // dht.begin();
  Serial.println("Setup begin");
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

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
    Serial.println("Failed to find AHT10/AHT20 chip");
    while (1) {
      delay(10);
    }
  }

  Serial.println("AHT10/AHT20 Found!");
  aht_temp = aht.getTemperatureSensor();
  aht_temp->printSensorDetails();

  aht_humidity = aht.getHumiditySensor();
  aht_humidity->printSensorDetails();

  unsigned status;
  //status = bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID);
  status = bmp.begin();
  if (!status) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    Serial.print("SensorID was: 0x"); Serial.println(bmp.sensorID(),16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
    while (1) delay(10);
  }
}

void loop() {
  unsigned long currentMillis = millis();
  // it publishes a new MQTT message
  if (currentMillis - previousMillis >= interval || previousMillis == 0) {

    if (WiFi.status() == WL_CONNECTED) {
        long rssi = WiFi.RSSI();
        Serial.print("Signal Strength (RSSI): ");
        Serial.print(rssi);
        Serial.println(" dBm");
        
        // Quick interpretation
        if (rssi >= -60) Serial.println("-> Excellent connection!");
        else if (rssi >= -75) Serial.println("-> OK, but starting to struggle.");
        else Serial.println("-> Critical! Expect packet loss or drops.");
    } else {
        Serial.println("Connection lost!");
    }


    Serial.println("Starting sensor reading");
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
    String topic = "sentinel/devices/" + deviceId + "/telemetry";
    uint16_t packetIdPub1 = mqttClient.publish(topic.c_str(), 1, false, payload);                            
    Serial.printf("Publishing on topic %s at QoS 1, packetId: %i\n", topic.c_str(), packetIdPub1);
    Serial.printf("Message: %.2f \n", temp_event.temperature);
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
