// =============================================
// SMART FOREST ROAD SAFETY SYSTEM
// Components: PIR + Ultrasonic + LCD + Buzzer
// For complete beginners — fully commented
// =============================================

#include <Wire.h>               // Built-in: I2C protocol
#include <LiquidCrystal_I2C.h>  // Install: LCD I2C control

// LCD setup: address 0x27, 16 columns, 2 rows
// If LCD shows nothing, try 0x3F instead of 0x27
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---- Pin assignments ----
const int PIR_PIN  = 7;   // PIR motion/heat sensor output
const int TRIG_PIN = 9;   // HC-SR04 trigger (send pulse)
const int ECHO_PIN = 10;  // HC-SR04 echo (receive pulse)
const int BUZZER   = 8;   // Active buzzer

// ---- Detection settings ----
const float MIN_DIST    = 10.0;  // cm - too close = ignore
const float MAX_DIST    = 200.0; // cm - 2 meter detection range
const long  COOLDOWN_MS = 6000;  // ms - 6 sec between alerts
const int   WARMUP_SECS = 30;    // PIR needs 30s to stabilise

unsigned long lastAlertTime = 0; // Tracks last alert time

// ---- SETUP: runs once at start ----
void setup() {
  Serial.begin(9600); // USB serial to PC

  // Set pin modes
  pinMode(PIR_PIN,  INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER,   OUTPUT);
  digitalWrite(BUZZER, LOW); // Buzzer off at start

  // Start LCD
  Wire.begin();
  lcd.init();
  lcd.backlight(); // Turn on LCD light

  // Show warmup message on LCD
  lcd.setCursor(0, 0); // Column 0, Row 0
  lcd.print("Forest Safety V1");
  lcd.setCursor(0, 1); // Column 0, Row 1
  lcd.print("Warming up...   ");
  Serial.println("STATUS,Warming up PIR sensor...");

  // Wait for PIR to stabilise (counts down on LCD)
  for (int i = WARMUP_SECS; i > 0; i--) {
    lcd.setCursor(0, 1);
    lcd.print("Ready in ");
    lcd.print(i);
    lcd.print("s   ");
    delay(1000);
  }

  // Warmup done
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready!   ");
  delay(1000);
  Serial.println("STATUS,System Ready!");
}

// ---- FUNCTION: Measure distance in cm ----
float getDistance() {
  // Send 10-microsecond pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure echo return time (timeout: 25ms)
  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration == 0) return 999.0; // No echo = nothing there

  // Distance formula: speed of sound = 0.0343 cm/microsecond
  // Divide by 2 because sound goes TO object and BACK
  return duration * 0.0343 / 2.0;
}

// ---- FUNCTION: Show ROAD CLEAR on LCD ----
void showClear() {
  lcd.setCursor(0, 0);
  lcd.print("  ROAD CLEAR    ");
  lcd.setCursor(0, 1);
  lcd.print("System Active   ");
}

// ---- FUNCTION: Show CAUTION state ----
void showCaution(float dist) {
  lcd.setCursor(0, 0);
  lcd.print("Caution! Object ");
  lcd.setCursor(0, 1);
  lcd.print("Dist: ");
  lcd.print((int)dist);
  lcd.print(" cm        "); // Spaces clear old text
}

// ---- FUNCTION: Show ALERT state + trigger camera ----
void showAlert(float dist) {
  // LCD alert message
  lcd.setCursor(0, 0);
  lcd.print("!ANIMAL ALERT!  ");
  lcd.setCursor(0, 1);
  lcd.print("Dist:");
  lcd.print((int)dist);
  lcd.print("cm PHOTO!");

  // Buzzer: 3 short beeps
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(250);
    digitalWrite(BUZZER, LOW);
    delay(150);
  }

  // Send command to Python on PC
  // Python will capture photo and upload to ThingSpeak
  Serial.print("CAPTURE,");
  Serial.println(dist);
}

// ---- MAIN LOOP: runs forever ----
void loop() {
  int   pirValue = digitalRead(PIR_PIN); // HIGH = motion+heat
  float dist     = getDistance();         // Distance in cm

  // Debug: print to Serial Monitor
  Serial.print("PIR:");
  Serial.print(pirValue);
  Serial.print(" DIST:");
  Serial.println(dist, 1); // 1 decimal place
 
  if (pirValue == HIGH && dist >= MIN_DIST && dist <= MAX_DIST) {
    // BOTH sensors triggered: confirmed animal!
    unsigned long now = millis();
    if (now - lastAlertTime > COOLDOWN_MS) {
      showAlert(dist);
      lastAlertTime = now;
    }
    // Keep showing alert on LCD even during cooldown
    lcd.setCursor(0, 0);
    lcd.print("!ANIMAL ALERT!  ");

  } else if (dist >= MIN_DIST && dist <= MAX_DIST) {
    // Only ultrasonic triggered: something close but not warm
    showCaution(dist);
    digitalWrite(BUZZER, LOW);

  } else {
    // Nothing detected: all clear
    showClear();
    digitalWrite(BUZZER, LOW);
  }

  delay(300); // Check ~3 times per second
}