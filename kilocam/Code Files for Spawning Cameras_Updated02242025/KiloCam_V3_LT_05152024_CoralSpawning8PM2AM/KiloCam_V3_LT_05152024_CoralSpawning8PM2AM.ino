// WELCOME TO THE CODE FOR KILOCAM V3

// NOTE: This code should only be used with KiloCam V3 boards and ESP32-CAM boards that are also running the code for KiloCam V3 serial comms.
// Don't know your board version? Check the PCB for a version number. Most KiloCam V2 and older boards are green. KiloCam V3 is blue. 

// A huge thank you to KiloCam user Mark Pinches to helping develop serial communications used in this version of KiloCam.

// Load libraries 
#include <DS3232RTC.h>        // https://github.com/JChristensen/DS3232RTC
#include <LowPower.h>         // https://github.com/rocketscream/Low-Power
#include <Wire.h>
#include <SPI.h>
#include <Servo.h>

// Define constants
#define RTC_ALARM_PIN 2 //pin for interrupt 
#define LIGHTSENSORPIN A6
#define TEMPSENSORPIN A7

// Object instantiations
time_t          t, alarmTime, alarmTimestamp;
tmElements_t    tm;
DS3232RTC RTC;

// Object for LED control on pin D6 breakout
byte servoPin = 6; //Is usually 6, changed to 3 for the test of alternate board 
Servo servo;

// Global variable declarations
volatile bool alarmIsrWasCalled = true;    // DS3231 RTC alarm interrupt service routine (ISR) flag. Set to true to allow the first iteration of the loop to take place and sleep the system.
unsigned long alarmInt;

// ADJUST KILOCAM SETTINGS HERE...
// First, set the number of photos PER BURST you want. More photos per burst = less battery life.
unsigned int NumPics = 4; // Must be an integer from 1 to 9.

// Next, set the hour and minute you want KiloCam to wake after being powered.
unsigned int Hour = 13; // Set what hour KiloCam wakes up tomorrow (24 hour clock)
unsigned int Minute = 0; // Set what minute KiloCam wakes up tomorrow

// Next, set the interval between photos
unsigned long DayInterval = 120;  // Set sleep duration (in seconds) between data samples during daylight
unsigned long NightInterval = 120;  // DISABLED by default, see below. Set sleep duration (in seconds) between data samples during nighttime

// Last, set the hours during which you want KiloCam to operate 
unsigned int Dawn = 2; // Set the earliest hour (24 hour clock) you want captures to occur in. Or the time to move to the night interval.
unsigned int Dusk = 12; // Set the latest hour (24 hour clock) you want captures to occur in. Or the time to move to the day interval.

