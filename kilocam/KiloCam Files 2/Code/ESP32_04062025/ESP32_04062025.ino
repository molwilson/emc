#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include <esp_camera.h>
#include <Arduino.h>
#include <FS.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include "driver/rtc_io.h"
#include <CRC32.h>
#include <SPI.h>
#include <SD.h>
#include <esp_task_wdt.h>

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Define SPI pins for 1-bit mode
#define SD_CS 13     // Chip Select pin
#define SPI_SCK 14   // Clock pin
#define SPI_MISO 2   // MISO (Data In) pin
#define SPI_MOSI 15  // MOSI (Data Out) pin

const char* ap_ssid = "kilocam-config";
const char* ap_password = "kilocam";

// Add these global variables
bool isOperational = false;
bool isProgramming = false;

AsyncWebServer server(80);
const char* SETTINGS_FILE = "/settings.json";

// Settings structure
struct Settings {
  String initial_time_hour = "00";
  String initial_time_minute = "00";
  int interval = 30;
  int photos_per_capture = 5;
  String sunrise_hour = "06";
  String sunrise_minute = "00";
  String sunset_hour = "18";
  String sunset_minute = "00";
  String capture_mode = "daylight";
  String client_date = "2024-01-01";
  String client_time = "00:00:00";
  int light_intensity = 50;   // Default 50%
  bool toggle_state = false;  // New toggle state
  uint32_t checksum = 0;
};
Settings currentSettings;


#define PAYLOAD_SIZE 128
char PAYLOAD[PAYLOAD_SIZE];
char TIMESTAMP[15];
char NUMPICS[5];
int NumPics = 5;  // Initialize NumPics variable to one. DO NOT ADJUST.
int LIGHT = 0;
float TEMP = 0.0;

#define flashled 4

// Add these global variables
bool sdCardPresent = false;
bool modeInitialized = false;
bool captureTriggered = false;
bool isdatasent = false;

enum SystemMode {
  PROGRAMMING_MODE,
  OPERATIONAL_MODE
};

enum LEDPattern {
  NONE,
  PROGRAMMING_PATTERN,
  OPERATIONAL_PATTERN,
  ACKNOWLEDGEMENT_PATTERN
};
LEDPattern ledPattern = PROGRAMMING_PATTERN;

SystemMode currentMode = PROGRAMMING_MODE;

unsigned long lastKiloCamCheck = 0;
unsigned long lastModeCheck = 0;
unsigned long lastBlink = 0;
//const unsigned long MODE_CHECK_INTERVAL = 5000;
const unsigned long BLINK_INTERVAL_PROGRAMMING = 1000;
const unsigned long BLINK_INTERVAL_OPERATIONAL = 200;
const unsigned long BLINK_INTERVAL_ACKNOWLEDGEMENT = 100;
const unsigned long KILOCAM_CHECK_INTERVAL = 5000;

const String MODE_OPERATIONAL = "@M_O@";  // Unique operational command
const String MODE_PROGRAMMING = "@M_P@";  // Unique programming command

void blinkLED() 
{
  static unsigned long lastSwitch = 0;
  static bool ledState = false;

  if (ledPattern != NONE) {
    unsigned long now = millis();
    int interval = (ledPattern == ACKNOWLEDGEMENT_PATTERN) ? BLINK_INTERVAL_ACKNOWLEDGEMENT : (currentMode == PROGRAMMING_MODE) ? BLINK_INTERVAL_PROGRAMMING
                                                                                                                                : BLINK_INTERVAL_OPERATIONAL;

    // Only blink in PROGRAMMING_MODE if toggle is OFF
    // In OPERATIONAL_MODE, only blink if toggle is ON
    if ((currentMode == PROGRAMMING_MODE) || 
        (currentMode == OPERATIONAL_MODE && currentSettings.toggle_state)) {
      if (now - lastSwitch >= interval) {
        ledState = !ledState;
        digitalWrite(flashled, ledState);
        lastSwitch = now;
      }
    } else {
      // If in OPERATIONAL_MODE and toggle is OFF, keep LED off
      digitalWrite(flashled, LOW);
    }
  } else {
    digitalWrite(flashled, LOW);
  }
}

void ledcontrolFunction() {
  if (currentSettings.toggle_state) {
    // LED enabled - will blink in both PROGRAMMING_MODE and OPERATIONAL_MODE
    Serial.println("LED is enabled for all modes");
  } else {
    // LED disabled - will only blink in PROGRAMMING_MODE
    Serial.println("LED is disabled for OPERATIONAL_MODE");
  }
}

// Function to turn on Wi-Fi
void turnOnWiFi() {
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(apIP);
}

// Function to turn off Wi-Fi to save power
void turnOffWiFi() {
  WiFi.mode(WIFI_OFF);  // Disable Wi-Fi
  //Serial.println("Wi-Fi turned off to save power!");
}

// Function to turn on BLE
void turnOnBLE() {
  btStart();
  //Serial.println("BLE started!");
}

// Function to turn off BLE to save power
void turnOffBLE() {
  btStop();  // Disable the entire Bluetooth controller (Classic Bluetooth and BLE)
  //Serial.println("BLE turned off to save power!");
}

bool isSDCardPresent() {
  // Initialize SPI for 1-bit mode
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);

  // Try to initialize the SD card in 1-bit SPI mode
  if (!SD.begin(SD_CS)) {
    //Serial.println("SD card initialization failed!");
    return false;
  }

  // Check if root directory exists
  File root = SD.open("/");
  if (!root) {
    SD.end();  // Close and end if directory not found
    return false;
  }
  root.close();

  Serial.println("SD Card detected in 1-bit SPI mode");
  return true;
}

void setupProgrammingMode() {
  // Set WiFi to Station mode (less CPU intensive than AP+STA)
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);

  // Start BLE only if needed
  turnOnBLE();

  ledPattern = PROGRAMMING_PATTERN;
  server.reset();

  // Add handlers with decreased stack usage
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    handleRoot(request);
  });

  server.on("/test", HTTP_GET, [](AsyncWebServerRequest* request) {
    handleTest(request);
  });

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest* request) {
    handleUpdate(request);
  });

  server.on("/sync-time", HTTP_POST, [](AsyncWebServerRequest* request) {
    handleSyncTime(request);
  });

  server.on("/set-light", HTTP_POST, [](AsyncWebServerRequest* request) {
    handleSetLight(request);
  });

  server.on("/toggle", HTTP_POST, [](AsyncWebServerRequest* request) {
    handleToggle(request);
  });

  // Set lower priority for AsyncTCP task
  server.begin();
}

