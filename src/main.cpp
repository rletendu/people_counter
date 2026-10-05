#include <Arduino.h>
#include <EEPROM.h>
#include <TM1637Display.h>

#define DEBUG_SERIAL 0

#if DEBUG_SERIAL
#define DEBUG_PRINT(value) Serial.print(value)
#define DEBUG_PRINTLN(value) Serial.println(value)
#else
#define DEBUG_PRINT(value) ((void)0)
#define DEBUG_PRINTLN(value) ((void)0)
#endif

// ---------- Pins ----------
const int TRIG_PIN  = 9;
const int ECHO_PIN  = 10;
const int RESET_BTN = 3;          // to GND, uses INPUT_PULLUP
const int BUZZER_PIN = 6;

TM1637Display display(4, 5);      // CLK = D4, DIO = D5

// ---------- Settings ----------
const int           MARGIN_CM       = 20;      // how much closer than the empty wall = detection
const byte          CONFIRM_READS   = 2;       // consistent readings required to change state
const unsigned long MAX_PRESENCE_MS = 10000;   // after this, the sensor is considered blocked
const unsigned long LOOP_DELAY_MS   = 40;      // ~25 measurements per second
const unsigned long CALIBRATION_MIN_MS = 2000; // minimum snake animation duration
const unsigned long SNAKE_STEP_MS   = 80;      // snake animation speed
const unsigned int  CALIBRATION_BEEP_HZ = 1600;
const unsigned int  COUNT_BEEP_HZ   = 2200;
const unsigned int  BLOCKED_BEEP_FIRST_HZ  = 1100;
const unsigned int  BLOCKED_BEEP_SECOND_HZ = 700;
const unsigned long BEEP_DURATION_MS = 35;
const int           EEPROM_ADDRESS  = 0;
const uint16_t      EEPROM_MAGIC    = 0x5043;

struct CounterRecord {
  uint16_t magic;
  uint16_t count;
};

// ---------- State ----------
unsigned int  passCount     = 0;
int           threshold     = 0;       // distance (cm) below which someone is present
bool          personPresent = false;
bool          blocked       = false;
byte          confirmCount  = 0;
unsigned long presenceStart = 0;
#if DEBUG_SERIAL
unsigned long lastDebugPrint = 0;
#endif

void savePassCount() {
  CounterRecord record = {EEPROM_MAGIC, static_cast<uint16_t>(passCount)};
  EEPROM.put(EEPROM_ADDRESS, record);
}

void loadPassCount() {
  CounterRecord record;
  EEPROM.get(EEPROM_ADDRESS, record);
  if (record.magic == EEPROM_MAGIC) {
    passCount = record.count;
  } else {
    passCount = 0;
    savePassCount();
  }
}

// Snake animation: a 3-segment tail runs around the outline of the 4 digits.
// Path (digit, segment bit): top a, right b/c, bottom d (backwards), left e/f.
void showSnakeFrame(byte step) {
  const byte PATH_LEN = 12;
  const byte SNAKE_LEN = 3;
  const byte pathDigit[PATH_LEN] = {0, 1, 2, 3, 3, 3, 3, 2, 1, 0, 0, 0};
  const uint8_t pathSeg[PATH_LEN] = {0x01, 0x01, 0x01, 0x01, 0x02, 0x04,
                                     0x08, 0x08, 0x08, 0x08, 0x10, 0x20};
  uint8_t segs[4] = {0, 0, 0, 0};
  for (byte i = 0; i < SNAKE_LEN; i++) {
    byte p = (step + PATH_LEN - i) % PATH_LEN;
    segs[pathDigit[p]] |= pathSeg[p];
  }
  display.setSegments(segs);
}

void playBlockedAlert() {
  tone(BUZZER_PIN, BLOCKED_BEEP_FIRST_HZ, BEEP_DURATION_MS);
  delay(LOOP_DELAY_MS);
  tone(BUZZER_PIN, BLOCKED_BEEP_SECOND_HZ, BEEP_DURATION_MS);
}

// Returns distance in cm, or 0 if no echo (absorbed or out of range)
int readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 25000);   // timeout ~4 m
  return duration ? duration / 58 : 0;
}

