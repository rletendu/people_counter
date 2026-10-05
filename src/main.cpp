#include <Arduino.h>
#include <EEPROM.h>
#include <TM1637Display.h>
#include <Wire.h>
#include <VL53L1X.h>

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
const int BTN_A_PIN = 3;          // to GND, uses INPUT_PULLUP; reset combo + menu + peek
const int BTN_B_PIN = 2;          // to GND, uses INPUT_PULLUP; reset combo + menu + peek
const int BUZZER_PIN = 6;
const int LASER_PIN = 7;          // aiming laser, via transistor (module draws more than a pin can source directly)

TM1637Display display(4, 5);      // CLK = D4, DIO = D5
VL53L1X tofSensor;                 // I2C: SDA = A4, SCL = A5

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
const uint16_t      EEPROM_MAGIC    = 0x5046;

// Buttons / menu timings
const unsigned long LONG_PRESS_MS    = 1200; // single button held alone -> open the menu
const unsigned long RESET_HOLD_MS    = 5000; // both buttons held together -> reset the counter
const unsigned long MENU_TIMEOUT_MS  = 4000; // inactivity in the menu -> confirm and exit
const unsigned long MENU_BLINK_MS    = 300;  // menu display blink period
const unsigned long PEEK_DURATION_MS = 1200; // short press (outside the menu) distance preview

// Extra sound feedback
const unsigned int  MENU_ENTER_BEEP_FIRST_HZ  = 1800;
const unsigned int  MENU_ENTER_BEEP_SECOND_HZ = 2400;
const unsigned int  MENU_EXIT_BEEP_HZ         = 1500;
const unsigned int  RESET_BEEP_FIRST_HZ       = 500;
const unsigned int  RESET_BEEP_SECOND_HZ      = 900;
const unsigned int  CLICK_BEEP_HZ             = 3000;
const unsigned long CLICK_DURATION_MS         = 15;

enum class SensorType : uint8_t { Ultrasonic = 0, Tof = 1 };

// ToF region-of-interest width, narrowing the detection cone from the default
// ~27 deg (Large) down to ~20 deg (Medium) or ~15 deg (Narrow, ST's recommended
// minimum) at the cost of less reflected light collected (shorter range / noisier).
enum class RoiLevel : uint8_t { Large = 0, Medium = 1, Narrow = 2 };
const uint8_t ROI_WIDTH_FOR_LEVEL[3] = {16, 8, 4};

struct CounterRecord {
  uint16_t magic;
  uint16_t count;
  uint8_t  sensorMode;   // SensorType
  uint8_t  roiLevel;     // RoiLevel
  uint16_t sequence;     // bumped on every write, to find the newest slot at boot
  uint8_t  checksum;     // catches a slot left half-written by a power loss
};

// Ring buffer of slots spanning the whole 1 KB EEPROM: each save rotates to the
// next slot instead of rewriting the same cell, multiplying write endurance by
// EEPROM_SLOT_COUNT (~128x, so ~100k becomes ~12.8M writes).
const int EEPROM_SLOT_COUNT = 1024 / sizeof(CounterRecord);

int     currentSlot  = 0;
uint16_t lastSequence = 0;

uint8_t computeChecksum(const CounterRecord &record) {
  uint8_t sum = 0;
  sum += record.magic & 0xFF;
  sum += record.magic >> 8;
  sum += record.count & 0xFF;
  sum += record.count >> 8;
  sum += record.sensorMode;
  sum += record.roiLevel;
  sum += record.sequence & 0xFF;
  sum += record.sequence >> 8;
  return sum;
}

// ---------- State ----------
unsigned int  passCount     = 0;
int           threshold     = 0;       // distance (cm) below which someone is present
bool          personPresent = false;
bool          blocked       = false;
byte          confirmCount  = 0;
unsigned long presenceStart = 0;
SensorType    currentSensor   = SensorType::Ultrasonic;
RoiLevel      currentRoiLevel = RoiLevel::Large;
bool          tofReady      = false;   // true if the VL53L1X answered during setup()
#if DEBUG_SERIAL
unsigned long lastDebugPrint = 0;
#endif

// Button / menu state
unsigned long pressStartA = 0;
unsigned long pressStartB = 0;
bool          wasAPressed = false;
bool          wasBPressed = false;
bool          resetArmed  = false;
bool          comboHandled = false;    // true once a combo (reset) fired, until both released
bool          resetCountdownActive = false;  // both buttons held, counting down to reset
int           resetCountdownValue  = 0;      // seconds remaining, shown on the display

