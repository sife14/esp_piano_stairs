#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>
#include <LittleFS.h>
#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>

// --- USER SETTINGS ---
const char* AP_SSID = "Piano-Config";
const char* AP_PASS = "Piano1234"; 
#define WIFI_TIMEOUT_MS 300000 

// --- PINS ---
#define SENSOR_SDA 21
#define SENSOR_SCL 22

// --- GLOBALS ---
VL53L0X sensor;
AudioFileSourceLittleFS *file = nullptr;
AudioGeneratorWAV *wav = nullptr;
AudioOutputI2S *out = nullptr;
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

// Config Vars
int config_trigger_mm = 800;
int config_hysteresis_mm = 100; 
float config_volume = 1.0;
String active_note = "C"; 

// Multi-Tone & Loop Config
bool config_multitone = false; 
int config_note_spacing_mm = 50; 
bool config_loop = false; 

// State
bool isActive = false;
bool hasPlayedOnce = false; 
unsigned long lastSensorCheck = 0;
unsigned long bootTime = 0;
bool wifiActive = true;
int lastValidDistance = 9999;
String lastPlayedMultiNote = ""; 

// Notes List
const char* NOTES[] = {"C", "D", "E", "F", "G", "A", "H"};
const int NUM_NOTES = 7;

// --- HTML PAGE GENERATOR ---
String getHTML() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:sans-serif;margin:20px;max-width:600px;margin:auto;}";
  html += ".card{background:#f9f9f9;padding:15px;border:1px solid #ddd;border-radius:5px;margin-bottom:15px;}";
  html += "h3{margin:0 0 10px 0;} input[type=number]{width:80px;}";
  html += ".note-row{display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid #eee;}";
  html += ".status-ok{color:green;font-weight:bold;} .status-no{color:red;}";
  html += "</style><title>Piano Config</title></head><body>";
  
  html += "<h1>🎹 Piano Config</h1>";
  
  // 1. GLOBAL SETTINGS
  html += "<div class='card'><h3>⚙️ Settings</h3><form action='/save' method='POST'>";
  
  // Mode Selection
  String chkSingle = (!config_multitone) ? "checked" : "";
  String chkMulti = (config_multitone) ? "checked" : "";
  html += "<b>Mode:</b> <label><input type='radio' name='mode' value='0' " + chkSingle + "> Single Tone</label> ";
  html += "<label><input type='radio' name='mode' value='1' " + chkMulti + "> Multi Tone</label><br><br>";

  // Loop Selection
  String chkLoop = (config_loop) ? "checked" : "";
  html += "<b>Loop Sound:</b> <label><input type='checkbox' name='loop' value='1' " + chkLoop + "> Enable Looping</label><br><br>";

  html += "Volume (0.0 - 3.0): <input type='number' step='0.1' name='volume' value='" + String(config_volume) + "'><br>";
  html += "Trigger Dist (mm): <input type='number' name='trigger' value='" + String(config_trigger_mm) + "'><br>";
  html += "Hysteresis (mm): <input type='number' name='hyst' value='" + String(config_hysteresis_mm) + "'><br>";
  html += "Multi-Tone Spacing (mm): <input type='number' name='spacing' value='" + String(config_note_spacing_mm) + "'><br>";
  html += "<small>(Only for Multi-Tone: Trigger=C, +Spacing=D...)</small><br><br>";
  html += "<input type='submit' value='💾 Save Settings'></form></div>";

  // 2. ACTIVE TONE (Only relevant for Single Mode)
  if (!config_multitone) {
    html += "<div class='card'><h3>🎵 Active Tone (Single Mode)</h3><form action='/set_active' method='POST'>";
    html += "Note: <select name='note'>";
    for(int i=0; i<NUM_NOTES; i++){
      String n = NOTES[i];
      String sel = (n == active_note) ? "selected" : "";
      html += "<option value='" + n + "' " + sel + ">" + n + "</option>";
    }
    html += "</select> <input type='submit' value='Set Active'></form></div>";
  }

  // 3. FILE MANAGEMENT
  html += "<div class='card'><h3>📂 Manage Sounds</h3>";
  for(int i=0; i<NUM_NOTES; i++){
    String note = NOTES[i];
    String filename = "/" + note + ".wav";
    bool exists = LittleFS.exists(filename);
    
    html += "<div class='note-row'>";
    html += "<div><b>Note " + note + "</b> ";
    html += exists ? "<span class='status-ok'>[OK]</span>" : "<span class='status-no'>[Missing]</span>";
    html += "</div>";
    
    html += "<form method='POST' action='/upload?target=" + note + "' enctype='multipart/form-data'>";
    html += "<input type='file' name='upload' style='width:180px;'>";
    html += "<input type='submit' value='Upload'>";
    html += "</form></div>";
  }
  html += "</div></body></html>";
  return html;
}

