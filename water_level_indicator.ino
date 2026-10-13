/*
 * IoT-Based Water Level Indicator
 * Board   : ESP32 DevKit V1
 * Sensor  : JSN-SR04T waterproof ultrasonic (TRIG/ECHO mode)
 * Display : 16x2 I2C LCD (address 0x27)
 * Cloud   : Blynk IoT (live level, alerts, data logging, pump control)
 *
 * Libraries (Arduino Library Manager):
 *   - Blynk by Volodymyr Shymanskyy
 *   - LiquidCrystal I2C by Frank de Brabander
 *
 * Blynk template datastreams (Virtual Pins):
 *   V0 = Water level %        (Double, 0-100)
 *   V1 = Distance cm          (Double, 0-600)
 *   V2 = Pump state           (Integer, 0/1)
 *   V3 = Auto mode switch     (Integer, 0/1)  <- input from app
 *   V4 = Manual pump switch   (Integer, 0/1)  <- input from app
 * Blynk events (Events tab): tank_full, tank_low
 */

// ---------- Blynk credentials (fill in from your Blynk template) ----------
#define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "Water Level Indicator"
#define BLYNK_AUTH_TOKEN    "YOUR_AUTH_TOKEN"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- Wi-Fi ----------
const char WIFI_SSID[] = "YOUR_WIFI_NAME";
const char WIFI_PASS[] = "YOUR_WIFI_PASSWORD";

// ---------- Pins ----------
const int PIN_TRIG   = 5;
const int PIN_ECHO   = 18;   // through 1k/2k divider
const int PIN_RELAY  = 26;
const int PIN_BUZZER = 27;
const int PIN_LED_OK = 25;   // green
const int PIN_LED_AL = 33;   // red

// Most relay modules are active-LOW. Set to false if yours is active-HIGH.
const bool RELAY_ACTIVE_LOW = true;

// ---------- Tank calibration (centimetres) ----------
// EMPTY_DIST : sensor to tank bottom (tank empty)
// FULL_DIST  : sensor to water surface when tank is full (keep >= 20 cm,
//              the JSN-SR04T blind zone)
const float EMPTY_DIST = 100.0;
const float FULL_DIST  = 25.0;

// ---------- Thresholds (percent) ----------
const float PUMP_ON_BELOW   = 20.0;
const float PUMP_OFF_ABOVE  = 90.0;
const float ALERT_LOW_AT    = 20.0;
const float ALERT_FULL_AT   = 95.0;
const float ALERT_REARM_LOW = 30.0;   // level must rise above this to re-arm low alert
const float ALERT_REARM_FULL = 85.0;  // level must fall below this to re-arm full alert

// ---------- Timing (milliseconds) ----------
const unsigned long MEASURE_MS = 1000;
const unsigned long DISPLAY_MS = 1000;
const unsigned long UPLOAD_MS  = 5000;

// ---------- Globals ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);
BlynkTimer timer;

float distanceCm = 0;
float levelPct   = 0;
bool  sensorOk   = false;
bool  pumpOn     = false;
bool  autoMode   = true;
bool  manualPump = false;
bool  lowAlertArmed  = true;
bool  fullAlertArmed = true;