void setup()
{
  // DS3231 Real-time clock (RTC)
  RTC.setAlarm(DS3232RTC::ALM1_MATCH_DATE, 0, 0, 0, 1);    // Initialize alarm 1 to a known value
  RTC.setAlarm(DS3232RTC::ALM2_MATCH_DATE, 0, 0, 0, 1);    // Initialize alarm 2 to a known value
  RTC.alarm(DS3232RTC::ALARM_1);                           // Clear alarm 1 interrupt flag
  RTC.alarm(DS3232RTC::ALARM_2);                           // Clear alarm 2 interrupt flag
  RTC.alarmInterrupt(DS3232RTC::ALARM_1, false);           // Disable interrupt output for alarm 1
  RTC.alarmInterrupt(DS3232RTC::ALARM_2, false);           // Disable interrupt output for alarm 2
  RTC.squareWave(DS3232RTC::SQWAVE_NONE);                  // Configure INT/SQW pin for interrupt operation by disabling default square wave output

  // Initializing shield pins
  pinMode(5, OUTPUT); // Pin D5 controlling power to ESP32-CAM
  digitalWrite(5, LOW); // Set D5 to LOW, power to ESP32-CAM is off

  // Initialize pin for control of light 
  servo.attach(servoPin);
  servo.writeMicroseconds(1100); // send "off" signal to Lumen light
  servo.detach();
  digitalWrite(servoPin, LOW);

  // Initialize serial monitor
  Serial.begin(57600);

  pinMode(RTC_ALARM_PIN, INPUT);           // Enable internal pull-up resistor on external interrupt pin
  digitalWrite(RTC_ALARM_PIN, HIGH);
  attachInterrupt(0, alarmIsr, FALLING);       // Wake on falling edge of RTC_ALARM_PIN

  // Set up light sensor pin as an analog input - acts as a voltage divider circuit
  pinMode(LIGHTSENSORPIN,  INPUT); 

  // Set initial RTC alarm
  RTC.setAlarm(DS3232RTC::ALM1_MATCH_HOURS, 0, Minute, Hour, 0);  // STEP 1: Set the initial alarm 1 here. The camera is inactive until this time/date.
  RTC.alarm(DS3232RTC::ALARM_1);                             // Ensure alarm 1 interrupt flag is cleared
  RTC.alarmInterrupt(DS3232RTC::ALARM_1, true);              // Enable interrupt output for alarm

  // Flash two short times to show KiloCam is powered on
  digitalWrite(13, HIGH);
  delay(250); 
  digitalWrite(13, LOW);   
  delay(250); 
  digitalWrite(13, HIGH);
  delay(250); 
  digitalWrite(13, LOW);   
  delay(250); 

  t = RTC.get(); // get the time 
  RunCamera_LED(); // do a demo photo cycle before going to sleep to verify all buttons work. Comment out this line with "//" to disable
}

// Loop
void loop()
{
  if (alarmIsrWasCalled)
  {
    Serial.println(F("Alarm ISR set to True! Waking up."));
    t = RTC.get();            // Read the current date and time from RTC
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

    if (RTC.alarm(DS3232RTC::ALARM_1))   // Check alarm 1 and clear the flag if set
    { 
      if  (hour(t) >= Dawn && hour(t) <= Dusk) { // Only run the camera for capture if the current RTC time is between dawn and dusk
        //Serial.println(F("Daytime alarm activated."));
        alarmInt = DayInterval;
        alarmTime = RTC.get() + alarmInt;  // Calculate the next alarm
        //RunCamera(); 
      }
      else {
        // ********** Comment out the four lines below if you do not want night photos*********
        // //Serial.println(F("Nighttime alarm activated."));
        alarmInt = NightInterval;
        alarmTime = RTC.get() + alarmInt;
        RunCamera_LED();
      }
      
      // Set the alarm
      Serial.println(F("Setting a new alarm."));
      // alarmTime = alarmTimestamp + alarmInt;  // Calculate the next alarm
      RTC.setAlarm(DS3232RTC::ALM1_MATCH_DATE, second(alarmTime), minute(alarmTime), hour(alarmTime), day(alarmTime));   // Set the alarm
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
      if (RTC.get() >= alarmTime)
      {
        Serial.println(F("The new alarm has already passed! Setting the next one."));
        alarmTime = alarmTimestamp + alarmInt;    // Calculate the next alarm
        RTC.setAlarm(DS3232RTC::ALM1_MATCH_DATE, second(alarmTime), minute(alarmTime), hour(alarmTime), day(alarmTime));
        RTC.alarm(DS3232RTC::ALARM_1);               // Ensure the alarm 1 interrupt flag is cleared
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
    }
    
    alarmIsrWasCalled = false;  // Reset the RTC ISR flag
    Serial.println(F("Alarm ISR set to False"));
    goToSleep();                // Sleep
  }
}

// Enable sleep and await the RTC alarm interrupt
void goToSleep()
{
  Serial.println(F("Going to sleep..."));
  Serial.flush();
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);  // Enter sleep and await an external interrupt 
}

// Real-time clock alarm interrupt service routine (ISR)
void alarmIsr()
{
  alarmIsrWasCalled = true;
}