void handleToggle(AsyncWebServerRequest* request) {
  // Debug the toggle action
  Serial.print("Toggle: Changing state from ");
  Serial.print(currentSettings.toggle_state ? "ON" : "OFF");

  // Toggle the current state
  currentSettings.toggle_state = !currentSettings.toggle_state;

  Serial.print(" to ");
  Serial.println(currentSettings.toggle_state ? "ON" : "OFF");

  // Save to SPIFFS
  File configFile = SPIFFS.open(SETTINGS_FILE, "w");
  if (configFile) {
    JsonDocument doc;
    // Save all current settings
    doc["initial_time_hour"] = currentSettings.initial_time_hour;
    doc["initial_time_minute"] = currentSettings.initial_time_minute;
    doc["interval"] = currentSettings.interval;
    doc["photos_per_capture"] = currentSettings.photos_per_capture;
    doc["sunrise_hour"] = currentSettings.sunrise_hour;
    doc["sunrise_minute"] = currentSettings.sunrise_minute;
    doc["sunset_hour"] = currentSettings.sunset_hour;
    doc["sunset_minute"] = currentSettings.sunset_minute;
    doc["capture_mode"] = currentSettings.capture_mode;
    doc["client_date"] = currentSettings.client_date;
    doc["client_time"] = currentSettings.client_time;
    doc["light_intensity"] = currentSettings.light_intensity;
    doc["toggle_state"] = currentSettings.toggle_state;

    serializeJson(doc, configFile);
    configFile.close();
    Serial.println("Toggle state saved to SPIFFS");
  } else {
    Serial.println("Failed to open settings file for writing");
  }

  // Send response
  request->send(200, "application/json", "{\"state\":" + String(currentSettings.toggle_state ? "true" : "false") + "}");
}


void setupOperationalMode() {
  server.end();
  turnOffWiFi();
  turnOffBLE();
  ledPattern = OPERATIONAL_PATTERN;
  WiFi.mode(WIFI_OFF);
  btStop();
  //Serial.println("Operational mode activated");
}

void checkMode() {
  static unsigned long lastCheckTime = 0;
  const unsigned long checkInterval = 2000;  // Check every 2 seconds

  static bool firstRun = true;              // Track first run after boot
  static bool previousSDCardState = false;  // Track previous SD card state

  if (millis() - lastCheckTime < checkInterval) {
    return;
  }

  lastCheckTime = millis();
  bool currentSDCardState = isSDCardPresent();
  //bool currentSDCardState = true;

  //Serial.printf("SD Card check: %s\n", currentSDCardState ? "Present" : "Absent");
  //Serial.printf("Current Mode: %s\n", currentMode == PROGRAMMING_MODE ? "Programming" : "Operational");

  // Force setup on first run if no SD card
  if (firstRun && !currentSDCardState) {
    firstRun = false;
    if (currentMode != PROGRAMMING_MODE) {
      Serial.println("Forcing Programming Mode setup on first check...");
      setupProgrammingMode();
      currentMode = PROGRAMMING_MODE;
    }
  }

  // Handle state changes
  if (currentSDCardState != previousSDCardState) {
    if (currentSDCardState) {
      // SD card inserted: Switch to operational mode
      if (currentMode != OPERATIONAL_MODE) {
        Serial.println("Switching to Operational Mode...");
        setupOperationalMode();
        currentMode = OPERATIONAL_MODE;
      }
    } else {
      // SD card removed: Switch to programming mode
      if (currentMode != PROGRAMMING_MODE) {
        Serial.println("Switching to Programming Mode...");
        setupProgrammingMode();
        currentMode = PROGRAMMING_MODE;
      }
    }
    previousSDCardState = currentSDCardState;
  }
}


void indicateAcknowledgement() {
  ledPattern = ACKNOWLEDGEMENT_PATTERN;
  delay(1000);  // Keep the pattern active for 1 second
  ledPattern = (currentMode == PROGRAMMING_MODE) ? PROGRAMMING_PATTERN : OPERATIONAL_PATTERN;
}

String createPacket(Settings& currentSettings) {
  String payload;
  payload.reserve(200);
  payload += "ITH=" + currentSettings.initial_time_hour;
  payload += ";ITM=" + currentSettings.initial_time_minute;
  payload += ";INT=" + String(currentSettings.interval);
  payload += ";PPC=" + String(currentSettings.photos_per_capture);
  payload += ";SRH=" + currentSettings.sunrise_hour;
  payload += ";SRM=" + currentSettings.sunrise_minute;
  payload += ";SSH=" + currentSettings.sunset_hour;
  payload += ";SSM=" + currentSettings.sunset_minute;
  payload += ";MODE=" + currentSettings.capture_mode;
  payload += ";CDATE=" + currentSettings.client_date;
  payload += ";CTIME=" + currentSettings.client_time;

  // Calculate checksum and store in struct
  CRC32 crc;
  crc.reset();
  for (size_t i = 0; i < payload.length(); i++) {
    crc.update(payload[i]);
  }
  currentSettings.checksum = crc.finalize();

  // Build packet
  String packet;
  packet.reserve(220);
  packet += "<CS=";
  packet += String(currentSettings.checksum);
  packet += ";";
  packet += payload;
  packet += ">";

  return packet;
}

