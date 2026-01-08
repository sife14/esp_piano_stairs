# ESP32 Smart Piano Trigger

This project turns an ESP32 into an interactive "piano key". It plays a sound when someone steps in front of a distance sensor (VL53L0X).

There are two versions included in this project:
1.  **Simple Version:** Hardcoded single sound, no WiFi.
2.  **Advanced Version:** Web interface for configuration, multiple sounds (C, D, E...), WiFi AP with Captive Portal.

---

## 🚀 Quick Start (PlatformIO)

This project uses **two separate environments**. Choose the one you want to upload in PlatformIO:

### 1. Simple Version (`env:main_easy`)
- **File:** `src/main_simple.cpp`
- **Features:**
    - Plays `/piano.wav` from LittleFS.
    - Config (Trigger distance, Volume) is hardcoded in `main_simple.cpp`.
    - No WiFi, instant start.

### 2. Advanced Version (`env:main`)
- **File:** `src/main.cpp`
- **Features:**
    - **WiFi Access Point:** Creates "Piano-Config" (Pass: `Piano1234`) for 5 minutes after boot.
    - **Captive Portal:** Automatically opens settings page on phone connection.
    - **Web Interface:** Set Trigger Distance, Volume, and upload new sounds via browser.
    - **Multi-Sound:** Upload sounds for notes C, D, E, F, G, A, H and select the active one.
    - **Persistence:** Settings are saved permanently (even after reboot).

**To upload:**
- In VS Code (PlatformIO): Project Tasks -> **env:main** (or main_easy) -> **Upload**.
- **Don't forget:** Upload Filesystem Image first!

---

## 🛠 Hardware Setup

### Parts
- **ESP32 DevKit V1**
- **GY-53 (VL53L0X)** Time-of-Flight Sensor
- **Speaker** (8 Ohm / ?? W)
- **Amplifier:** PAM8403 Module (Recommended) OR Transistor (BC546B)

### Wiring (Sensor)
| Sensor | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### Wiring (Audio - Recommended)
Using a **PAM8403** amplifier eliminates "popping/clicking" noise and is much louder.
ESP32 VIN (5V)  -----> PAM8403 VCC
ESP32 GND       -----> PAM8403 GND
ESP32 GPIO25    -----> PAM8403 L_IN
PAM8403 L_OUT   -----> Speaker

### Wiring (Audio - Simple Transistor)
*Note: This circuit is quiet and may cause clicking sounds on start/stop.*
ESP32 VIN (5V)  -----> Speaker (+)
Speaker (-)     -----> BC546B Collector
ESP32 GND       -----> BC546B Emitter
ESP32 GPIO25    --[1kΩ]-- BC546B Base

---

## 🎵 Audio Files

- **Format:** WAV, Mono, 16-bit PCM, 22050 Hz (or 16000 Hz). (can be converted using https://audio.online-convert.com/convert-to-wav)
- **Location:** Put default files in `data/` folder (e.g., `C.wav`, `piano.wav`).
- **Optimization:** Add a short Fade-In (10ms) and Fade-Out (10ms) to your WAV files to prevent clicking noises.

---

## 📂 Project Structure

├── platformio.ini      # Configuration for env:main and env:main_easy
├── data/               # WAV files (Upload this via "Upload Filesystem Image")
│   ├── piano.wav       # Default for simple version
│   └── C.wav           # Default note for advanced version
└── src/
    ├── main.cpp        # Code for Advanced Version
    └── main_simple.cpp # Code for Simple Version