// Function to send data for each picture
// Sends a package of temperature data, light data, and date/time each time the ESP32 requests it
void SendPhotoData() { 
    int LIGHT = 0;  // Initialize LIGHT to 0

  // Read the light sensor three times with a 100 ms delay in between
  for (int i = 0; i < 3; i++) {
    LIGHT += analogRead(LIGHTSENSORPIN);
    delay(100);  // 100 ms delay
  }

  // Calculate the average of the three readings
  LIGHT /= 3;

  // Read the TEMP sensor three times and calculate the average (note: reported as an integer)
  int TEMP = 0;  // Initialize LIGHT to 0

  // Read the TEMP sensor three times with a 100 ms delay in between
  for (int i = 0; i < 3; i++) {
    int sensorValue = analogRead(TEMPSENSORPIN);
    float voltage = sensorValue * (3.3 / 1023.0);
    float temperature = voltage - 0.5;
    temperature = temperature / 0.01;
    TEMP += temperature;
    delay(100);  // 100 ms delay
  }

  // Calculate the average of the three readings
  TEMP /= 3; // average temp reading

  // Get the current timestamp
  time_t t = RTC.get();

  // Format the timestamp in the required format (YYYYMMDDHHMMSS) (15 characters)
  char timestamp[15];
  sprintf(timestamp, "%04d%02d%02d%02d%02d%02d", year(t), month(t), day(t), hour(t), minute(t), second(t));

  // Create the data string in the specified format
  char dataString[50];
  snprintf(dataString, sizeof(dataString), "H_%s_%d_%d", timestamp, LIGHT, TEMP);

  // Send the data string via Serial
  Serial.print(dataString);  
  Serial.print("\n");
}

void RunCamera() {
    // Set up a 30-second window for data capture to occur in. 
    unsigned long currentMillis = millis(); // Get the time in case ESP32 doesn't send a signal
    unsigned long shutdownMillis = currentMillis + 30000; 

    // Flash two short times to show the unit is awake 
    digitalWrite(13, HIGH);  // Flash the LED to show the cycle is about to start
    delay(250);
    digitalWrite(13, LOW);

    // Attach Lumen LED control pin 
    servo.attach(servoPin);

    // Serial.println(F("Booting ESP32-CAM."));
    Serial.flush(); // Flush before serial comms to ESP32
    digitalWrite(5, HIGH);   // Pull pin 5 high to power on ESP32-CAM

    // Wait for ESP32 to wake and send a "P" on signal
    while (Serial.read() != 'P' && (millis() <= shutdownMillis)) {
      // No actions here
    };

    // Send the payload with NumPics and await confirmation. 
    // Create the NumPics string in the specified format
    char NumPicsString[4];
    snprintf(NumPicsString, sizeof(NumPicsString), "N_%d", NumPics);

    // Send the data string via Serial
    Serial.print(NumPicsString);  
    Serial.print("\n");

    digitalWrite(13, HIGH);  // Flash the LED to show data sent
    delay(50);
    digitalWrite(13, LOW);

    // Once ESP32 confirms it knows NumPics, send new data for each photo
    // Turn on the Lumen LED for each photo, and off after each. 
    for (int I = 0; I < NumPics; I++) {

      // Wait for ESP32 to send an "L" signal for data and LED on
      while (Serial.read() != 'L' && (millis() <= shutdownMillis)) {
      // No actions here
      };

      // Turn on the Lumen LED for the photo
      //servo.writeMicroseconds(1900); // Max brightness is 1900, off is 1100. Adjust as needed.
      //delay(500); // Delay between captures in a series
      
      // Send data for this photo
      SendPhotoData();
      //Serial.println("L"); // Tell ESP32-CAM LED is on

      // Wait for ESP32 to send a "D" signal that the photo is captured
      while (Serial.read() != 'D' && (millis() <= shutdownMillis)) {
      // No actions here
      };

      // Turn off the Lumen LED after the photo
      //servo.writeMicroseconds(1100); // Turn off the LED

      Serial.flush();
    }

    // Wait for ESP32 to save files and send an Off "Q" signal for power down
    while (Serial.read() != 'Q' && (millis() <= shutdownMillis)) {
      // No actions here
    };

    // Safety to confirm the LED is powered off and servopin pulled low before sleep
    servo.writeMicroseconds(1100); // Turn off the LED
    delay(250); 
    servo.detach();
    digitalWrite(servoPin, LOW); // Pull the pin low. Else the LED turns on during sleep.
    
    digitalWrite(5, LOW);    // Pull the pin 5 low to power off ESP32-CAM 

    digitalWrite(13, HIGH);  // Flash the blue LED to show the cycle has completed.
    delay(250);
    digitalWrite(13, LOW);
    delay(250);
    digitalWrite(13, HIGH);  // Flash the blue LED to show the cycle has completed.
    delay(250);
    digitalWrite(13, LOW);
    Serial.println(F("ESP32-CAM Shutdown."));
    
    // delay(500); // wait 1 second before powering down KiloCam
}