void sendSettingsToKiloCam(Settings& currentSettings) {
  String packet = createPacket(currentSettings);
  Serial.println(packet);
  Serial.flush();
  //Serial.print("Sent: ");
  //Serial.println(packet);
}
void handleSyncTime(AsyncWebServerRequest* request) {
  // Get values from the form first to validate
  String clientDate = request->arg("client_date");
  String clientTime = request->arg("client_time");

  // Validate parameters before proceeding
  if (clientDate.isEmpty() || clientTime.isEmpty()) {
    Serial.println("Error: Missing date or time parameters");
    request->send(400, "text/plain", "Missing parameters");
    return;
  }

  // Update settings structure with the new date/time
  currentSettings.client_date = clientDate;
  currentSettings.client_time = clientTime;

  // Clear serial buffer first to ensure clean communication
  while (Serial.available()) Serial.read();

  // Step 1: Send TIME command to indicate time sync mode
  Serial.println("TIME");
  Serial.flush();  // Wait for data to be transmitted

  // Step 2: Delay to ensure Arduino has processed the command
  delay(500);

  // Step 3: Create and send the settings packet with updated time
  String packet = createPacket(currentSettings);
  Serial.println(packet);
  Serial.flush();

  // Step 4: Wait for response with timeout
  unsigned long startTime = millis();
  bool receivedResponse = false;
  String response = "";

  while (millis() - startTime < 2000) {  // 2 second timeout
    if (Serial.available()) {
      char c = Serial.read();
      response += c;

      // Look for RTC update confirmation
      if (response.indexOf("RTC updated") != -1) {
        receivedResponse = true;
        break;
      }
    }
    delay(10);  // Short delay to prevent CPU hogging
  }

  // Log the outcome
  if (receivedResponse) {
    Serial.println("Time sync successful");
  } else {
    Serial.println("Time sync response timeout");
  }

  // Send response back to client
  request->send(200, "text/plain", "OK");
}

// IMPROVED LIGHT CONTROL HANDLER FOR ESP32-CAM

void handleSetLight(AsyncWebServerRequest* request) {
  if (request->hasParam("value", true)) {
    int value = request->getParam("value", true)->value().toInt();
    value = constrain(value, 0, 100);

    // Update currentSettings with new light value
    currentSettings.light_intensity = value;

    // Save to SPIFFS using existing file handle
    File configFile = SPIFFS.open(SETTINGS_FILE, "w");
    if (configFile) {
      JsonDocument doc;
      // Save all current settings
      doc["initial_time_hour"] = currentSettings.initial_time_hour;
      doc["initial_time_minute"] = currentSettings.initial_time_minute;
      doc["interval"] = currentSettings.interval;
      doc["photos_per_capture"] = currentSettings.photos_per_capture;
      doc["sunrise_hour"] = currentSettings.sunrise_hour;
      doc["sunrise_minute"] = currentSettings.sunrise_minute;
      doc["sunset_hour"] = currentSettings.sunset_hour;
      doc["sunset_minute"] = currentSettings.sunset_minute;
      doc["capture_mode"] = currentSettings.capture_mode;
      doc["client_date"] = currentSettings.client_date;
      doc["client_time"] = currentSettings.client_time;
      doc["light_intensity"] = currentSettings.light_intensity;

      serializeJson(doc, configFile);
      configFile.close();
    }

    // Send a brief response immediately
    request->send(200, "text/plain", "OK");

    // Send the light command after responding
    String lightCommand = "L:" + String(value) + "\n";
    Serial.print(lightCommand);
    Serial.flush();
  } else {
    request->send(400, "text/plain", "Invalid value");
  }
}

// IMPROVED SETTINGS UPDATE HANDLER FOR ESP32-CAM

void handleUpdate(AsyncWebServerRequest* request) {
  // Extract all parameters from the request
  String initialTimeHour = request->arg("initial_time_hour");
  String initialTimeMinute = request->arg("initial_time_minute");
  String sunriseHour = request->arg("sunrise_hour");
  String sunriseMinute = request->arg("sunrise_minute");
  String sunsetHour = request->arg("sunset_hour");
  String sunsetMinute = request->arg("sunset_minute");
  int interval = request->arg("interval").toInt();
  int photos = request->arg("photos").toInt();
  String captureMode = request->arg("capture_mode");
  String clientDate = request->arg("client_date");
  String clientTime = request->arg("client_time");

  // Validate all parameters
  if (initialTimeHour.isEmpty() || initialTimeMinute.isEmpty() || interval < 1 || photos < 1 || sunriseHour.isEmpty() || sunriseMinute.isEmpty() || sunsetHour.isEmpty() || sunsetMinute.isEmpty() || captureMode.isEmpty() || clientDate.isEmpty() || clientTime.isEmpty()) {
    request->send(400, "text/plain", "Missing or invalid parameters");
    return;
  }

  // Update settings structure
  currentSettings.initial_time_hour = initialTimeHour;
  currentSettings.initial_time_minute = initialTimeMinute;
  currentSettings.interval = interval;
  currentSettings.photos_per_capture = photos;
  currentSettings.sunrise_hour = sunriseHour;
  currentSettings.sunrise_minute = sunriseMinute;
  currentSettings.sunset_hour = sunsetHour;
  currentSettings.sunset_minute = sunsetMinute;
  currentSettings.capture_mode = captureMode;
  currentSettings.client_date = clientDate;
  currentSettings.client_time = clientTime;

  // Save settings to SPIFFS
  JsonDocument doc;
  doc["initial_time_hour"] = currentSettings.initial_time_hour;
  doc["initial_time_minute"] = currentSettings.initial_time_minute;
  doc["interval"] = currentSettings.interval;
  doc["photos_per_capture"] = currentSettings.photos_per_capture;
  doc["sunrise_hour"] = currentSettings.sunrise_hour;
  doc["sunrise_minute"] = currentSettings.sunrise_minute;
  doc["sunset_hour"] = currentSettings.sunset_hour;
  doc["sunset_minute"] = currentSettings.sunset_minute;
  doc["capture_mode"] = currentSettings.capture_mode;
  doc["client_date"] = currentSettings.client_date;
  doc["client_time"] = currentSettings.client_time;
  doc["light_intensity"] = currentSettings.light_intensity;
  doc["toggle_state"] = currentSettings.toggle_state;

  File configFile = SPIFFS.open(SETTINGS_FILE, "w");
  if (!configFile) {
    request->send(500, "text/plain", "Failed to save settings to SPIFFS");
    return;
  }

  serializeJson(doc, configFile);
  configFile.close();

  // Clear serial buffer first
  while (Serial.available()) Serial.read();

  // Send the new settings to KiloCam
  String packet = createPacket(currentSettings);
  Serial.println(packet);
  Serial.flush();

  // Wait for confirmation with timeout
  unsigned long startTime = millis();
  bool receivedConfirmation = false;
  String response = "";

  while (millis() - startTime < 2000) {  // 2 second timeout
    if (Serial.available()) {
      char c = Serial.read();
      response += c;

      // Look for settings confirmation message
      if (response.indexOf("Settings Stored") != -1) {
        receivedConfirmation = true;
        break;
      }
    }
    delay(10);  // Short delay to prevent CPU hogging
  }

  // Log the outcome
  if (receivedConfirmation) {
    Serial.println("Settings update confirmed by KiloCam");
  } else {
    Serial.println("Settings update response timeout");
  }

  // Redirect back to main page
  request->redirect("/?success=1");
}

