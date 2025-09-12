#include <Arduino.h>
#include <CRC32.h>
#include <EEPROM.h>
#include <DS3232RTC.h>
#include <LowPower.h>
#include <Wire.h>
#include <SPI.h>
#include <Servo.h>

// flags to trigger each mode
bool Operationstatus = false;
bool Programmingstatus = false;
bool firstAlarmProcessed = false;  // Global flag to track if the first alarm has been processed
bool isdatasent = false;
bool istimeset = false;

String cmdBuffer = "";
unsigned long lastCmdTime = 0;
const unsigned long CMD_TIMEOUT = 100;  // 100ms timeout

// Add this at top with other global variables
unsigned long LAST_SERIAL_FLUSH_TIME = 0;
const unsigned long SERIAL_FLUSH_INTERVAL = 50;  // ms


// Global state variables for serial communication
static String serialBuffer;
static bool isReceivingPacket = false;
static unsigned long packetTimeout = 0;

// EEPROM Configuration
#define EEPROM_START 0
#define SETTINGS_SIZE 128      // Adjust based on actual struct size
#define SERIAL_FLUSH_DELAY 10  // ms to wait after transmission

unsigned long lastsentData = 0;

// Define constants
#define RTC_ALARM_PIN 2  //pin for interrupt
#define LIGHTSENSORPIN A6
#define TEMPSENSORPIN A7
#define SAMPLE_DELAY 50  // Reduced delay for faster sampling

// Object instantiations
time_t t, alarmTime, alarmTimestamp;
tmElements_t tm;
DS3232RTC RTC;

// Object for LED control on pin D6 breakout
byte servoPin = 6;  //Is usually 6, changed to 3 for the test of alternate board
Servo servo;

unsigned long lastKiloCamCheck = 0;


// Global variable declarations
volatile bool alarmIsrWasCalled = true;  // DS3231 RTC alarm interrupt service routine (ISR) flag. Set to true to allow the first iteration of the loop to take place and sleep the system.
unsigned long alarmInt;

// Settings structure with fixed-size buffers
struct Settings {
  char initial_time_hour[3] = "00";
  char initial_time_minute[3] = "00";
  int interval = 30;
  int photos_per_capture = 5;
  char sunrise_hour[3] = "06";
  char sunrise_minute[3] = "00";
  char sunset_hour[3] = "18";
  char sunset_minute[3] = "00";
  char capture_mode[12] = "daylight";
  char client_date[11] = "2024-01-01";
  char client_time[9] = "00:00";
  uint32_t checksum = 0;
};
Settings currentSettings;

int NumPics = 5;
int lightLevel = 0;  // Default value
int servoValue = 0;

void SendPhotoData() {
  // Light measurement with validation
  int lightSum = 0;
  for (int i = 0; i < 3; i++) {
    int raw = analogRead(LIGHTSENSORPIN);
    if (raw < 0 || raw > 1023) return;
    lightSum += raw;
    delay(SAMPLE_DELAY);
  }
  const int LIGHT = lightSum / 3;

  // Temperature measurement with validation
  float tempSum = 0;
  for (int i = 0; i < 3; i++) {
    int raw = analogRead(TEMPSENSORPIN);
    if (raw < 0 || raw > 1023) return;
    float voltage = raw * (3.3f / 1023.0f);
    tempSum += (voltage - 0.5f) / 0.01f;
    delay(SAMPLE_DELAY);
  }
  const float TEMP = tempSum / 3.0f;

  // Manual temperature formatting (same as sample data)
  char tempStr[6];  // XX.X format + null
  int tempInt = static_cast<int>(TEMP);
  int tempDecimal = static_cast<int>((TEMP - tempInt) * 10);
  snprintf(tempStr, sizeof(tempStr), "%d.%d", tempInt, abs(tempDecimal));

  // Get validated timestamp
  const time_t t = RTC.get();
  if (year(t) < 2024) return;

  char timestamp[15];
  snprintf(timestamp, sizeof(timestamp),
           "%04d%02d%02d%02d%02d%02d",
           year(t), month(t), day(t),
           hour(t), minute(t), second(t));

  // Create packed message using string temperature
  char dataString[50];
  const int written = snprintf(dataString, sizeof(dataString),
                               "H_%s_%d_%s", timestamp, LIGHT, tempStr);

  // Validate and send
  if (written > 0 && written < sizeof(dataString)) {
    Serial.write(dataString, written);
    Serial.write('\n');
    Serial.flush();
    delay(SERIAL_FLUSH_DELAY);
  }
  //Serial.println("Data Sent: " + String(dataString));
}