void calibrate() {
  // Power on with an empty passage. Retry until a valid echo is received.
  DEBUG_PRINTLN(F("Calibration started; keep the passage empty."));
  int d = 0;
  byte snakeStep = 0;
  unsigned long calibrationStart = millis();
  while (d == 0 || millis() - calibrationStart < CALIBRATION_MIN_MS) {
    if (d == 0) {
      d = readDistanceCm();
      DEBUG_PRINT(F("Calibration reading="));
      DEBUG_PRINT(d);
      DEBUG_PRINTLN(F(" cm"));
    }
    showSnakeFrame(snakeStep++);
    delay(SNAKE_STEP_MS);
  }
  threshold = d - MARGIN_CM;
  DEBUG_PRINT(F("Initial distance="));
  DEBUG_PRINT(d);
  DEBUG_PRINT(F(" cm; calibration threshold="));
  DEBUG_PRINT(threshold);
  DEBUG_PRINTLN(F(" cm"));
  tone(BUZZER_PIN, CALIBRATION_BEEP_HZ, BEEP_DURATION_MS);
  delay(LOOP_DELAY_MS);
}

void setup() {
#if DEBUG_SERIAL
  Serial.begin(9600);
#endif
  loadPassCount();
  DEBUG_PRINT(F("People counter starting; saved count="));
  DEBUG_PRINTLN(passCount);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RESET_BTN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  display.setBrightness(5);
  delay(1000);
  calibrate();
}

void loop() {
  // Reset counter
  if (!digitalRead(RESET_BTN)) {
    if (passCount != 0) {
      passCount = 0;
      savePassCount();
      DEBUG_PRINTLN(F("Counter reset to 0"));
    }
    delay(300);
  }

  // 0 (absorbed echo) also counts as a detection
  int distanceCm = readDistanceCm();
  bool detected = distanceCm < threshold;

#if DEBUG_SERIAL
  if (millis() - lastDebugPrint >= 1000) {
    lastDebugPrint = millis();
    DEBUG_PRINT(F("distance="));
    DEBUG_PRINT(distanceCm);
    DEBUG_PRINT(F(" cm, detected="));
    DEBUG_PRINT(detected ? F("yes") : F("no"));
    DEBUG_PRINT(F(", present="));
    DEBUG_PRINT(personPresent ? F("yes") : F("no"));
    DEBUG_PRINT(F(", blocked="));
    DEBUG_PRINT(blocked ? F("yes") : F("no"));
    DEBUG_PRINT(F(", count="));
    DEBUG_PRINTLN(passCount);
  }
#endif

  // State change with confirmation
  if (detected != personPresent) {
    if (++confirmCount >= CONFIRM_READS) {
      personPresent = detected;
      confirmCount = 0;
      DEBUG_PRINT(F("Presence changed: "));
      DEBUG_PRINTLN(personPresent ? F("detected") : F("clear"));

      if (personPresent) {
        presenceStart = millis();
        if (!blocked) {
          passCount++;                 // count on arrival, unless blocked
          savePassCount();
          DEBUG_PRINT(F("Passage counted; count="));
          DEBUG_PRINTLN(passCount);
          tone(BUZZER_PIN, COUNT_BEEP_HZ, BEEP_DURATION_MS);
        } else {
          DEBUG_PRINTLN(F("Passage not counted; sensor is blocked"));
        }
      } else {
        blocked = false;               // path is clear again, resume counting
        DEBUG_PRINTLN(F("Path clear; counting resumed"));
      }
    }
  } else {
    confirmCount = 0;
  }

  // Someone or something stays too long: stop counting until it clears
  if (personPresent && !blocked && millis() - presenceStart > MAX_PRESENCE_MS) {
    blocked = true;
    DEBUG_PRINTLN(F("Sensor blocked: presence exceeded timeout"));
    playBlockedAlert();
  }

  // Display: colon lit = sensor blocked
  if (blocked) {
    display.showNumberDecEx(passCount, 0x40);
  } else {
    display.showNumberDec(passCount);   // raw count, or passCount / 2 for entries
  }

  delay(LOOP_DELAY_MS);
}