void handleTest(AsyncWebServerRequest* request) {
  Serial.println("Button pressed");
  camera_fb_t* fb = esp_camera_fb_get();

  if (!fb || fb->format != PIXFORMAT_JPEG) {
    if (fb) esp_camera_fb_return(fb);
    request->send(500, "text/plain", "Camera error");
    return;
  }

  AsyncWebServerResponse* response = request->beginResponse_P(200, "image/jpeg", fb->buf, fb->len);

  // Add cleanup handler using onDisconnect
  request->onDisconnect([fb]() {
    esp_camera_fb_return(fb);
  });

  response->addHeader("Content-Disposition", "inline; filename=capture.jpg");
  response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response->addHeader("Pragma", "no-cache");
  response->addHeader("Expires", "0");

  request->send(response);
}
void handleRoot(AsyncWebServerRequest* request) {
  AsyncResponseStream* response = request->beginResponseStream("text/html");

  const char* htmlTemplate = R"rawliteral(
    <!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Wildlife Camera Configuration</title>
  <style>
    /* Modern Color Scheme */
    :root {
      --primary: #2c3e50;
      --secondary: #3498db;
      --success: #27ae60;
      --background: #f8f9fa;
      --text: #2c3e50;
    }

    /* Base Styles */
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }

    body {
      font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
      background-color: var(--background);
      color: var(--text);
      line-height: 1.6;
      min-height: 100vh;
      padding: 2rem;
      display: flex;
      justify-content: center;
      align-items: center;
    }

    /* Layout */
    .container {
      background: white;
      border-radius: 12px;
      box-shadow: 0 8px 30px rgba(0,0,0,0.12);
      width: 100%;
      max-width: 1200px;
      margin: 2rem;
      overflow: hidden;
      display: grid;
      grid-template-columns: 1fr 1.5fr;
      min-height: 80vh;
    }

    @media (max-width: 768px) {
      .container {
        grid-template-columns: 1fr;
        margin: 1rem;
      }
    }

    /* Sidebar */
    .sidebar {
      background: var(--primary);
      padding: 3rem 2rem;
      color: white;
      position: relative;
      display: flex;
      flex-direction: column;
      justify-content: space-between; /* This pushes content to top and bottom */
    }

    .sidebar-top {
      /* Content at the top of the sidebar */
    }

    .sidebar-bottom {
      /* Content at the bottom of the sidebar */
      margin-top: auto;
      padding-top: 2rem;
      border-top: 1px solid rgba(255, 255, 255, 0.2);
    }

    .branding {
      margin-bottom: 3rem;
    }

    .branding h1 {
      font-size: 1.8rem;
      margin-bottom: 0.5rem;
    }

    .branding p {
      opacity: 0.9;
      font-size: 0.9rem;
    }

    /* Form Section */
    .form-section {
      padding: 3rem 2rem;
    }

    .form-header {
      margin-bottom: 2.5rem;
    }

    .form-header h2 {
      font-size: 1.8rem;
      margin-bottom: 0.5rem;
      color: var(--primary);
    }

    .form-header p {
      color: #666;
    }

    /* Form Elements */
    .form-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 1.5rem;
    }

    .form-group {
      margin-bottom: 1.5rem;
    }

    label {
      display: block;
      font-weight: 600;
      margin-bottom: 0.5rem;
      font-size: 0.9rem;
    }

    input, select {
      width: 100%;
      padding: 0.8rem;
      border: 2px solid #e0e0e0;
      border-radius: 6px;
      font-size: 1rem;
      transition: border-color 0.3s ease;
    }

    input:focus, select:focus {
      outline: none;
      border-color: var(--secondary);
      box-shadow: 0 0 0 3px rgba(52, 152, 219, 0.1);
    }

    button {
      background: var(--secondary);
      color: white;
      padding: 1rem 2rem;
      border: none;
      border-radius: 6px;
      font-size: 1rem;
      font-weight: 600;
      cursor: pointer;
      transition: transform 0.2s ease, background 0.3s ease;
      width: 100%;
      margin-top: 1rem;
    }

    button:hover {
      background: #2980b9;
      transform: translateY(-1px);
    }

    /* Success Message */
    .alert-success {
      background: #e8f6ef;
      color: var(--success);
      padding: 1rem;
      border-radius: 6px;
      margin-bottom: 2rem;
      display: flex;
      align-items: center;
      gap: 0.75rem;
      border: 2px solid #27ae6050;
    }

    .alert-success::before {
      content: '✓';
      font-weight: bold;
      font-size: 1.2rem;
    }

    /* Camera Test Result */
    #cameraResult {
      margin-top: 1rem;
      text-align: center;
    }

    #cameraResult img {
      max-width: 100%;
      border-radius: 8px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.1);
    }

    /* Responsive Adjustments */
    @media (max-width: 480px) {
      body {
        padding: 1rem;
      }
      
      .container {
        border-radius: 8px;
      }
      
      .sidebar, .form-section {
        padding: 2rem 1.5rem;
      }
    }

    /* Time Input Styling */
    .time-input {
      display: flex;
      align-items: center;
      gap: 0.5rem;
    }

    .time-input select {
      flex: 1;
    }

    .time-input span {
      font-size: 1.2rem;
      font-weight: 600;
      color: var(--text);
    }
    #toggleButton {
  background-color: #95a5a6;
  transition: background-color 0.3s ease;
}