// Add these functions in your code
bool isDaylight() {
  // Get current time
  const time_t now = RTC.get();
  int currentHour = hour(now);
  int currentMinute = minute(now);
  int currentMinutes = (currentHour * 60) + currentMinute;

  // Convert sunrise time to minutes
  int sunriseH = atoi(currentSettings.sunrise_hour);
  int sunriseM = atoi(currentSettings.sunrise_minute);
  int sunriseMinutes = (sunriseH * 60) + sunriseM;

  // Convert sunset time to minutes
  int sunsetH = atoi(currentSettings.sunset_hour);
  int sunsetM = atoi(currentSettings.sunset_minute);
  int sunsetMinutes = (sunsetH * 60) + sunsetM;

  // Check if current time is within daylight window
  return (currentMinutes >= sunriseMinutes && currentMinutes < sunsetMinutes);
}
void RunCamera() {
  // Flash LED to indicate startup
  digitalWrite(13, HIGH);
  delay(250);
  digitalWrite(13, LOW);

  // Attach servo/LED control
  servo.attach(servoPin);

  // Initialize communication with ESP32-CAM

  digitalWrite(5, HIGH);  // Power ESP32-CAM
  Serial.println("waiting for ESP 32 cam to power up");
  Serial.flush();  // Clear old data
  delay(3000);

  // Wait for "M_O" packet before continuing
  bool receivedMO = false;
  while (!receivedMO) {
    handleSerialCommands();  // Call your existing serial command handler
    if (Operationstatus) {   // This checks if "M_O" was received
      receivedMO = true;
      Serial.println("M_O received, proceeding with capture.");
    }
  }

  Serial.println("CAPTURE");
  Serial.println("Command Sent capture");

  // Wait for 'P' from ESP32-CAM (blocking wait)
  while (Serial.read() != 'P') {};
  Serial.println("P Received");
  Serial.flush();  // Clear old data

  NumPics = currentSettings.photos_per_capture;  // Use the value from EEPROM
  if (NumPics <= 0) {
    NumPics = 1;  // Default value if invalid (e.g., zero or negative)
    Serial.println("Invalid number of photos, using default: 1");
  }

  for (int I = 0; I < NumPics; I++) {
    // Wait for "L" from ESP32-CAM
    while (Serial.read() != 'L') {}  // Block until 'L' received
    //Serial.println("L Received");

    // Send metadata (e.g., "H_TIMESTAMP_LIGHT_TEMP\n")
    SendPhotoData();

    // Wait for "D" from ESP32-CAM
    while (Serial.read() != 'D') {}  // Block until 'D' received
    Serial.println("D Received");
  }

  // --------------------------------------
  // Step 4: Wait for "Q" to finalize
  // --------------------------------------
  while (Serial.read() != 'Q') {}  // Block until 'Q' received
  Serial.println("Q Received");

  // --------------------------------------
  // Shutdown ESP32-CAM
  // --------------------------------------
  // Safety to confirm the LED is powered off and servopin pulled low before sleep
  servo.writeMicroseconds(1100);  // Turn off the LED
  delay(250);
  servo.detach();
  digitalWrite(servoPin, LOW);  // Pull the pin low. Else the LED turns on during sleep.

  digitalWrite(5, LOW);  // Pull the pin 5 low to power off ESP32-CAM

  digitalWrite(13, HIGH);  // Flash the blue LED to show the cycle has completed.
  delay(250);
  digitalWrite(13, LOW);
  delay(250);
  digitalWrite(13, HIGH);  // Flash the blue LED to show the cycle has completed.
  delay(250);
  digitalWrite(13, LOW);
  Serial.println(F("ESP32-CAM Shutdown."));
}

