#include "sensors.h"

#include <Arduino.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

#include "config.h"

namespace {

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

// These only become valid after begin(). I originally had them at global scope like
// the Adafruit example, which reads them before the driver is configured.
Adafruit_Sensor *ahtTemp = nullptr;
Adafruit_Sensor *ahtHumidity = nullptr;
Adafruit_Sensor *bmpPressure = nullptr;

bool ahtReady = false;
bool bmpReady = false;

bool beginAht() {
  if (!aht.begin()) {
    Serial.println(F("[sensors] AHT10/AHT20 not found, no temperature or humidity"));
    return false;
  }
  Serial.println(F("[sensors] AHT10/AHT20 found"));
  ahtTemp = aht.getTemperatureSensor();
  ahtHumidity = aht.getHumiditySensor();
  return ahtTemp != nullptr && ahtHumidity != nullptr;
}

bool beginBmp() {
  if (!bmp.begin(BMP280_I2C_ADDR)) {
    Serial.println(F("[sensors] BMP280 not found, no pressure. Check the wiring and the address."));
    Serial.printf("[sensors] sensorID was 0x%02X\n", bmp.sensorID());
    Serial.println(F("          0xFF -> bad address, or a BMP180/BMP085"));
    Serial.println(F("          0x56-0x58 -> BMP280, 0x60 -> BME280, 0x61 -> BME680"));
    return false;
  }
  Serial.println(F("[sensors] BMP280 found"));

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */

  bmpPressure = bmp.getPressureSensor();
  return bmpPressure != nullptr;
}

} // namespace

bool sensorsBegin() {
  ahtReady = beginAht();
  bmpReady = beginBmp();
  return ahtReady || bmpReady;
}

Readings sensorsRead() {
  Readings sample;

  if (ahtReady) {
    sensors_event_t tempEvent;
    sensors_event_t humidityEvent;
    ahtTemp->getEvent(&tempEvent);
    ahtHumidity->getEvent(&humidityEvent);

    sample.temperatureC = tempEvent.temperature;
    sample.temperatureValid = true;

    sample.humidityPct = humidityEvent.relative_humidity;
    sample.humidityValid = true;
  }

  if (bmpReady) {
    sensors_event_t pressureEvent;
    bmpPressure->getEvent(&pressureEvent);

    sample.pressureHPa = pressureEvent.pressure;
    sample.pressureValid = true;
  }

  if (!sample.any()) {
    Serial.println("[sensors] no valid readings this cycle");
  }
  return sample;
}