#toggleButton.active {
  background-color: #2ecc71;
}

    /* Light Control Styling */
    .light-control {
      margin-bottom: 1rem;
    }
    
    .light-control h3 {
      margin-bottom: 1.5rem;
      font-size: 1.2rem;
      font-weight: 600;
      color: white;
    }
    
    .light-dropdown {
      margin: 1rem 0;
    }

    .light-dropdown select {
      background-color: rgba(255, 255, 255, 0.9);
      color: var(--primary);
      border: none;
      font-weight: 600;
    }

    /* Button Spacing */
    .button-group {
      display: grid;
      grid-template-columns: 1fr 1fr 1fr;
      gap: 1rem;
      margin-top: 1.5rem;
    }
    
    @media (max-width: 768px) {
      .button-group {
        grid-template-columns: 1fr;
      }
    }
  </style>
  <style>
    .loading {
      color: #666;
      padding: 1rem;
      text-align: center;
    }

    .error {
      color: #e74c3c;
      padding: 1rem;
      border: 1px solid #e74c3c;
      border-radius: 4px;
      margin: 1rem 0;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="sidebar">
      <div class="sidebar-top">
        <div class="branding">
          <h1>Kilocam</h1>
          <p>Ecological monitoring for everyone</p>
        </div>
        <div class="system-status">
          <h3>Current Status</h3>
          <p>Last Capture: %LAST_CAPTURE%</p>
        </div>
      </div>
    </div>
    
    <div class="form-section">
      <div class="form-header">
        <h2>Camera Configuration</h2>
        <p>Adjust settings for optimal wildlife monitoring</p>
      </div>
      
      <form method="POST" action="/update" class="form-grid" id="config-form">
        <div class="form-group">
          <label>Initial Start Time (24-hour format)</label>
          <div class="time-input">
            <select name="initial_time_hour" id="initial_time_hour" required>
              <!-- Hour options will be generated by JavaScript -->
            </select>
            <span>:</span>
            <select name="initial_time_minute" id="initial_time_minute" required>
              <!-- Minute options will be generated by JavaScript -->
            </select>
          </div>
        </div>
        
        <div class="form-group">
          <label>Capture Interval (seconds)</label>
          <input type="number" name="interval" value="%INTERVAL%" min="1" required>
        </div>
        
        <div class="form-group">
          <label>Photos per Capture</label>
          <input type="number" name="photos" value="%PHOTOS%" min="1" required>
        </div>
        
        <div class="form-group">
          <label>Sunrise Time (24-hour format)</label>
          <div class="time-input">
            <select name="sunrise_hour" id="sunrise_hour" required>
              <!-- Hour options will be generated by JavaScript -->
            </select>
            <span>:</span>
            <select name="sunrise_minute" id="sunrise_minute" required>
              <!-- Minute options will be generated by JavaScript -->
            </select>
          </div>
        </div>
        
        <div class="form-group">
          <label>Sunset Time (24-hour format)</label>
          <div class="time-input">
            <select name="sunset_hour" id="sunset_hour" required>
              <!-- Hour options will be generated by JavaScript -->
            </select>
            <span>:</span>
            <select name="sunset_minute" id="sunset_minute" required>
              <!-- Minute options will be generated by JavaScript -->
            </select>
          </div>
        </div>
        
        <div class="form-group">
          <label>Capture Mode</label>
          <select name="capture_mode" required>
            <option value="daylight" %DAYLIGHT_SELECTED%>Daylight Only</option>
            <option value="night" %NIGHT_SELECTED%>Night Only</option>
            <option value="both" %BOTH_SELECTED%>Both Day/Night</option>
          </select>
        </div>
        
        <!-- Hidden fields for client date and time -->
        <input type="hidden" name="client_date" id="client_date">
        <input type="hidden" name="client_time" id="client_time">
        
        <!-- Save button moved to button group -->
      </form>
      
      <!-- Button Group -->
      <div class="button-group">
  <button type="submit" form="config-form">Save Configuration</button>
  <button id="timeSyncButton">Time Sync</button>
  <button id="testCameraButton">Check Camera Function</button>
  <button id="toggleButton" class="%TOGGLE_STATE%">Toggle Feature</button>
</div>

<!-- Move the light control here - after the button group -->
<div class="light-control" style="margin-top: 2rem; background-color: #f8f9fa; padding: 1.5rem; border-radius: 8px; border: 1px solid #e0e0e0;">
  <h3 style="color: var(--primary); margin-bottom: 1rem; font-size: 1.2rem;">Light Intensity</h3>
  <div class="light-dropdown">
    <label for="light-select" style="display: block; margin-bottom: 0.5rem; font-weight: 600; color: var(--text);">Brightness Level</label>
    <select id="light-select" name="light_intensity" style="width: 100%; padding: 0.8rem; border: 2px solid #e0e0e0; border-radius: 6px; font-size: 1rem;">
      <option value="0">0% (Off)</option>
      <option value="25">25%</option>
      <option value="50" selected>50%</option>
      <option value="75">75%</option>
      <option value="100">100% (Maximum)</option>
    </select>
  </div>
</div>
      
      <div id="cameraResult"></div>
    </div>
  </div>
  <script>
    function generateTimeOptions() {
      // Generate hour options (00 to 23)
      const hours = Array.from({ length: 24 }, (_, i) => String(i).padStart(2, '0'));
      // Generate minute options (00, 05, 10, ..., 55)
      const minutes = Array.from({ length: 12 }, (_, i) => String(i * 5).padStart(2, '0'));

      const timeSelectors = document.querySelectorAll('select[name$="_hour"], select[name$="_minute"]');

      timeSelectors.forEach(selector => {
        if (selector.name.endsWith('_hour')) {
          hours.forEach(hour => {
            selector.options.add(new Option(hour, hour));
          });
        } else if (selector.name.endsWith('_minute')) {
          minutes.forEach(minute => {
            selector.options.add(new Option(minute, minute));
          });
        }
      });
    }

    // Set selected values for time dropdowns
    function setSelectedTime(id, value) {
      const select = document.getElementById(id);
      if (select) {
        select.value = value;
      }
    }

    // Function to update date and time fields from the client device
    function updateDateTime() {
      const now = new Date();
      const year = now.getFullYear();
      const month = String(now.getMonth() + 1).padStart(2, '0');
      const day = String(now.getDate()).padStart(2, '0');
      const hours = String(now.getHours()).padStart(2, '0');
      const minutes = String(now.getMinutes()).padStart(2, '0');
      const seconds = String(now.getSeconds()).padStart(2, '0');
      
      document.getElementById('client_date').value = `${year}-${month}-${day}`;
      document.getElementById('client_time').value = `${hours}:${minutes}:${seconds}`;
    }

    // Call the function to generate time options when the page loads
    window.onload = function() {
      generateTimeOptions();
      // Set selected values for time dropdowns
      setSelectedTime('initial_time_hour', '%INITIAL_TIME_HOUR%');
      setSelectedTime('initial_time_minute', '%INITIAL_TIME_MINUTE%');
      setSelectedTime('sunrise_hour', '%SUNRISE_HOUR%');
      setSelectedTime('sunrise_minute', '%SUNRISE_MINUTE%');
      setSelectedTime('sunset_hour', '%SUNSET_HOUR%');
      setSelectedTime('sunset_minute', '%SUNSET_MINUTE%');
      
      // Initialize client date and time
      updateDateTime();

      // Setup light intensity dropdown
      const lightSelect = document.getElementById('light-select');
      if (lightSelect) {
        // Set initial value if available
        if ('%LIGHT_INTENSITY%') {
          const intensity = parseInt('%LIGHT_INTENSITY%');
          // Find the closest value in our dropdown
          if (intensity <= 12) lightSelect.value = "0";
          else if (intensity <= 37) lightSelect.value = "25";
          else if (intensity <= 62) lightSelect.value = "50";
          else if (intensity <= 87) lightSelect.value = "75";
          else lightSelect.value = "100";
        }
        
        // Add event listener for changes
        lightSelect.addEventListener('change', function() {
          fetch('/set-light', {
            method: 'POST',
            headers: {
              'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: 'value=' + this.value
          })
          .catch(err => {
            console.log('Error sending light value');
          });
        });
      }
    };
    
    // Update date/time when form is submitted
    document.getElementById('config-form').addEventListener('submit', function() {
      updateDateTime();
    });
  </script>
  <script>
  document.getElementById('toggleButton').addEventListener('click', function() {
  const button = this;
  
  // Show visual feedback during request
  button.textContent = 'Updating...';
  button.disabled = true;
  
  fetch('/toggle', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    }
  })
  .then(response => response.json())
  .then(data => {
    // Update button state based on response
    if (data.state) {
      button.classList.add('active');
      button.textContent = 'LED On';
    } else {
      button.classList.remove('active');
      button.textContent = 'LED Off';
    }
  })
  .catch(error => {
    console.error('Error toggling state:', error);
    button.textContent = 'Error';
  })
  .finally(() => {
    button.disabled = false;
    
    // If we didn't set the text already, restore it
    if (button.textContent === 'Updating...') {
      button.textContent = 'Toggle Feature';
    }
  });
});