void RunCamera_LED() {
  // Set up a 30-second window for data capture to occur in.
  unsigned long currentMillis = millis();  // Get the time in case ESP32 doesn't send a signal
  unsigned long shutdownMillis = currentMillis + 30000;

  digitalWrite(13, HIGH);  // Indicate startup
  delay(250);
  digitalWrite(13, LOW);

  servo.attach(servoPin);
  digitalWrite(5, HIGH);  // Power ESP32-CAM
  Serial.println("waiting for ESP 32 cam to power up");

  Serial.flush();  // Clear old data

  // Wait for "M_O" packet before continuing
  bool receivedMO = false;
  while (!receivedMO) {
    handleSerialCommands();  // Call your existing serial command handler
    if (Operationstatus) {   // This checks if "M_O" was received
      receivedMO = true;
      //Serial.println("M_O received, proceeding with capture.");
    }
  }

  delay(3000);  // Adjust this value as necessary to ensure stability

  Serial.println("CAPTURE");
  //Serial.println("Command Sent capture");

  // Wait for 'P' from ESP32-CAM (blocking wait)
  while (Serial.read() != 'P') {};
  Serial.println("P Received");
  Serial.flush();  // Clear old data

  NumPics = currentSettings.photos_per_capture;  // Use the value from EEPROM
  if (NumPics <= 0) {
    NumPics = 1;  // Default value if invalid (e.g., zero or negative)
    Serial.println("Invalid number of photos, using default: 1");
  }

  digitalWrite(13, HIGH);  // Flash the LED to show data sent
  delay(50);
  digitalWrite(13, LOW);

  // Photo capture loop
  for (int I = 0; I < NumPics; I++) {
    // Wait for 'L' from ESP32-CAM (blocking wait)
    while (Serial.read() != 'L') {};
    Serial.println("Receive L");

    // Turn on the Lumen LED for the photo
    servo.writeMicroseconds(servoValue);  // Max brightness is 1900, off is 1100. Adjust as needed.

    delay(250);

    // Send metadata (e.g., "H_TIMESTAMP_LIGHT_TEMP\n")
    SendPhotoData();
    // Wait for 'D' from ESP32-CAM (blocking wait)
    while (Serial.read() != 'D') {};
    Serial.println("D Received");

    // Turn off the Lumen LED after the photo
    servo.writeMicroseconds(1100);  // Turn off the LED

    Serial.flush();
  }

  // Wait for 'Q' to confirm shutdown (blocking wait)
  while (Serial.read() != 'Q') {};
  Serial.println("Q Received");
  Serial.flush();  // Clear old data
  delay(500);

  // Safety to confirm the LED is powered off and servopin pulled low before sleep
  servo.writeMicroseconds(1100);  // Turn off the LED
  delay(500);
  servo.detach();
  digitalWrite(servoPin, LOW);  // Pull the pin low. Else the LED turns on during sleep.

  digitalWrite(5, LOW);  // Pull the pin 5 low to power off ESP32-CAM

  digitalWrite(13, HIGH);  // Flash the blue LED to show the cycle has completed.
  delay(250);
  digitalWrite(13, LOW);
  delay(250);
  digitalWrite(13, HIGH);  // Flash the blue LED to show the cycle has completed.
  delay(250);
  digitalWrite(13, LOW);
  Serial.println(F("ESP32-CAM Shutdown."));
}