// --- WEB HANDLERS ---
void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void handleSave() {
  if (server.hasArg("trigger")) config_trigger_mm = server.arg("trigger").toInt();
  if (server.hasArg("hyst")) config_hysteresis_mm = server.arg("hyst").toInt();
  if (server.hasArg("volume")) config_volume = server.arg("volume").toFloat();
  if (server.hasArg("spacing")) config_note_spacing_mm = server.arg("spacing").toInt();
  
  config_multitone = (server.arg("mode") == "1");
  config_loop = server.hasArg("loop"); 

  prefs.putInt("trigger", config_trigger_mm);
  prefs.putInt("hyst", config_hysteresis_mm);
  prefs.putFloat("volume", config_volume);
  prefs.putInt("spacing", config_note_spacing_mm);
  prefs.putBool("multi", config_multitone);
  prefs.putBool("loop", config_loop);
  
  if(out) out->SetGain(config_volume);
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetActive() {
  if (server.hasArg("note")) {
    active_note = server.arg("note");
    prefs.putString("note", active_note);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

File fsUploadFile;
void handleFileUpload(){
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String targetNote = server.arg("target");
    if(targetNote == "") targetNote = "Temp"; 
    fsUploadFile = LittleFS.open("/" + targetNote + ".wav", "w"); 
  } 
  else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
  } 
  else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) fsUploadFile.close();
    server.sendHeader("Location", "/");
    server.send(303);
  }
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  bootTime = millis();

  prefs.begin("piano", false);
  config_trigger_mm = prefs.getInt("trigger", 800);
  config_hysteresis_mm = prefs.getInt("hyst", 100);
  config_volume = prefs.getFloat("volume", 1.0);
  active_note = prefs.getString("note", "C");
  config_multitone = prefs.getBool("multi", false);
  config_note_spacing_mm = prefs.getInt("spacing", 50);
  config_loop = prefs.getBool("loop", false);

  Wire.begin(SENSOR_SDA, SENSOR_SCL);
  if(!LittleFS.begin(true)){ Serial.println("LittleFS Error"); }

  sensor.setTimeout(500);
  if (!sensor.init()) { Serial.println("Sensor Fail"); } 
  else { 
    sensor.setSignalRateLimit(0.1);
    sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
    sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
    sensor.startContinuous();
  }

  out = new AudioOutputI2S(0, 1);
  out->SetGain(config_volume);

  WiFi.softAP(AP_SSID, AP_PASS, 6, 0, 4);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/set_active", HTTP_POST, handleSetActive);
  server.on("/upload", HTTP_POST, [](){ server.send(200); }, handleFileUpload);
  server.onNotFound(handleRoot); 

  server.begin();
}

void playTone(String noteToPlay) {
  if (wav && wav->isRunning()) {
    // If same note and NOT multi-mode, return (prevent restart)
    if (active_note == noteToPlay && !config_multitone) return; 
    
    // Multi-Mode: If note is same as currently playing, return (let it sustain)
    if (config_multitone && lastPlayedMultiNote == noteToPlay) return; 
  }

  // Update tracking
  if(config_multitone) lastPlayedMultiNote = noteToPlay;
  
  String filename = "/" + noteToPlay + ".wav";
  if (!LittleFS.exists(filename)) return;

  if (file) delete file;
  if (wav) delete wav;

  file = new AudioFileSourceLittleFS(filename.c_str());
  wav = new AudioGeneratorWAV();
  wav->begin(file, out);
}

void stopTone() {
  if (wav) {
    if (wav->isRunning()) wav->stop();
    delete wav; wav = nullptr;
  }
  if (file) { delete file; file = nullptr; }
  
  // FIX: Only clear memory if user has actually left the area
  if (!isActive) {
      lastPlayedMultiNote = ""; 
  }
}

void loop() {
  // 1. WiFi Logic
  if (wifiActive) {
    dnsServer.processNextRequest();
    server.handleClient();
    if (WiFi.softAPgetStationNum() > 0) bootTime = millis();
    if (millis() - bootTime > WIFI_TIMEOUT_MS) {
      WiFi.softAPdisconnect(true);
      wifiActive = false;
      WiFi.mode(WIFI_OFF);
    }
  }

  // 2. Audio Engine
  if (wav && wav->isRunning()) {
    if (!wav->loop()) {
      // Audio finished
      if (config_loop && isActive) {
        // Loop is ON -> Restart
        String restartNote = config_multitone ? lastPlayedMultiNote : active_note;
        playTone(restartNote);
      } else {
        // Loop is OFF -> Stop and mark as done
        stopTone();
        hasPlayedOnce = true; 
      }
    }
  }

  // 3. Sensor Check
  if (millis() - lastSensorCheck > 50) {
    lastSensorCheck = millis();
    uint16_t rawDist = sensor.readRangeContinuousMillimeters();
    int dist = rawDist;
    static int err = 0;

    if (sensor.timeoutOccurred() || rawDist > 8000) {
      err++;
      if (err > 10) { dist = 9999; lastValidDistance = 9999; }
      else dist = lastValidDistance;
    } else {
      err = 0;
      lastValidDistance = dist;
    }

    // --- LOGIC ---
    if (dist < config_trigger_mm) {
      // New Entry Event
      if (!isActive) {
        isActive = true;
        hasPlayedOnce = false; 
      }
      
      String targetNote = active_note; 

      if (config_multitone) {
        int diff = config_trigger_mm - dist; 
        if (diff < 0) diff = 0;
        int noteIndex = diff / config_note_spacing_mm;
        if (noteIndex >= NUM_NOTES) noteIndex = NUM_NOTES - 1; 
        targetNote = NOTES[noteIndex];

        // If user moved hand to a NEW note, allow playing again (even if loop off)
        if (targetNote != lastPlayedMultiNote) {
             hasPlayedOnce = false; 
        }
      }
      
      // Play ONLY if allowed
      if (!hasPlayedOnce) {
        playTone(targetNote);
      }
    }
    else if (isActive && dist > (config_trigger_mm + config_hysteresis_mm)) {
      isActive = false;
      stopTone();
      // Ensure memory is cleared on exit
      lastPlayedMultiNote = ""; 
      hasPlayedOnce = false; 
    }
  }
}