bool          inMenu              = false;
int           openerPin           = -1;
bool          menuIgnoreRelease   = false;
SensorType    menuSensorSelection = SensorType::Ultrasonic;  // toggled by button A
RoiLevel      menuRoiSelection    = RoiLevel::Large;         // cycled by button B
unsigned long menuLastActivity    = 0;
unsigned long menuBlinkStart      = 0;

bool          peekActive = false;
unsigned long peekUntil  = 0;

void calibrate();

void setLaser(bool on) {
  digitalWrite(LASER_PIN, on ? HIGH : LOW);
}

void applyRoiSize() {
  if (!tofReady) return;
  uint8_t width = ROI_WIDTH_FOR_LEVEL[static_cast<uint8_t>(currentRoiLevel)];
  tofSensor.setROISize(width, width);
}

void saveState() {
  currentSlot = (currentSlot + 1) % EEPROM_SLOT_COUNT;
  lastSequence++;
  CounterRecord record = {EEPROM_MAGIC, static_cast<uint16_t>(passCount),
                           static_cast<uint8_t>(currentSensor),
                           static_cast<uint8_t>(currentRoiLevel), lastSequence, 0};
  record.checksum = computeChecksum(record);
  EEPROM.put(currentSlot * sizeof(CounterRecord), record);
}

// Scans every slot of the ring buffer and keeps the newest valid one (highest
// sequence number, with wraparound handled via signed 16-bit comparison).
void loadState() {
  bool found = false;
  CounterRecord best;
  int bestSlot = 0;
  for (int slot = 0; slot < EEPROM_SLOT_COUNT; slot++) {
    CounterRecord record;
    EEPROM.get(slot * sizeof(CounterRecord), record);
    if (record.magic != EEPROM_MAGIC || record.checksum != computeChecksum(record)) {
      continue;
    }
    if (!found || (int16_t)(record.sequence - best.sequence) > 0) {
      best = record;
      bestSlot = slot;
      found = true;
    }
  }
  if (found) {
    passCount = best.count;
    currentSensor = (best.sensorMode == static_cast<uint8_t>(SensorType::Tof))
                        ? SensorType::Tof
                        : SensorType::Ultrasonic;
    currentRoiLevel = (best.roiLevel <= static_cast<uint8_t>(RoiLevel::Narrow))
                          ? static_cast<RoiLevel>(best.roiLevel)
                          : RoiLevel::Large;
    currentSlot = bestSlot;
    lastSequence = best.sequence;
  } else {
    passCount = 0;
    currentSensor = SensorType::Ultrasonic;
    currentRoiLevel = RoiLevel::Large;
    currentSlot = -1;    // so the first saveState() below lands on slot 0
    lastSequence = 0;
    saveState();
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

void playMenuEnterSound() {
  tone(BUZZER_PIN, MENU_ENTER_BEEP_FIRST_HZ, BEEP_DURATION_MS);
  delay(LOOP_DELAY_MS);
  tone(BUZZER_PIN, MENU_ENTER_BEEP_SECOND_HZ, BEEP_DURATION_MS);
}

void playMenuExitSound() {
  tone(BUZZER_PIN, MENU_EXIT_BEEP_HZ, BEEP_DURATION_MS);
}

void playResetConfirmSound() {
  tone(BUZZER_PIN, RESET_BEEP_FIRST_HZ, BEEP_DURATION_MS);
  delay(LOOP_DELAY_MS);
  tone(BUZZER_PIN, RESET_BEEP_SECOND_HZ, BEEP_DURATION_MS);
}

void playClickSound() {
  tone(BUZZER_PIN, CLICK_BEEP_HZ, CLICK_DURATION_MS);
}

// Returns distance in cm, or 0 if no echo (absorbed or out of range)
int readDistanceUltrasonicCm() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 25000);   // timeout ~4 m
  return duration ? duration / 58 : 0;
}

// Returns distance in cm, or 0 if the ToF sensor isn't ready / has no valid reading
int readDistanceTofCm() {
  if (!tofReady) return 0;
  tofSensor.read();
  if (tofSensor.timeoutOccurred() || tofSensor.ranging_data.range_status != VL53L1X::RangeValid) {
    return 0;
  }
  return tofSensor.ranging_data.range_mm / 10;
}