// Real-time clock alarm interrupt service routine (ISR)
void alarmIsr() {
  alarmIsrWasCalled = true;
}

// Enable sleep and await the RTC alarm interrupt
void goToSleep() {
  Serial.println(F("Going to sleep..."));
  Serial.flush();

  // Ensure interrupt is properly set up before sleeping
  RTC.alarmInterrupt(DS3232RTC::ALARM_1, true);
  attachInterrupt(digitalPinToInterrupt(RTC_ALARM_PIN), alarmIsr, LOW);

  // Enter sleep and await an external interrupt
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

  // When we wake up
  detachInterrupt(digitalPinToInterrupt(RTC_ALARM_PIN));
}

void handleOperationalMode() {

  if (!isdatasent) {
  // Simple handshake
  Serial.println("DSENT");

  // Listen for settings packet
  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {  // 5-second timeout
    if (Serial.available()) {
      char c = Serial.read();

      // Use same buffer as programming mode
      if (c == '<') {
        serialBuffer = "<";
        isReceivingPacket = true;
      } else if (isReceivingPacket) {
        serialBuffer += c;

        if (c == '>') {
          processHybridPacket(serialBuffer);
          //isdatasent = true;
          break;
        }
      }
    }
  }

  // Now look for light value command after settings packet
  startTime = millis();
  while (millis() - startTime < 3000) {  // 3-second timeout for light value
    if (Serial.available()) {
      char c = Serial.read();
      
      // Add to command buffer
      cmdBuffer += c;
      
      // Check for complete line
      if (c == '\n') {
        // Process light command
        if (cmdBuffer.startsWith("L:")) {
          String valueStr = cmdBuffer.substring(2);
          valueStr.trim();
          
          if (valueStr.length() > 0) {
            int value = valueStr.toInt();
            value = constrain(value, 0, 100);
            lightLevel = value;
            servoValue = 1100 + (lightLevel * 8);  
            
            // Acknowledge receipt
            Serial.print("L_OK:");
            Serial.println(servoValue);
            Serial.flush();
          }
        }
        
        // Clear buffer
        cmdBuffer = "";
      }
      
      // Buffer overflow protection
      if (cmdBuffer.length() > 20) {
        cmdBuffer = "";
      }
    }
  }
  
  isdatasent = true;
}

  if (alarmIsrWasCalled) {
    Serial.println(F("Alarm ISR set to True! Waking up."));
    t = RTC.get();                   // Read the current date and time from RTC
    time_t currentTime = RTC.get();  // Read the current date and time from RTC

    // Log current time
    Serial.println(F("Current time is: "));
    Serial.print(year(t));
    Serial.print("/");
    Serial.print(month(t));
    Serial.print("/");
    Serial.print(day(t));
    Serial.print("  ");
    Serial.print(hour(t));
    Serial.print(":");
    Serial.print(minute(t));
    Serial.print(":");
    Serial.print(second(t));
    Serial.println("  ");

    // Check if this is the first alarm
    if (!firstAlarmProcessed) {

      // Convert current time to total seconds since midnight
      uint32_t currentSeconds = ((uint32_t)hour(currentTime) * 3600) + ((uint32_t)minute(currentTime) * 60 + (uint32_t)second(currentTime));
      //Serial.println("Current Sec :");
      //Serial.print(currentSeconds);

      int initialHour = atoi(currentSettings.initial_time_hour);
      int initialMinute = atoi(currentSettings.initial_time_minute);

      //Serial.println(initialHour);
      //Serial.println(initialMinute);
      uint32_t initialAlarmSeconds = ((uint32_t)initialHour * 3600) + ((uint32_t)initialMinute * 60);

      //Serial.print("initialAlarmSeconds :");
      //Serial.println(initialAlarmSeconds);

      // Check if the current time is before the initial alarm time
      if (currentSeconds < initialAlarmSeconds) {
        Serial.println(F("Initial alarm time not reached. Going back to sleep."));
        digitalWrite(5, LOW);
        delay(100);
        alarmIsrWasCalled = false;  // Reset the RTC ISR flag
        RTC.setAlarm(DS3232RTC::ALM1_MATCH_HOURS, 0, initialMinute, initialHour, 0);
        RTC.alarm(DS3232RTC::ALARM_1);                 // Clear the alarm flag
        RTC.alarmInterrupt(DS3232RTC::ALARM_1, true);  // Enable interrupt
        goToSleep();                                   // Go back to sleep
        return;                                        // Exit the function
      }

      // Mark the first alarm as processed
      firstAlarmProcessed = true;
      Serial.println(F("First alarm processed."));
    }

    // Clear the alarm flag and continue with processing
    RTC.alarm(DS3232RTC::ALARM_1);  // Clear the alarm flag

    // Check if current time is within daylight window
    bool isDaytime = isDaylight();

    // Process based on time of day and mode settings
    if (strcmp(currentSettings.capture_mode, "daylight") == 0) {
      // Only run in daylight mode
      if (isDaytime) {
        Serial.println(F("Daylight mode - capturing photo"));
        alarmInt = currentSettings.interval;
        alarmTime = RTC.get() + alarmInt;  // Calculate the next alarm
        RunCamera();
      } else {
        Serial.println(F("Daylight mode - skipping (nighttime)"));
        alarmInt = currentSettings.interval;
        alarmTime = RTC.get() + alarmInt;  // Still set next alarm
      }
    } else if (strcmp(currentSettings.capture_mode, "night") == 0) {
      // Only run in night mode
      if (!isDaytime) {
        Serial.println(F("Night mode - capturing photo with LED"));
        alarmInt = currentSettings.interval;
        alarmTime = RTC.get() + alarmInt;  // Calculate the next alarm
        RunCamera_LED();
      } else {
        Serial.println(F("Night mode - skipping (daytime)"));
        alarmInt = currentSettings.interval;
        alarmTime = RTC.get() + alarmInt;  // Still set next alarm
      }
    } else {
      // Both modes or default (always run)
      if (isDaytime) {
        Serial.println(F("Both mode - daytime capture"));
        alarmInt = currentSettings.interval;
        alarmTime = RTC.get() + alarmInt;  // Calculate the next alarm
        RunCamera();
      } else {
        Serial.println(F("Both mode - nighttime capture with LED"));
        alarmInt = currentSettings.interval;
        alarmTime = RTC.get() + alarmInt;
        RunCamera_LED();
      }
    }

    // Set the alarm
    Serial.println(F("Setting a new alarm."));
    RTC.setAlarm(DS3232RTC::ALM1_MATCH_DATE, second(alarmTime), minute(alarmTime), hour(alarmTime), day(alarmTime));

    // Log the next alarm time
    Serial.println(F("Next alarm is at: "));
    Serial.print(year(alarmTime));
    Serial.print("/");
    Serial.print(month(alarmTime));
    Serial.print("/");
    Serial.print(day(alarmTime));
    Serial.print("  ");
    Serial.print(hour(alarmTime));
    Serial.print(":");
    Serial.print(minute(alarmTime));
    Serial.print(":");
    Serial.print(second(alarmTime));
    Serial.println("  ");

    // Check if the alarm was set in the past
    if (RTC.get() >= alarmTime) {
      Serial.println(F("The new alarm has already passed! Setting the next one."));
      // Add a small buffer to ensure we're in the future
      alarmTime = RTC.get() + alarmInt + 10;  // Add 10 second buffer

      // Set the corrected alarm
      RTC.setAlarm(DS3232RTC::ALM1_MATCH_DATE, second(alarmTime), minute(alarmTime), hour(alarmTime), day(alarmTime));
      RTC.alarm(DS3232RTC::ALARM_1);  // Ensure the alarm flag is cleared

      // Log the corrected alarm time
      Serial.println(F("Next alarm is at: "));
      Serial.print(year(alarmTime));
      Serial.print("/");
      Serial.print(month(alarmTime));
      Serial.print("/");
      Serial.print(day(alarmTime));
      Serial.print("  ");
      Serial.print(hour(alarmTime));
      Serial.print(":");
      Serial.print(minute(alarmTime));
      Serial.print(":");
      Serial.print(second(alarmTime));
      Serial.println("  ");
    }

    // Make sure alarm interrupt is enabled
    RTC.alarmInterrupt(DS3232RTC::ALARM_1, true);

    // Reset the flag and go to sleep
    alarmIsrWasCalled = false;  // Reset the RTC ISR flag
    Serial.println(F("Alarm ISR set to False"));
    Serial.println(F("ESP32-CAM Shutdown. Pulling D5 LOW."));
    digitalWrite(5, LOW);  // Ensure ESP32-CAM is powered off
    goToSleep();           // Sleep
  }
}