// Initialize toggle button text based on state
window.addEventListener('DOMContentLoaded', function() {
  const toggleButton = document.getElementById('toggleButton');
  if (toggleButton.classList.contains('active')) {
    toggleButton.textContent = 'LED On';
  } else {
    toggleButton.textContent = 'LED Off';
  }
});
    document.getElementById('testCameraButton').addEventListener('click', function() {
      const cameraResult = document.getElementById('cameraResult');
      cameraResult.innerHTML = '<div class="loading">Testing camera...</div>';
      
      const img = new Image();
      img.style.maxWidth = '100%';
      img.style.borderRadius = '8px';
      img.src = '/test?' + Date.now();
      
      img.onload = () => {
        cameraResult.innerHTML = '';
        cameraResult.appendChild(img);
      };
      
      img.onerror = () => {
        cameraResult.innerHTML = '<div class="error">Error: Failed to capture image</div>';
      };
      
      // Timeout handler
      setTimeout(() => {
        if (!img.complete) {
          cameraResult.innerHTML = '<div class="error">Error: Camera timeout</div>';
        }
      }, 5000);
    });

    // Time Sync Button Event Listener
    document.getElementById('timeSyncButton').addEventListener('click', function() {
      const cameraResult = document.getElementById('cameraResult');
      cameraResult.innerHTML = '<div class="loading">Synchronizing time...</div>';
      
      // Update date/time
      updateDateTime();
      
      // Send request to sync time
      fetch('/sync-time', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: 'client_date=' + document.getElementById('client_date').value + 
              '&client_time=' + document.getElementById('client_time').value
      })
      .then(response => {
        if (response.ok) {
          cameraResult.innerHTML = '<div class="alert-success">Time synchronized successfully</div>';
          return;
        }
        throw new Error('Network response was not ok');
      })
      .catch(error => {
        cameraResult.innerHTML = '<div class="error">Error: Failed to synchronize time</div>';
      });
    });
  </script>
</body>
</html>
)rawliteral";

  String html = htmlTemplate;
  // Replace placeholders for capture mode
  if (currentSettings.capture_mode == "daylight") {
    html.replace("%DAYLIGHT_SELECTED%", "selected");
    html.replace("%NIGHT_SELECTED%", "");
    html.replace("%BOTH_SELECTED%", "");
  } else if (currentSettings.capture_mode == "night") {
    html.replace("%DAYLIGHT_SELECTED%", "");
    html.replace("%NIGHT_SELECTED%", "selected");
    html.replace("%BOTH_SELECTED%", "");
  } else if (currentSettings.capture_mode == "both") {
    html.replace("%DAYLIGHT_SELECTED%", "");
    html.replace("%NIGHT_SELECTED%", "");
    html.replace("%BOTH_SELECTED%", "selected");
  }

  // Replace placeholders with actual values
  html.replace("%INITIAL_TIME_HOUR%", currentSettings.initial_time_hour.c_str());
  html.replace("%INITIAL_TIME_MINUTE%", currentSettings.initial_time_minute.c_str());
  html.replace("%INTERVAL%", String(currentSettings.interval));
  html.replace("%PHOTOS%", String(currentSettings.photos_per_capture));
  html.replace("%SUNRISE_HOUR%", currentSettings.sunrise_hour.c_str());
  html.replace("%SUNRISE_MINUTE%", currentSettings.sunrise_minute.c_str());
  html.replace("%SUNSET_HOUR%", currentSettings.sunset_hour.c_str());
  html.replace("%SUNSET_MINUTE%", currentSettings.sunset_minute.c_str());
  html.replace("%CAPTURE_MODE%", currentSettings.capture_mode.c_str());
  html.replace("%SD_STATUS%", SD.cardType() == CARD_NONE ? "Not Detected" : "Ready");
  html.replace("%LAST_CAPTURE%", currentSettings.client_date + " " + currentSettings.client_time);
  html.replace("%LIGHT_INTENSITY%", String(currentSettings.light_intensity));
  html.replace("%TOGGLE_STATE%", currentSettings.toggle_state ? "active" : "");

  // Check for success parameter in the URL
  if (request->hasParam("success")) {
    //html.replace("%SUCCESS_MESSAGE%", "<div class=\"alert-success\">Settings saved successfully!</div>");
  } else {
    html.replace("%SUCCESS_MESSAGE%", "");
  }

  response->print(html);
  request->send(response);
}