// Dispatches to whichever sensor is currently selected. 0 always means
// "no valid reading", same convention for both sensor types.
int readDistanceCm() {
  return currentSensor == SensorType::Tof ? readDistanceTofCm() : readDistanceUltrasonicCm();
}

void calibrate() {
  // Power on with an empty passage. Retry until a valid echo is received.
  DEBUG_PRINTLN(F("Calibration started; keep the passage empty."));
  setLaser(true);
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
  setLaser(false);
}

// Reads both buttons, drives the reset combo / menu / distance-peek state
// machine. Called once per loop() iteration, before anything else.
void updateButtons() {
  unsigned long now = millis();
  bool aPressed = !digitalRead(BTN_A_PIN);
  bool bPressed = !digitalRead(BTN_B_PIN);

  bool releasedA = wasAPressed && !aPressed;
  bool releasedB = wasBPressed && !bPressed;
  unsigned long durationA = releasedA ? now - pressStartA : 0;
  unsigned long durationB = releasedB ? now - pressStartB : 0;

  if (aPressed && !wasAPressed) pressStartA = now;
  if (!aPressed) pressStartA = 0;
  if (bPressed && !wasBPressed) pressStartB = now;
  if (!bPressed) pressStartB = 0;
  wasAPressed = aPressed;
  wasBPressed = bPressed;

  if (inMenu) {
    bool ignoredReleaseA = false;
    bool ignoredReleaseB = false;
    if (menuIgnoreRelease && openerPin == BTN_A_PIN && releasedA) {
      menuIgnoreRelease = false;
      ignoredReleaseA = true;
    } else if (menuIgnoreRelease && openerPin == BTN_B_PIN && releasedB) {
      menuIgnoreRelease = false;
      ignoredReleaseB = true;
    }
    // Button A cycles the sensor, button B cycles the ToF ROI width — independent axes.
    if (!ignoredReleaseA && releasedA) {
      menuSensorSelection = (menuSensorSelection == SensorType::Ultrasonic) ? SensorType::Tof
                                                                             : SensorType::Ultrasonic;
      menuLastActivity = now;
      playClickSound();
    }
    if (!ignoredReleaseB && releasedB) {
      menuRoiSelection = static_cast<RoiLevel>((static_cast<uint8_t>(menuRoiSelection) + 1) % 3);
      menuLastActivity = now;
      playClickSound();
    }
    if (now - menuLastActivity >= MENU_TIMEOUT_MS) {
      bool sensorChanged = (menuSensorSelection != currentSensor);
      bool roiChanged = (menuRoiSelection != currentRoiLevel);
      currentSensor = menuSensorSelection;
      currentRoiLevel = menuRoiSelection;
      if (roiChanged) applyRoiSize();
      saveState();
      inMenu = false;
      openerPin = -1;
      menuIgnoreRelease = false;
      DEBUG_PRINT(F("Menu closed; sensor="));
      DEBUG_PRINT(currentSensor == SensorType::Tof ? F("ToF") : F("Ultrasonic"));
      DEBUG_PRINT(F(", roiWidth="));
      DEBUG_PRINTLN(ROI_WIDTH_FOR_LEVEL[static_cast<uint8_t>(currentRoiLevel)]);
      playMenuExitSound();
      if (sensorChanged || (roiChanged && currentSensor == SensorType::Tof)) {
        delay(LOOP_DELAY_MS);
        calibrate();
      }
    }
    return;
  }

  bool bothHeld = aPressed && bPressed;
  if (bothHeld) {
    unsigned long heldMs = min(now - pressStartA, now - pressStartB);
    if (!resetArmed && heldMs < RESET_HOLD_MS) {
      resetCountdownActive = true;
      resetCountdownValue = (RESET_HOLD_MS - heldMs + 999) / 1000;  // ceil to whole seconds
    } else {
      resetCountdownActive = false;
    }
  } else {
    resetCountdownActive = false;
  }
  if (bothHeld && !resetArmed &&
      now - pressStartA >= RESET_HOLD_MS && now - pressStartB >= RESET_HOLD_MS) {
    resetArmed = true;
    comboHandled = true;
    if (passCount != 0) {
      passCount = 0;
      saveState();
      DEBUG_PRINTLN(F("Counter reset to 0"));
    }
    playResetConfirmSound();
  }
  if (!bothHeld) {
    resetArmed = false;
  }
  if (!aPressed && !bPressed) {
    comboHandled = false;
  }

  bool onlyA = aPressed && !bPressed;
  bool onlyB = bPressed && !aPressed;
  if (!comboHandled) {
    if (onlyA && now - pressStartA >= LONG_PRESS_MS) {
      inMenu = true;
      openerPin = BTN_A_PIN;
    } else if (onlyB && now - pressStartB >= LONG_PRESS_MS) {
      inMenu = true;
      openerPin = BTN_B_PIN;
    }
    if (inMenu) {
      menuIgnoreRelease = true;
      menuSensorSelection = currentSensor;
      menuRoiSelection = currentRoiLevel;
      menuLastActivity = now;
      menuBlinkStart = now;
      DEBUG_PRINTLN(F("Menu opened"));
      playMenuEnterSound();
      return;
    }
  }

  // Short press (released before reaching the long-press threshold): preview
  // the active sensor's instantaneous distance for a short while.
  if (releasedA && durationA < LONG_PRESS_MS) {
    peekActive = true;
    peekUntil = now + PEEK_DURATION_MS;
    setLaser(true);
    playClickSound();
  } else if (releasedB && durationB < LONG_PRESS_MS) {
    peekActive = true;
    peekUntil = now + PEEK_DURATION_MS;
    setLaser(true);
    playClickSound();
  }
}