// ---------- Relay ----------
void setRelay(bool on) {
  pumpOn = on;
  digitalWrite(PIN_RELAY, (on ^ RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

// ---------- Ultrasonic reading ----------
float readDistanceOnce() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(4);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long dur = pulseIn(PIN_ECHO, HIGH, 30000UL);  // 30 ms timeout
  if (dur == 0) return -1;                               // no echo
  return dur * 0.0343f / 2.0f;
}

// Median of 5 readings removes ripple spikes and false echoes
float readDistanceMedian() {
  float v[5];
  int n = 0;
  for (int i = 0; i < 5; i++) {
    float d = readDistanceOnce();
    if (d > 0) v[n++] = d;
    delay(30);
  }
  if (n == 0) return -1;
  for (int i = 1; i < n; i++) {            // insertion sort
    float k = v[i];
    int j = i - 1;
    while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; }
    v[j + 1] = k;
  }
  return v[n / 2];
}

// ---------- Measurement, control and alerts ----------
void measureAndControl() {
  float d = readDistanceMedian();
  sensorOk = (d > 0);
  if (!sensorOk) {
    setRelay(false);                       // fail safe: stop pump if sensor is blind
    digitalWrite(PIN_LED_OK, LOW);
    digitalWrite(PIN_LED_AL, HIGH);
    return;
  }

  distanceCm = d;
  levelPct = (EMPTY_DIST - distanceCm) / (EMPTY_DIST - FULL_DIST) * 100.0f;
  levelPct = constrain(levelPct, 0.0f, 100.0f);

  // Pump control with hysteresis
  if (autoMode) {
    if (levelPct < PUMP_ON_BELOW)       setRelay(true);
    else if (levelPct > PUMP_OFF_ABOVE) setRelay(false);
  } else {
    setRelay(manualPump && levelPct < ALERT_FULL_AT);  // never pump into a full tank
  }

  // Local indication
  bool low  = levelPct <= ALERT_LOW_AT;
  bool full = levelPct >= ALERT_FULL_AT;
  digitalWrite(PIN_LED_OK, (!low && !full) ? HIGH : LOW);
  digitalWrite(PIN_LED_AL, (low || full) ? HIGH : LOW);
  digitalWrite(PIN_BUZZER, full ? HIGH : LOW);

  // Mobile alerts, sent once per event
  if (low && lowAlertArmed) {
    Blynk.logEvent("tank_low", String("Tank low: ") + String(levelPct, 0) + "%");
    lowAlertArmed = false;
  }
  if (levelPct > ALERT_REARM_LOW) lowAlertArmed = true;

  if (full && fullAlertArmed) {
    Blynk.logEvent("tank_full", String("Tank full: ") + String(levelPct, 0) + "%");
    fullAlertArmed = false;
  }
  if (levelPct < ALERT_REARM_FULL) fullAlertArmed = true;
}

// ---------- LCD ----------
void updateDisplay() {
  lcd.setCursor(0, 0);
  if (!sensorOk) {
    lcd.print("Sensor error    ");
    lcd.setCursor(0, 1);
    lcd.print("Check wiring    ");
    return;
  }
  char l1[17], l2[17];
  snprintf(l1, sizeof(l1), "Level: %3d%%      ", (int)(levelPct + 0.5f));
  snprintf(l2, sizeof(l2), "%-4s Pump:%-3s  ", WiFi.status() == WL_CONNECTED ? "WiFi" : "Off",
           pumpOn ? "ON" : "OFF");
  lcd.print(l1);
  lcd.setCursor(0, 1);
  lcd.print(l2);
}

// ---------- Cloud upload ----------
void uploadToCloud() {
  if (!Blynk.connected() || !sensorOk) return;
  Blynk.virtualWrite(V0, levelPct);
  Blynk.virtualWrite(V1, distanceCm);
  Blynk.virtualWrite(V2, pumpOn ? 1 : 0);
}

// ---------- Blynk inputs from the app ----------
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V3, V4);
}
BLYNK_WRITE(V3) { autoMode   = param.asInt() == 1; }
BLYNK_WRITE(V4) { manualPump = param.asInt() == 1; }

// ---------- Setup / loop ----------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_OK, OUTPUT);
  pinMode(PIN_LED_AL, OUTPUT);
  setRelay(false);
  digitalWrite(PIN_BUZZER, LOW);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Water Level IoT ");
  lcd.setCursor(0, 1);
  lcd.print("Starting...     ");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Blynk.config(BLYNK_AUTH_TOKEN);   // non-blocking: device works even without Wi-Fi

  timer.setInterval(MEASURE_MS, measureAndControl);
  timer.setInterval(DISPLAY_MS, updateDisplay);
  timer.setInterval(UPLOAD_MS, uploadToCloud);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();                    // connects and reconnects automatically
  }
  timer.run();
}