void initliazecamera() {
  // Ensure onboard LED is off
  digitalWrite(4, LOW);
  // Initialize camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 16000000;
  config.pixel_format = PIXFORMAT_JPEG;   //Can also be: YUV422,GRAYSCALE,RGB565,JPEG
  config.grab_mode = CAMERA_GRAB_LATEST;  // CRITICAL! If set to CAMERA_GRAB_WHEN_EMPTY you get old images

  // Define frame size, image quality, and number of pictures saved in the frame buffer.

  if (psramFound()) {
    config.frame_size = FRAMESIZE_QSXGA;  // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 4;              //10-63, lower number is higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    //Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Set camera settings (note that this can be tricky, best to leave as-is.
  // Use ESP32CAM examples > Camera > CameraWebServer to find the best settings for your situation
  sensor_t* s = esp_camera_sensor_get();

  s->set_brightness(s, 0);  // -2 to 2
  //s->set_contrast(s, 2);       // -2 to 2
  //s->set_saturation(s, 0);     // -2 to 2
  //s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
  s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
  s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
  s->set_wb_mode(s, 1);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable auto exposure!
  //s->set_aec2(s, 0);           // 0 = disable , 1 = enable
  //s->set_ae_level(s, 0);       // -2 to 2
  //s->set_aec_value(s, 300);    // 0 to 1200
  //s->set_gain_ctrl(s, 0);      // 0 = disable , 1 = enable
  //s->set_agc_gain(s, 0);       // 0 to 30
  //s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
  s->set_bpc(s, 1);  // 0 = disable , 1 = enable
  s->set_wpc(s, 1);  // 0 = disable , 1 = enable
  //s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
  s->set_lenc(s, 1);  // 0 = disable , 1 = enable
  // s->set_hmirror(s, 1);        // 0 = disable , 1 = enable
  //s->set_vflip(s, 0);          // 0 = disable , 1 = enable
  //s->set_dcw(s, 1);            // 0 = disable , 1 = enable
  //s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
}

void setup() {

  Serial.begin(57600);

  rtc_gpio_hold_en(GPIO_NUM_4);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  rtc_gpio_hold_dis(GPIO_NUM_4);


  pinMode(33, OUTPUT);        // GPIO for on-board LED flash
  pinMode(flashled, OUTPUT);  //GPIO for LED flash

  esp_task_wdt_init(10, false);

  // Initialize camera before WiFi
  initliazecamera();
  delay(1000);
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed");
    return;
  }

  File configFile = SPIFFS.open(SETTINGS_FILE, "r");
  if (configFile) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    if (error) {
      Serial.print("Failed to parse settings: ");
      Serial.println(error.c_str());
    } else {
      currentSettings.initial_time_hour = doc["initial_time_hour"].as<String>();
      currentSettings.initial_time_minute = doc["initial_time_minute"].as<String>();
      currentSettings.interval = doc["interval"].as<int>();
      currentSettings.photos_per_capture = doc["photos_per_capture"].as<int>();
      currentSettings.sunrise_hour = doc["sunrise_hour"].as<String>();
      currentSettings.sunrise_minute = doc["sunrise_minute"].as<String>();
      currentSettings.sunset_hour = doc["sunset_hour"].as<String>();
      currentSettings.sunset_minute = doc["sunset_minute"].as<String>();
      currentSettings.capture_mode = doc["capture_mode"].as<String>();
      currentSettings.client_date = doc["client_date"].as<String>();
      currentSettings.client_time = doc["client_time"].as<String>();

      // Add this line to load light_intensity:
      if (doc.containsKey("light_intensity")) {
        currentSettings.light_intensity = doc["light_intensity"].as<int>();
      }
      if (doc.containsKey("toggle_state")) {
        currentSettings.toggle_state = doc["toggle_state"].as<bool>();
      }
    }
    configFile.close();
  } else {
    Serial.println("Settings file not found. Using defaults.");
  }

  // Initial SD card check with proper initialization
  sdCardPresent = isSDCardPresent();

  // Set initial mode based on SD presence
  if (sdCardPresent) {
    Serial.println("Boot: SD Card detected, starting in Operational");
    currentMode = OPERATIONAL_MODE;
    setupOperationalMode();
  } else {
    Serial.println("Boot: No SD Card, starting in Programming");
    currentMode = PROGRAMMING_MODE;
    setupProgrammingMode();
  }
}

// Add this function to directly use the parsepicsdata logic with a timeout
bool getMetadataExisting() {
  // Clear the global variables first
  memset(TIMESTAMP, 0, sizeof(TIMESTAMP));
  LIGHT = 0;
  TEMP = 0.0;

  // Local buffer for this function
  char localPayload[PAYLOAD_SIZE] = { 0 };
  byte index = 0;
  unsigned long startTime = millis();
  const unsigned long TIMEOUT = 3000;  // 3 second timeout

  // Wait for complete metadata with timeout
  while (millis() - startTime < TIMEOUT) {
    while (Serial.available() > 0) {
      char c = Serial.read();

      if (c == '\n') {
        // Trim trailing \r if present
        if (index > 0 && localPayload[index - 1] == '\r') {
          index--;
        }

        localPayload[index] = '\0';  // Null-terminate the string
        //Serial.print("Received: ");
        //Serial.println(localPayload);

        // Only process if it starts with H_
        if (strncmp(localPayload, "H_", 2) == 0) {
          char* token = strtok(localPayload, "_");
          token = strtok(NULL, "_");  // Get timestamp

          if (token && strlen(token) == 14) {
            // Copy timestamp to global variable
            strncpy(TIMESTAMP, token, sizeof(TIMESTAMP) - 1);
            TIMESTAMP[sizeof(TIMESTAMP) - 1] = '\0';

            // Get light value
            token = strtok(NULL, "_");
            if (token) LIGHT = atoi(token);

            // Get temperature
            token = strtok(NULL, "_");
            if (token) TEMP = atof(token);

            //Serial.printf("Parsed: TS=%s, L=%d, T=%.1f\n",
            //              TIMESTAMP, LIGHT, TEMP);
            return true;
          }
        }

        // If we got here but couldn't parse, reset and try again if time permits
        index = 0;
        memset(localPayload, 0, PAYLOAD_SIZE);
        break;
      } else if (index < PAYLOAD_SIZE - 1) {
        localPayload[index++] = c;
      }
    }

    // Small delay to prevent CPU hogging
    delay(1);
  }

  Serial.println("Metadata timeout");
  return false;
}

