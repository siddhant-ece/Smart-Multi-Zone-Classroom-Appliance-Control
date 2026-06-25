# 🏫 Smart Multi-Zone Classroom Appliance Control System
### ESP32 | PIR Sensors | DHT22 | Relay Control | I2C LCD | Blynk IoT | C (ESP-IDF)

---

## 📌 Overview
A smart, automated classroom power management system that controls lights and fans
zone-by-zone using **PIR-based presence detection** and **temperature-adaptive fan delay logic**.
Built on the **ESP32 microcontroller**, the system ensures only occupied areas of the classroom
receive power — eliminating electricity waste without any manual intervention.

The system is **privacy-friendly** (no cameras), **low-cost**, and **IoT-enabled** via Blynk
for real-time monitoring and long-term occupancy analytics.

---

## ⚙️ Technologies Used
| Category | Details |
|---|---|
| Microcontroller | ESP32 (ESP32-WROOM) |
| Occupancy Sensors | 3x PIR Sensor Modules |
| Temperature Sensor | DHT22 |
| Display | 16x2 LCD (I2C, PCF8574) |
| Relay Driver | NPN Transistor + 1N4007 Flyback Diode |
| IoT Platform | Blynk Cloud |
| Communication | Wi-Fi (ESP32 built-in) |
| Language | C (ESP-IDF Framework) |
| IDE | VS Code + ESP-IDF Plugin |

---

## 🔌 Hardware Pin Configuration
| Component | ESP32 Pin | Mode |
|---|---|---|
| PIR Sensor — Front | GPIO 5 | Digital Input |
| PIR Sensor — Middle | GPIO 18 | Digital Input |
| PIR Sensor — Back | GPIO 19 | Digital Input |
| Relay — Front Zone | GPIO 25 | Digital Output |
| Relay — Middle Zone | GPIO 26 | Digital Output |
| Relay — Back Zone | GPIO 27 | Digital Output |
| DHT22 Data | GPIO 4 | Single-wire |
| LCD SDA (I2C) | GPIO 21 | I2C |
| LCD SCL (I2C) | GPIO 22 | I2C |

---

## 🌡️ Temperature-Adaptive Fan Delay Logic
| Room Temperature | Fan Stays ON After Last Motion |
|---|---|
| Above 30°C | 9 minutes |
| 27°C – 30°C | 6 minutes |
| Below 27°C | 2 minutes |

---

## 📱 Blynk Virtual Pin Mapping
| Virtual Pin | Data |
|---|---|
| V0 | Temperature (°C) |
| V1 | Humidity (%) |
| V2 | Front Zone Status (0/1) |
| V3 | Middle Zone Status (0/1) |
| V4 | Back Zone Status (0/1) |
| V5 | Full Zone Status (Text) |

---

## 🤖 How It Works
1. Three **PIR sensors** continuously monitor Front, Middle, and Back zones of the classroom
2. When motion is detected in a zone, only **that zone's relay** activates (light + fan ON)
3. After last detected motion, **DHT22 temperature** determines how long the fan stays ON
4. When the delay expires and no new motion is detected, the relay switches OFF automatically
5. **LCD display** shows live temperature, humidity, and per-zone ON/OFF status
6. All data is pushed to **Blynk Cloud** every 10 seconds via Wi-Fi for remote monitoring

---

## 🧠 Zone Control Logic Flow
```
PIR Sensor Reads Motion
        |
        v
Motion Detected? ──── NO ──── Check Fan Delay Timer
        |                            |
       YES                    Delay Expired?
        |                       YES  |  NO
        v                        |   |
Relay ON (Light + Fan)      Relay OFF  Relay stays ON
        |
Read DHT22 Temperature
        |
        v
Set Fan Delay:
  > 30°C  → 9 min
  27-30°C → 6 min
  < 27°C  → 2 min
        |
        v
Upload to Blynk + Update LCD
```

---

## 🧪 Test Results
| Test Scenario | Expected Result | Observed Result |
|---|---|---|
| No motion in any zone | All relays OFF | ✅ All OFF |
| Motion in Front, Temp < 27°C | Front light ON, fan short delay | ✅ Correct |
| Motion in Middle, Temp > 27°C | Middle light + fan ON | ✅ Correct |
| No motion after delay expires | Relay turns OFF automatically | ✅ Correct |
| Multi-zone occupancy | Only occupied zones powered | ✅ Correct |

---

## 🗂️ Repository Structure
```
smart-classroom-control-esp32/
├── main/
│   └── smart_classroom.c        # Full ESP-IDF C Source Code
├── CMakeLists.txt               # ESP-IDF Build Config
└── README.md                    # Project Documentation
```

---

## 🚀 Getting Started

### 1. Clone the Repository
```bash
git clone https://github.com/YourUsername/smart-classroom-control-esp32.git
cd smart-classroom-control-esp32
```

### 2. Install ESP-IDF
Follow: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/

### 3. Configure Credentials
Open `main/smart_classroom.c` and update:
```c
#define WIFI_SSID        "YourWiFiSSID"
#define WIFI_PASSWORD    "YourWiFiPassword"
#define BLYNK_AUTH_TOKEN "YourBlynkAuthToken"
```

### 4. Build & Flash
```bash
idf.py build
idf.py -p COMx flash monitor
```

### 5. Setup Blynk Dashboard
- Add **Value Display** widgets → V0 (Temp), V1 (Humidity)
- Add **LED** widgets → V2, V3, V4 (Zone status indicators)
- Add **Label** widget → V5 (Full zone status text)

---

## 🔮 Future Scope
- Cloud-based occupancy analytics for energy audit reports
- Predictive scheduling based on class timetable integration
- Expansion to multi-room buildings using networked ESP32 nodes
- MQTT protocol for faster and more reliable IoT data transfer

---

## 👥 Team Members
| Name | Institution |
|---|---|
| Priyanshu Pandey | Lovely Professional University |
| Suyash Raj Tiwari | Lovely Professional University |
| Siddhant Singh | Lovely Professional University |

**Department:** Electronics & Communication Engineering
**Location:** Punjab – 144411, India
