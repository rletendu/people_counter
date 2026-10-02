#include <Arduino.h>
#include <TM1637Display.h>

// ---------- Pins ----------
const int TRIG_PIN  = 9;
const int ECHO_PIN  = 10;
const int RESET_BTN = 3;          // to GND, uses INPUT_PULLUP

TM1637Display display(4, 5);      // CLK = D4, DIO = D5

// ---------- Settings ----------
const int           MARGIN_CM       = 20;      // how much closer than the empty wall = detection
const byte          CONFIRM_READS   = 2;       // consistent readings required to change state
const unsigned long MAX_PRESENCE_MS = 10000;   // after this, the sensor is considered blocked
const unsigned long LOOP_DELAY_MS   = 40;      // ~25 measurements per second

// ---------- State ----------
unsigned int  passCount     = 0;
int           threshold     = 0;       // distance (cm) below which someone is present
bool          personPresent = false;
bool          blocked       = false;
byte          confirmCount  = 0;
unsigned long presenceStart = 0;

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
  int d = 0;
  while (d == 0) {
    d = readDistanceCm();
    display.showNumberDecEx(0, 0x40);   // colon lit while calibrating
    delay(200);
  }
  threshold = d - MARGIN_CM;
}

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RESET_BTN, INPUT_PULLUP);
  display.setBrightness(5);
  delay(1000);
  calibrate();
}

void loop() {
  // Reset counter
  if (!digitalRead(RESET_BTN)) {
    passCount = 0;
    delay(300);
  }

  // 0 (absorbed echo) also counts as a detection
  bool detected = readDistanceCm() < threshold;

  // State change with confirmation
  if (detected != personPresent) {
    if (++confirmCount >= CONFIRM_READS) {
      personPresent = detected;
      confirmCount = 0;

      if (personPresent) {
        presenceStart = millis();
        if (!blocked) passCount++;     // count on arrival, unless blocked
      } else {
        blocked = false;               // path is clear again, resume counting
      }
    }
  } else {
    confirmCount = 0;
  }

  // Someone or something stays too long: stop counting until it clears
  if (personPresent && !blocked && millis() - presenceStart > MAX_PRESENCE_MS) {
    blocked = true;
  }

  // Display: colon lit = sensor blocked
  if (blocked) {
    display.showNumberDecEx(passCount, 0x40);
  } else {
    display.showNumberDec(passCount);   // raw count, or passCount / 2 for entries
  }

  delay(LOOP_DELAY_MS);
}