void handleProgrammingMode() {

  // Flash two short times to show KiloCam is powered on
  digitalWrite(13, LOW);

}
void handleSerialCommands() {
  static String lineBuffer = "";
  static bool isProcessingCommand = false;
  static unsigned long commandStartTime = 0;
  
  // Process any available data in the serial buffer
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    // Special case: If we receive '<', start packet mode immediately
    if (c == '<' && !isReceivingPacket) {
      // Cancel any in-progress command
      isProcessingCommand = false;
      lineBuffer = "";
      
      // Start packet reception
      isReceivingPacket = true;
      serialBuffer = "<";
      packetTimeout = millis();
      
      // Debug output
      Serial.println("Starting packet reception");
      continue;
    }
    
    // If we're receiving a packet, add to packet buffer
    if (isReceivingPacket) {
      serialBuffer += c;
      
      // Check for packet completion
      if (c == '>') {
        Serial.println("Complete packet received");
        
        // Process the packet immediately
        processHybridPacket(serialBuffer);
        
        // Reset packet state
        isReceivingPacket = false;
        serialBuffer = "";
        
        // Also reset any command state to avoid confusion
        isProcessingCommand = false;
        lineBuffer = "";
        
        // Clear any leftover bytes
        while (Serial.available()) Serial.read();
        continue;
      }
      
      // Continue collecting packet data
      continue;
    }
    
    // Normal command processing (not in packet mode)
    
    // Add character to line buffer
    lineBuffer += c;
    
    // Handle newline - process complete command
    if (c == '\n') {
      // Trim the string
      lineBuffer.trim();
      
      // Process different command types
      if (lineBuffer == "TIME") {
        Serial.println("Time sync requested");
        istimeset = true;
        
        // Reset command state
        isProcessingCommand = false;
        lineBuffer = "";
      }
      else if (lineBuffer.startsWith("L:")) {
        String valueStr = lineBuffer.substring(2);
        valueStr.trim();
        
        if (valueStr.length() > 0) {
          int value = valueStr.toInt();
          value = constrain(value, 0, 100);
          lightLevel = value;
          
          // Immediately acknowledge with a specific format
          Serial.print("L_OK:");
          Serial.println(lightLevel);
          Serial.flush();
          
          // Very important: Reset the time sync flag to prevent conflicts
          istimeset = false;
        }
        
        // Reset command state
        isProcessingCommand = false;
        lineBuffer = "";
      }
      else if (lineBuffer.endsWith("@M_O@")) {
        Serial.println("Switching to operational mode");
        Programmingstatus = false;
        Operationstatus = true;
        
        // Reset command state
        isProcessingCommand = false;
        lineBuffer = "";
      }
      else if (lineBuffer.endsWith("@M_P@")) {
        Serial.println("Switching to programming mode");
        Programmingstatus = true;
        Operationstatus = false;
        
        // Reset command state
        isProcessingCommand = false;
        lineBuffer = "";
      }
      else {
        // Unknown command, reset
        isProcessingCommand = false;
        lineBuffer = "";
      }
    }
    
    // Command buffer overflow protection
    if (lineBuffer.length() > 20) {
      lineBuffer = "";
    }
  }
  
  // Check for packet timeout
  if (isReceivingPacket && (millis() - packetTimeout > 5000)) {
    Serial.println("Packet reception timeout");
    isReceivingPacket = false;
    serialBuffer = "";
  }
}