void setup() {
#if DEBUG_SERIAL
  Serial.begin(9600);
#endif
  loadState();
  DEBUG_PRINT(F("People counter starting; saved count="));
  DEBUG_PRINTLN(passCount);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BTN_A_PIN, INPUT_PULLUP);
  pinMode(BTN_B_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LASER_PIN, OUTPUT);
  setLaser(false);
  display.setBrightness(5);

  Wire.begin();
  tofSensor.setTimeout(500);
  tofReady = tofSensor.init();
  if (tofReady) {
    tofSensor.setDistanceMode(VL53L1X::Long);
    tofSensor.setMeasurementTimingBudget(50000);
    applyRoiSize();
    tofSensor.startContinuous(50);
    DEBUG_PRINTLN(F("VL53L1X ready"));
  } else {
    DEBUG_PRINTLN(F("VL53L1X not found; ToF mode will stall in calibration"));
  }

  delay(1000);
  calibrate();
}

void loop() {
  unsigned long now = millis();
  updateButtons();

  if (inMenu) {
    bool blinkOn = ((now - menuBlinkStart) / MENU_BLINK_MS) % 2 == 0;
    if (blinkOn) {
      int sensorDigit = (menuSensorSelection == SensorType::Tof) ? 2 : 1;
      int roiDigit = static_cast<int>(menuRoiSelection) + 1;
      display.showNumberDec(sensorDigit * 10 + roiDigit);
    } else {
      display.clear();
    }
    delay(LOOP_DELAY_MS);
    return;
  }

  // 0 (absorbed echo / no reading) also counts as a detection
  int distanceCm = readDistanceCm();
  bool detected = distanceCm < threshold;

#if DEBUG_SERIAL
  if (millis() - lastDebugPrint >= 1000) {
    lastDebugPrint = millis();
    DEBUG_PRINT(F("sensor="));
    DEBUG_PRINT(currentSensor == SensorType::Tof ? F("ToF") : F("Ultrasonic"));
    DEBUG_PRINT(F(", distance="));
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
          saveState();
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

  // Display: distance peek overrides everything else for a short while,
  // then colon lit = sensor blocked, otherwise the running count.
  if (peekActive && now >= peekUntil) {
    peekActive = false;
    setLaser(false);
  }
  if (resetCountdownActive) {
    display.showNumberDec(resetCountdownValue);
  } else if (peekActive) {
    if (distanceCm == 0) {
      const uint8_t dashes[4] = {SEG_G, SEG_G, SEG_G, SEG_G};
      display.setSegments(dashes);
    } else {
      display.showNumberDec(distanceCm);
    }
  } else if (blocked) {
    display.showNumberDecEx(passCount, 0x40);
  } else {
    display.showNumberDec(passCount);   // raw count, or passCount / 2 for entries
  }

  delay(LOOP_DELAY_MS);
}