// Modified handlecaptureImages() that uses the existing parsing approach
void handlecaptureImages() {
  Serial.println("P");  // Signal ready

  NumPics = currentSettings.photos_per_capture;
  if (NumPics <= 0) {
    NumPics = 1;
    Serial.println("Using default photo count: 1");
  }

  // Initialize SD card in 1-bit SPI mode
  if (!SD.begin(SD_CS)) {
    Serial.println("SD FAILED");
    captureTriggered = false;
    return;
  }

  // Capture photos
  for (int I = 0; I < NumPics; I++) {
    // Clear these globals at the start of each photo
    memset(TIMESTAMP, 0, sizeof(TIMESTAMP));
    LIGHT = 0;
    TEMP = 0.0;

    delay(250); 

    Serial.println("L");  // Request LED on
    //Serial.println("Sent L");

    //delay(1000); 

    // Get metadata using existing parsing logic
    if (!getMetadataExisting()) {
      Serial.println("Failed to get metadata, using placeholder");
      strcpy(TIMESTAMP, "NOTS");
    }

    // Capture and save photo
    camera_fb_t* fb = NULL;

    delay(1000); // Normalize exposure
    
    fb = esp_camera_fb_get();

    if (fb) {
      char path[64];
      int tempInt = (int)TEMP;

      // Log what we're using
      //Serial.printf("Saving with TS='%s', L=%d, T=%d\n",
      //              TIMESTAMP, LIGHT, tempInt);

      // Create filename
      snprintf(path, sizeof(path), "/IMG_%s_L%d_T%d_%d.jpg",
               TIMESTAMP, LIGHT, tempInt, I);

      File file = SD.open(path, FILE_WRITE);
      if (file) {
        file.write(fb->buf, fb->len);
        file.close();
        Serial.println("D");  // Signal photo done
        //Serial.printf("Saved: %s\n", path);
      } else {
        Serial.println("Failed to open file for writing");
        Serial.println("D");
      }
      esp_camera_fb_return(fb);

    } else {
      Serial.println("Camera capture failed");
      Serial.println("D");
    }

    //Serial.println("D");  // Signal photo done
    //Serial.println("Sent D");
  }
  
  Serial.println("Q");  // Signal shutdown
  //Serial.println("Sent Q");
  captureTriggered = false;  // Reset capture flag
}

void loop() {
  checkMode();
  blinkLED();

  // Dispatch to the current mode's handler
  if (currentMode == OPERATIONAL_MODE) {
    handleOperationalMode();  // Non-blocking
  } else {
    handleProgrammingMode();  // Non-blocking
  }
}

void handleOperationalMode() {
  if (millis() - lastKiloCamCheck >= KILOCAM_CHECK_INTERVAL) {
    Serial.println(MODE_OPERATIONAL);
    lastKiloCamCheck = millis();
  }

  // Set LED pattern
  ledPattern = OPERATIONAL_PATTERN;

  // Process incoming data
  parsepicsdata();

  // Handle capture if triggered
  if (captureTriggered) {
    digitalWrite(flashled, HIGH);  // Visual indicator that capture is starting
    delay(100);
    digitalWrite(flashled, LOW);

    handlecaptureImages();

    // Ensure flag is reset in case of early return
    captureTriggered = false;
  }
}

void handleProgrammingMode() {
  if (millis() - lastKiloCamCheck >= KILOCAM_CHECK_INTERVAL) {
    Serial.println(MODE_PROGRAMMING);
    lastKiloCamCheck = millis();
  }
  parsepicsdata();
  ledPattern = PROGRAMMING_PATTERN;
}

void parsepicsdata() {
  static byte index = 0;
  static unsigned long lastReceive = 0;
  const unsigned long timeout = 100;

  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n') {
      // Trim trailing \r if present
      if (index > 0 && PAYLOAD[index - 1] == '\r') {
        index--;
      }
      PAYLOAD[index] = '\0';  // Null-terminate the string

      // Keep only the command processing parts:
      if (strcmp(PAYLOAD, "CAPTURE") == 0) {
        captureTriggered = true;
        Serial.println("Capture command received!");
      } else if (strcmp(PAYLOAD, "DSENT") == 0) {
        sendSettingsFromSPIFFS();
        Serial.println("Sending Data");
      }

      index = 0;
      memset(PAYLOAD, 0, PAYLOAD_SIZE);
      lastReceive = millis();
    } else if (index < PAYLOAD_SIZE - 1) {
      PAYLOAD[index++] = c;
      lastReceive = millis();
    }
  }

  // Handle timeout
  if ((millis() - lastReceive) > timeout && index > 0) {
    index = 0;
    memset(PAYLOAD, 0, PAYLOAD_SIZE);
  }
}

void sendSettingsFromSPIFFS() {
  // Load settings from SPIFFS
  File configFile = SPIFFS.open(SETTINGS_FILE, "r");
  if (!configFile) {
    Serial.println("Failed to open settings file");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.print("Settings parse error: ");
    Serial.println(error.c_str());
    configFile.close();
    return;
  }

  // Populate settings structure
  currentSettings.initial_time_hour = doc["initial_time_hour"].as<String>();
  currentSettings.initial_time_minute = doc["initial_time_minute"].as<String>();
  currentSettings.interval = doc["interval"].as<int>();
  currentSettings.photos_per_capture = doc["photos_per_capture"].as<int>();
  currentSettings.sunrise_hour = doc["sunrise_hour"].as<String>();
  currentSettings.sunrise_minute = doc["sunrise_minute"].as<String>();
  currentSettings.sunset_hour = doc["sunset_hour"].as<String>();
  currentSettings.sunset_minute = doc["sunset_minute"].as<String>();
  currentSettings.capture_mode = doc["capture_mode"].as<String>();
  //currentSettings.client_date = doc["client_date"].as<String>();
  //currentSettings.client_time = doc["client_time"].as<String>();

  if (doc.containsKey("light_intensity")) {
    currentSettings.light_intensity = doc["light_intensity"].as<int>();
  }


  configFile.close();

  // Generate and send packet
  String packet = createPacket(currentSettings);
  Serial.println(packet);
  Serial.flush();
  //Serial.println("Settings sent from SPIFFS");

  delay(100);
  String lightCommand = "L:" + String(currentSettings.light_intensity);
  Serial.println(lightCommand);
  Serial.flush();
}