// Modified processing of hybrid packets to properly handle timing
void processHybridPacket(const String& packet) {
  // Extract checksum and payload
  size_t csStart = packet.indexOf("CS=") + 3;
  size_t csEnd = packet.indexOf(';', csStart);
  
  if (csStart < 3 || csEnd == -1) {
    Serial.println("Invalid packet format");
    return;
  }
  
  uint32_t receivedChecksum = strtoul(packet.substring(csStart, csEnd).c_str(), NULL, 10);
  String payload = packet.substring(csEnd + 1, packet.length() - 1);

  // Temporary settings storage
  Settings newSettings;
  newSettings.checksum = receivedChecksum;

  // Parse settings from payload
  size_t start = 0;
  while (start < payload.length()) {
    size_t end = payload.indexOf(';', start);
    if (end == -1) end = payload.length();
    String pair = payload.substring(start, end);
    size_t eq = pair.indexOf('=');
    if (eq == -1) {
      start = end + 1;
      continue;
    }

    String key = pair.substring(0, eq);
    String value = pair.substring(eq + 1);

    // Store in fixed-size buffers
    if (key == "ITH") value.toCharArray(newSettings.initial_time_hour, 3);
    else if (key == "ITM") value.toCharArray(newSettings.initial_time_minute, 3);
    else if (key == "INT") newSettings.interval = value.toInt();
    else if (key == "PPC") newSettings.photos_per_capture = value.toInt();
    else if (key == "SRH") value.toCharArray(newSettings.sunrise_hour, 3);
    else if (key == "SRM") value.toCharArray(newSettings.sunrise_minute, 3);
    else if (key == "SSH") value.toCharArray(newSettings.sunset_hour, 3);
    else if (key == "SSM") value.toCharArray(newSettings.sunset_minute, 3);
    else if (key == "MODE") value.toCharArray(newSettings.capture_mode, 12);
    else if (key == "CDATE") value.toCharArray(newSettings.client_date, 11);
    else if (key == "CTIME") value.toCharArray(newSettings.client_time, 9);

    start = end + 1;
  }

  // Save to EEPROM
  memcpy(&currentSettings, &newSettings, sizeof(Settings));

  // Check if this packet is for time synchronization
  bool isTimeSync = istimeset;
  
  // Important: Reset the flag immediately to prevent it affecting future operations
  istimeset = false;

  if (isTimeSync) {
    // Update RTC with client date/time
    tmElements_t tmSet;
    memset(&tmSet, 0, sizeof(tmSet));
    bool dateValid = false;
    bool timeValid = false;

    // Parse client date (YYYY-MM-DD)
    int year, month, day;
    if (sscanf(newSettings.client_date, "%d-%d-%d", &year, &month, &day) == 3) {
      if (year >= 2024 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
        tmSet.Year = CalendarYrToTm(year);
        tmSet.Month = month;
        tmSet.Day = day;
        dateValid = true;
      }
    }

    int hour, minute, second;
    if (sscanf(newSettings.client_time, "%d:%d:%d", &hour, &minute, &second) == 3) {
      if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59) {
        tmSet.Hour = hour;
        tmSet.Minute = minute;
        tmSet.Second = second;
        timeValid = true;
      }
    }

    // Set RTC if both date and time are valid
    if (dateValid && timeValid) {
      time_t newTime = makeTime(tmSet);
      RTC.set(newTime);
      setTime(newTime);  // Update Arduino time library
      Serial.print("RTC updated to: ");
      Serial.print(year);
      Serial.print("-");
      Serial.print(month);
      Serial.print("-");
      Serial.print(day);
      Serial.print(" ");
      Serial.print(hour);
      Serial.print(":");
      Serial.print(minute);
      Serial.print(":");
      Serial.println(second);
    } else {
      Serial.println("Invalid date/time format");
    }
  } else {
    // Print confirmation for configuration update
    Serial.println("\nSettings Stored in EEPROM:");
    Serial.println("Checksum: " + String(currentSettings.checksum));
    Serial.println("Initial Time: " + String(currentSettings.initial_time_hour) + ":" + String(currentSettings.initial_time_minute));
    Serial.println("Interval: " + String(currentSettings.interval));
    Serial.println("Photos/Capture: " + String(currentSettings.photos_per_capture));
    Serial.println("Sunrise: " + String(currentSettings.sunrise_hour) + ":" + String(currentSettings.sunrise_minute));
    Serial.println("Sunset: " + String(currentSettings.sunset_hour) + ":" + String(currentSettings.sunset_minute));
    Serial.println("Mode: " + String(currentSettings.capture_mode));
  }
}
void setup() {
  Serial.begin(57600);
  delay(20);

  // Initialize RTC - Use only one RTC instance consistently
  Wire.begin();
  RTC.begin();

  setSyncProvider(RTC.get);  // the function to get the time from the RTC
  if (timeStatus() != timeSet)
    Serial.println("Unable to sync with the RTC");
  else
    Serial.println("RTC has set the system time");

  firstAlarmProcessed = false;

  // Properly clear and initialize RTC alarms
  RTC.setAlarm(DS3232RTC::ALM1_MATCH_DATE, 0, 0, 0, 1);  // Initialize alarm 1
  RTC.setAlarm(DS3232RTC::ALM2_MATCH_DATE, 0, 0, 0, 1);  // Initialize alarm 2
  RTC.alarm(DS3232RTC::ALARM_1);                         // Clear alarm 1 flag
  RTC.alarm(DS3232RTC::ALARM_2);                         // Clear alarm 2 flag
  RTC.alarmInterrupt(DS3232RTC::ALARM_1, false);         // Disable interrupt output for alarm 1
  RTC.alarmInterrupt(DS3232RTC::ALARM_2, false);         // Disable interrupt output for alarm 2
  RTC.squareWave(DS3232RTC::SQWAVE_NONE);                // Disable square wave for interrupt mode

  // Initializing shield pins
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);

  // Initialize pin for control of light
  servo.attach(servoPin);
  servo.writeMicroseconds(1100);  // send "off" signal to Lumen light
  servo.detach();
  digitalWrite(servoPin, LOW);

  // Configure interrupt pin and ensure it's properly set up
  pinMode(RTC_ALARM_PIN, INPUT_PULLUP);  // Enable pull-up resistor
  digitalWrite(RTC_ALARM_PIN, HIGH);     // Ensure it's HIGH when no interrupt

  // Critical: set interrupt mode to LOW to detect state, not just edge
  attachInterrupt(digitalPinToInterrupt(RTC_ALARM_PIN), alarmIsr, LOW);

  // Set up light sensor pin
  pinMode(LIGHTSENSORPIN, INPUT);


  // Status LED flashes to indicate completed setup
  digitalWrite(13, HIGH);
  delay(250);
  digitalWrite(13, LOW);
  delay(250);
  digitalWrite(13, HIGH);
  delay(250);
  digitalWrite(13, LOW);
  delay(250);
}

void loop() {
  handleSerialCommands();

  if (Operationstatus == true && Programmingstatus == false) {
    handleOperationalMode();
  }
  if (Operationstatus == false && Programmingstatus == true) {
    handleProgrammingMode();
  }
}