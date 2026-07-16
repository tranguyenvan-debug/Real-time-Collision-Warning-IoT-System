<div align="center">

# Real-time Collision Warning IoT System

A real-time IoT-based collision warning system that detects obstacles, measures critical distances, and triggers instant local alerts along with a live radar-style web dashboard.

![Status](https://img.shields.io/badge/status-prototype-yellow)
![Platform](https://img.shields.io/badge/platform-ESP8266-blue)
![License](https://img.shields.io/badge/license-MIT-green)

</div>

---

## 📌 Overview

**Real-time Collision Warning IoT System** is a project built around an **ESP8266 (NodeMCU)** microcontroller. The system uses **HC-SR05 ultrasonic sensors** to continuously measure the distance to nearby obstacles, filters the raw signal for noise, and then:

- Triggers a **buzzer alarm** whenever an obstacle enters the danger zone.
- Displays real-time data on a **web-based radar dashboard** (sweeping radar view, updated every 100ms), accessible from any phone or computer on the same WiFi network.

## ✨ Key Features

- 📡 Real-time distance measurement using multiple HC-SR05 ultrasonic sensors.
- 🧮 Noise filtering via a **median filter** combined with a stability check across samples, rejecting sudden invalid readings.
- 🚨 Local audible alert via a **5VDC active buzzer** (driven through an NPN transistor, since the ESP8266 only outputs 3.3V).
- 🌐 **Radar-style web dashboard** showing each sensor's live position/distance, switching color when a danger zone is entered.
- 🔄 Automatic data refresh via a REST endpoint (`/laydulieu`) — no page reload needed.
- ⚙️ Configurable safe/warning distance thresholds in the firmware.

## 🧰 Hardware

<div align="center">

| Component | Qty | Notes |
|:---:|:---:|:---|
| ESP8266 NodeMCU | 1 | Main controller, WiFi + web server |
| HC-SR05 ultrasonic sensor | 2 (expandable) | Obstacle distance measurement |
| 5VDC active buzzer | 1 | Audible alert |
| NPN transistor (BC547 / 2N2222) | 1 | Drives the buzzer signal |
| 1kΩ resistor | 1 | Transistor base resistor |
| Perfboard / breadboard (9x15cm) | 1 | Circuit assembly |
| Jumper wires, micro-USB cable | — | — |

</div>

### Pinout (example with 2 sensors)

<div align="center">

| Signal | ESP8266 Pin | GPIO |
|:---:|:---:|:---:|
| Sensor 1 – TRIG | D1 | GPIO5 |
| Sensor 1 – ECHO | D2 | GPIO4 |
| Sensor 2 – TRIG | D5 | GPIO14 |
| Sensor 2 – ECHO | D6 | GPIO12 |
| Buzzer (via NPN transistor) | D0 | GPIO16 |

</div>

> ⚠️ **Note:** GPIO16 (D0) does not support PWM on the ESP8266 — it can only be driven digital HIGH/LOW, which is fine for an active buzzer.

## 🖥️ System Architecture

```
[HC-SR05 x N] → [ESP8266: distance measurement + noise filtering]
                        │
                        ├──> Local buzzer alarm (instant)
                        │
                        └──> Built-in web server (WiFi)
                                    │
                              [Web browser]
                              Radar dashboard
                          (refreshes every 100ms via /laydulieu)
```

## 🚀 Setup & Usage

### Requirements

- [PlatformIO](https://platformio.org/) (recommended, via VS Code) or the Arduino IDE.
- Supported board: `nodemcuv2` (ESP8266).

### PlatformIO configuration (`platformio.ini`)

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
upload_speed = 115200
```

### Deployment steps

1. Clone the repository:
   ```bash
   git clone https://github.com/tranguyenvan-debug/Real-time-Collision-Warning-IoT-System.git
   cd Real-time-Collision-Warning-IoT-System
   ```
2. Open the `Source Code` folder in PlatformIO.
3. Edit the WiFi credentials in the source code to match your network:
   ```cpp
   WiFi.begin("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD");
   ```
4. Connect the ESP8266 board via micro-USB, then build and upload the firmware.
5. Open the Serial Monitor (baud `115200`) to read the IP address assigned to the board.
6. Visit that IP address in a browser (on the same WiFi network) to open the **radar dashboard**.

## ⚙️ How It Works

1. Each HC-SR05 sensor is polled periodically (every 50ms by default).
2. Each raw reading passes through a filtering pipeline:
   - Validity check against the allowed distance range (2cm – 450cm).
   - Stability check against the most recent samples (maximum allowed deviation).
   - A **median filter** over the last 5 samples to reject noise.
3. If the filtered distance is ≤ the warning threshold (default 7cm), the system enters the **alert state**:
   - The buzzer sounds continuously.
   - The dashboard switches to "Unsafe" status (red/orange), showing which sensor(s) triggered it and their distance.
4. Once all sensors return to a safe distance, the buzzer stops and the dashboard returns to "Safe" status (green).

## 📁 Project Structure

```
Real-time-Collision-Warning-IoT-System/
├── Source Code/        # Firmware source (ESP8266, PlatformIO/Arduino)
├── .gitignore
├── LICENSE
└── README.md
```

## 🗺️ Roadmap

- [ ] Add more sensors for full 360° coverage around the device.
- [ ] Send alerts/telemetry to a remote dashboard (MQTT / Firebase / cloud server).
- [ ] Log alert history.
- [ ] Improve response speed and accuracy at short range.

## 📄 License

This project is released under the [MIT License](./LICENSE).

## 👤 Author

Built by **tranguyenvan-debug** — a real-time IoT collision warning system project.
