# ESP32-C3 environment sensor

A small MQTT sensor node. An ESP32-C3 reads temperature and humidity from an AHT20
and pressure from a BMP280, publishes them as JSON once a minute, and shows the
current values on an SSD1306 OLED.

It is meant to be always on: Wi-Fi and MQTT are handled with events and
FreeRTOS timers rather than blocking calls, reconnects back off exponentially, and the
device reboots itself if it stays offline long enough that something is clearly wrong.

## Hardware

| Part | Notes |
| --- | --- |
| ESP32-C3 (esp32-c3-devkitm-1) | Any C3 board with USB CDC should work |
| AHT20 | Temperature and humidity, I2C 0x38 |
| BMP280 | Pressure, I2C 0x77 |
| SSD1306 OLED | 128x64 or 128x32, I2C 0x3C, optional |

Everything shares one I2C bus on the ESP32-C3 defaults, SDA on GPIO8 and SCL on GPIO9, plus
3V3 and GND. 

## Getting started

1. Copy the credentials template and fill it in. The real file is gitignored.

   ```sh
   cp include/secrets.h.example include/secrets.h
   ```

   `SECRET_MQTT_HOST` takes either an IP or a hostname. Leave `SECRET_MQTT_USER` empty
   to connect without authentication.

2. Build and flash the environment that matches your panel:

   ```sh
   pio run -e oled_128x64 -t upload    # 128x64 panel
   pio run -e oled_128x32 -t upload    # 128x32 panel
   ```

3. Watch it come up:

   ```sh
   pio device monitor
   ```

   ```
   [main] booting
   [display] SSD1306 128x32 ready
   [sensors] AHT10/AHT20 found
   [sensors] BMP280 found
   [net] device id: AA:BB:CC:DD:EE:FF
   [net] telemetry topic: sentinel/devices/AA:BB:CC:DD:EE:FF/telemetry
   [net] broker: 192.168.1.10:1883
   [net] connecting to Wi-Fi...
   [net] Wi-Fi connected, IP address: 192.168.1.42
   [net] connecting to MQTT...
   [net] connected to MQTT (session present: 0)
   [net] RSSI -67 dBm (workable, starting to struggle)
   [net] published to sentinel/devices/E8:3D:C1:9D:B6:94/telemetry (packetId 1): {"temperature":24.96929,"humidity":41.42294,"pressure":1000.097}
   [net] publish acknowledged, packetId: 1
   ```

## What it publishes

Every 60 seconds, to `sentinel/devices/<MAC>/telemetry` at QoS 1, not retained:

```json
{"temperature": 23.4, "humidity": 41.2, "pressure": 1013.2}
```

The device id is the Wi-Fi MAC, so a node needs no per-device configuration. A field is
left out entirely when its sensor is missing or its reading is unrealistic, so the
consumer has to treat all three as optional. The key names match the Pydantic model on
the collector side (sentineledge), so renaming one breaks the consumer.

## Code layout

| File | Responsibility |
| --- | --- |
| `src/main.cpp` | Setup, the publish interval, JSON assembly |
| `src/net.cpp` | Wi-Fi and MQTT lifecycle, reconnect logic, the offline watchdog |
| `src/sensors.cpp` | AHT20 and BMP280 behind one `Readings` struct |
| `src/display_ui.cpp` | SSD1306 rendering, layout per panel size |
| `include/config.h` | Every non-secret tunable, with the reasoning next to it |

Panel size is a compile-time flag (`OLED_HEIGHT`), set per environment in
`platformio.ini`, so the font and line positions are picked at build time instead of
being decided at runtime on a device that will only ever have one panel.

## Notes

- **`AsyncMqttClient` sits in `lib/` instead of `lib_deps`, and that is deliberate.** I got
  it the way the tutorial I followed suggested, as a ZIP downloaded from GitHub, and that
  ZIP is the project's develop branch rather than the 0.9.0 release the PlatformIO registry
  serves. 0.9.0 also depends on the old me-no-dev AsyncTCP, which would clash with the
  ESP32Async 3.x pinned in `platformio.ini`. Move it into `lib_deps` and you are building
  against different code than this was tested with.
- TX power is capped at 8.5 dBm. My cheap C3 board browns out on full-power spikes;
  better boards do not need this, and it is a one-line change in `config.h`.
- `include/secrets.h` is gitignored and has never been committed. `secrets.h.example` is
  the template.

## Credits

The first version of this was a [Random Nerd Tutorials](https://randomnerdtutorials.com/esp32-mqtt-publish-bme680-arduino/)
MQTT example for the ESP32, which is also where the AsyncMqttClient ZIP came from. The
async client setup and the FreeRTOS reconnect timers still follow the shape their example
taught me. Everything else has moved on since.