// Function to run the camera WITH LED for night photos
void RunCamera_LED() {
    // Set up a 30-second window for data capture to occur in. 
    unsigned long currentMillis = millis(); // Get the time in case ESP32 doesn't send a signal
    unsigned long shutdownMillis = currentMillis + 30000; 

    // Flash two short times to show the unit is awake 
    digitalWrite(13, HIGH);  // Flash the LED to show the cycle is about to start
    delay(250);
    digitalWrite(13, LOW);

    // Attach Lumen LED control pin 
    servo.attach(servoPin);

    // Serial.println(F("Booting ESP32-CAM."));
    Serial.flush(); // Flush before serial comms to ESP32
    digitalWrite(5, HIGH);   // Pull pin 5 high to power on ESP32-CAM

    // Wait for ESP32 to wake and send a "P" on signal
    while (Serial.read() != 'P' && (millis() <= shutdownMillis)) {
      // No actions here
    };

    // Send the payload with NumPics and await confirmation. 
    // Create the NumPics string in the specified format
    char NumPicsString[4];
    snprintf(NumPicsString, sizeof(NumPicsString), "N_%d", NumPics);

    // Send the data string via Serial
    Serial.print(NumPicsString);  
    Serial.print("\n");

    digitalWrite(13, HIGH);  // Flash the LED to show data sent
    delay(50);
    digitalWrite(13, LOW);

    // Once ESP32 confirms it knows NumPics, send new data for each photo
    // Turn on the Lumen LED for each photo, and off after each. 
    for (int I = 0; I < NumPics; I++) {

      // Wait for ESP32 to send an "L" signal for data and LED on
      while (Serial.read() != 'L' && (millis() <= shutdownMillis)) {
      // No actions here
      };

      // Turn on the Lumen LED for the photo
      servo.writeMicroseconds(1900); // Max brightness is 1900, off is 1100. Adjust as needed.
      //delay(500); // Delay between captures in a series
      
      // Send data for this photo
      SendPhotoData();
      //Serial.println("L"); // Tell ESP32-CAM LED is on

      // Wait for ESP32 to send a "D" signal that the photo is captured
      while (Serial.read() != 'D' && (millis() <= shutdownMillis)) {
      // No actions here
      };

      // Turn off the Lumen LED after the photo
      servo.writeMicroseconds(1100); // Turn off the LED

      Serial.flush();
    }

    // Wait for ESP32 to save files and send an Off "Q" signal for power down
    while (Serial.read() != 'Q' && (millis() <= shutdownMillis)) {
      // No actions here
    };

    // Safety to confirm the LED is powered off and servopin pulled low before sleep
    servo.writeMicroseconds(1100); // Turn off the LED
    delay(250); 
    servo.detach();
    digitalWrite(servoPin, LOW); // Pull the pin low. Else the LED turns on during sleep.
    
    digitalWrite(5, LOW);    // Pull the pin 5 low to power off ESP32-CAM 

    digitalWrite(13, HIGH);  // Flash the blue LED to show the cycle has completed.
    delay(250);
    digitalWrite(13, LOW);
    delay(250);
    digitalWrite(13, HIGH);  // Flash the blue LED to show the cycle has completed.
    delay(250);
    digitalWrite(13, LOW);
    Serial.println(F("ESP32-CAM Shutdown."));
    
    // delay(500); // wait 1 second before powering down KiloCam
}
