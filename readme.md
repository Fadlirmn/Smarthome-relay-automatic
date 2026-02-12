# ESP32 Smart Relay & Server Battery Monitor

## Overview

This project uses an **ESP32** to:

* Control 4 relays via secure MQTT (TLS)
* Monitor server battery level
* Automatically control Relay 1 based on battery thresholds
* Display system information on a 16x2 I2C LCD:

  * Real-time clock (NTP)
  * Relay status
  * Battery percentage
  * Server connection status (`OK` / `X`)
  * Current temperature and weather (OpenWeather API)

---

## Features

* MQTT over TLS (Port 8883)
* Server battery monitoring via `server/battery`
* Automatic battery-based relay control
* Server timeout detection (> 2 minutes = offline)
* Weather updates every 10 minutes
* Real-time clock via NTP
* LCD auto backlight off between 00:00–05:00

---

## LCD Display Format (16x2)

### Normal Operation

```
14:25 ●●○○ 76%OK
29.3C Clouds
```

### Server Offline (> 2 minutes without update)

```
14:25 ●●○○ 76%X
29.3C Clouds
```

### Legend

| Symbol | Meaning                              |
| ------ | ------------------------------------ |
| ●      | Relay ON                             |
| ○      | Relay OFF                            |
| 76%    | Server battery level                 |
| OK     | Server connected (receiving updates) |
| X      | Server offline (timeout)             |

---

## Hardware Requirements

* ESP32 Dev Module
* 16x2 I2C LCD (Address: 0x27)
* 4-Channel Relay Module
* 5V Power Supply

---

## Pin Configuration (ESP32)

| Function | GPIO |
| -------- | ---- |
| Relay 1  | 18   |
| Relay 2  | 19   |
| Relay 3  | 23   |
| Relay 4  | 5    |
| I2C SDA  | 21   |
| I2C SCL  | 22   |

---

## MQTT Configuration

```
Broker  : emqxsl.com
Port    : 8883 (TLS)
Username: username
Password: password
```

### Subscribed Topics

```
home/relay/#
server/battery
```

### Expected JSON Format (server/battery)

```json
{
  "capacity": 76,
  "status": "Charging"
}
```

---

## Battery Automation Logic

| Condition             | Action            |
| --------------------- | ----------------- |
| Battery ≤ 50%         | Relay 1 ON        |
| Battery ≥ 80%         | Relay 1 OFF       |
| No update > 2 minutes | Server status = X |

---

## Weather API

Using OpenWeatherMap:

```
http://api.openweathermap.org/data/2.5/weather
```

Update interval: 10 minutes.

---

## System Flow

1. Connect to WiFi
2. Synchronize time via NTP
3. Connect to MQTT broker (TLS)
4. Subscribe to topics
5. Monitor:

   * Battery updates
   * MQTT connection
   * Weather refresh timer
6. Refresh LCD every 1 second

---

## Required Libraries

Install via Arduino Library Manager:

* WiFi (ESP32 Core)
* PubSubClient
* LiquidCrystal_I2C
* ArduinoJson
* HTTPClient

---

## Setup Instructions

1. Install ESP32 Board Support in Arduino IDE.

2. Select:

   ```
   Tools → Board → ESP32 Dev Module
   ```

3. Configure your WiFi credentials.

4. Insert your OpenWeather API key.

5. Upload the sketch to the ESP32.

---

## Security Note

Currently using:

```cpp
espClient.setInsecure();
```

This enables TLS without certificate validation.

For production environments, it is recommended to use proper CA certificate validation.

---

## Possible Improvements

* Non-blocking WiFi reconnect
* FreeRTOS task separation (MQTT, LCD, Weather)
* OLED display upgrade
* Web dashboard
* OTA firmware updates
* Buzzer alarm for extended offline detection
* Battery progress bar visualization

---

## Project Structure

```
/ESP32-SmartRelay
 ├── main.ino
 └── README.md
```

---

## Author

ESP32 MQTT Monitoring System
Designed for server battery automation and IoT relay control.
