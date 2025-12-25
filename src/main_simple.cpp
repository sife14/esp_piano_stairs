/**
 * ESP32 Smart Piano Trigger
 * -------------------------
 * Plays a WAV sound when a person triggers the VL53L0X distance sensor.
 * Includes "Anti-Pop" strategy and simple jitter filter.
 * 
 * Hardware:
 * - ESP32 DevKit V1
 * - VL53L0X Distance Sensor (I2C)
 * - Transistor Amplifier (BC546B) or PAM8403 Module
 * - 8 Ohm Speaker
 */

#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>  
#include <LittleFS.h>
#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>

// --- CONFIGURATION ---
int config_trigger_mm = 800;     // Trigger distance in mm (max 2000 for VL53L0X)
int config_hysteresis_mm = 100;  // Stop distance = trigger + hysteresis
float config_volume = 1.0;       // Volume gain (0.0 to 4.0)

// Pins
#define SENSOR_SDA 21
#define SENSOR_SCL 22

// Objects
VL53L0X sensor;
AudioFileSourceLittleFS *file = nullptr;
AudioGeneratorWAV *wav = nullptr;
AudioOutputI2S *out = nullptr;

bool isActive = false;
unsigned long lastSensorCheck = 0;
uint16_t lastDebugDistance = 0; 
int lastValidDistance = 9999; 

void setup() {
  Serial.begin(115200);
  
  // Initialize I2C
  Wire.begin(SENSOR_SDA, SENSOR_SCL);
  delay(100); 

  // Mount Filesystem
  if (!LittleFS.begin()) {
    Serial.println("[ERR] LittleFS Mount Failed. Did you upload the FS image?");
  }

  // Initialize Sensor
  Serial.print("[INIT] Sensor VL53L0X... ");
  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println("FAILED! Check wiring.");
  } else {
    Serial.println("OK!");
    // Configure for Long Range (better for human detection ~1-2m)
    // Increases sensitivity but measurement takes ~33ms
    sensor.setSignalRateLimit(0.1);
    sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
    sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
    sensor.startContinuous();
  }

  // Initialize Audio Output ONCE
  // Keeping the object alive prevents DAC voltage drops (clicking noise)
  out = new AudioOutputI2S(0, 1); // 1 = Internal DAC (GPIO 25)
  out->SetGain(config_volume);
  
  Serial.println("[SYS] Ready.");
}

void playTone() {
  // If already running, do nothing
  if (wav && wav->isRunning()) return;

  Serial.println(">> PLAY");
  
  // Clean up old file object if exists
  if (file) delete file;
  if (wav) delete wav;

  // Create new instances
  file = new AudioFileSourceLittleFS("/piano.wav");
  wav = new AudioGeneratorWAV();
  
  // Start playing
  wav->begin(file, out);
}

void stopTone() {
  if (wav) {
    if (wav->isRunning()) {
      Serial.println("<< STOP");
      // Stop decoding. 
      // Note: We do NOT delete the 'out' object to maintain bias voltage on the speaker line.
      wav->stop(); 
    }
    delete wav; 
    wav = nullptr;
  }
  
  if (file) { 
    delete file; 
    file = nullptr; 
  }
}

void loop() {
  // 1. Audio Engine Loop
  if (wav && wav->isRunning()) {
    // If file ends naturally, stop it cleanly
    if (!wav->loop()) {
       stopTone();
    }
  }

  // 2. Sensor Loop (run every 50ms)
  if (millis() - lastSensorCheck > 50) {
    lastSensorCheck = millis();
    
    uint16_t rawDist = sensor.readRangeContinuousMillimeters();
    int dist = rawDist;

    // --- Simple Jitter Filter ---
    // The VL53L0X sometimes returns 0 or >8000 on timeout/error.
    // We ignore these errors for a short time to prevent flickering.
    static int errorCounter = 0;
    
    if (sensor.timeoutOccurred() || rawDist > 8000) {
      errorCounter++;
      if (errorCounter > 10) { // If error persists > 500ms
        dist = 9999; // Treat as "far away"
        lastValidDistance = 9999;
      } else {
        dist = lastValidDistance; // Use last known good value
      }
    } else {
      errorCounter = 0;
      lastValidDistance = dist;
    }

    // --- Debug Output ---
    // Only print if value changes significantly (>50mm) to reduce serial spam
    if (abs((int)dist - (int)lastDebugDistance) > 50) {
       Serial.printf("Dist: %d mm\n", dist);
       lastDebugDistance = dist;
    }

    // --- Logic ---
    if (!isActive && dist < config_trigger_mm) {
      isActive = true;
      playTone();
    }
    else if (isActive && dist > (config_trigger_mm + config_hysteresis_mm)) {
      isActive = false;
      stopTone();
    }
  }
}
