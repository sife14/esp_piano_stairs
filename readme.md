# ESP32 Smart Piano Trigger (VL53L0X)

This project turns an ESP32 into an interactive “piano key”. When someone steps in front of a VL53L0X (GY-53) distance sensor (distance below a threshold), the ESP32 plays a WAV file from LittleFS. When the person leaves (threshold + hysteresis), playback stops.

## Features

- VL53L0X distance trigger with hysteresis
- WAV playback from LittleFS (`/piano.wav`)
- Debug output: current distance, PLAY/STOP events
- Simple sensor glitch filter (ignores short “out of range” bursts)

## Hardware

### Parts
- ESP32 DevKit (powered via USB)
- GY-53 / VL53L0X ToF sensor (I2C)
- Small speaker (e.g. 8Ω / 0.25W)
- **Option A (quick & dirty):** BC546B + base resistor (recommended: 1kΩ)
- **Option B (recommended):** PAM8403 amplifier module (cleaner, louder)

### Wiring (Sensor)
| Sensor | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### Wiring (Audio) – Option A: BC546B (simple transistor driver)
**BC546B pinout (flat side facing you):** Left=C, Middle=B, Right=E

ESP32 VIN (5V)  -----> Speaker (+)
Speaker (-)     -----> BC546B Collector (C)
BC546B Emitter (E) -----> ESP32 GND
ESP32 GPIO25 (DAC) --[1kΩ]--> BC546B Base (B)

#### Optimization idea: reduce clicking / popping
A simple transistor+speaker setup tends to “pop” when starting/stopping due to DC offsets / abrupt waveform steps.

- Best fix: use a small amplifier board (PAM8403).
- Alternative: add a coupling capacitor in series with the speaker (e.g. 100µF–470µF), and/or ensure the WAV has tiny fade-in/out.

### Wiring (Audio) – Option B: PAM8403 (recommended)
ESP32 VIN (5V)  -----> PAM8403 VCC
ESP32 GND       -----> PAM8403 GND
ESP32 GPIO25    -----> PAM8403 INL (or INR)
PAM8403 OUTL+/OUTL- -> Speaker

## Audio file requirements (`piano.wav`)

Put the file here:
project/
  data/
    piano.wav

Recommended WAV format for best compatibility:
- Mono
- 16-bit PCM
- Sample rate: 22050 Hz (or 16000 Hz)
- Keep it short enough to fit into LittleFS
- files can be converted with https://audio.online-convert.com/convert-to-wav

To reduce “clicks” inside the sample:
- Apply a very short fade-in and fade-out (e.g. 5–10 ms)
- Ensure the waveform starts/ends near zero crossing


## Build & upload steps

1. Place `piano.wav` into the `data/` folder.
2. Upload the filesystem image:
   - PlatformIO: Project Tasks → esp32dev → Platform → Upload Filesystem Image
3. Upload firmware:
   - PlatformIO: Upload
4. Open serial monitor at 115200 and watch debug output.

## Runtime behavior (debug)

- Prints distance updates (filtered) every time it changes significantly
- Prints:
  - `>> PLAY` when triggered
  - `<< STOP` when leaving range

## Troubleshooting

### Sensor prints values but playback never starts
- Your trigger threshold is too low/high. If you see distances like 2000mm, set trigger higher or move closer.

### No sound
- Verify `piano.wav` exists and you uploaded filesystem image.
- Verify GPIO25 is connected to your amplifier/driver.
