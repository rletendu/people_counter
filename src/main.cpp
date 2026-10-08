#include <Arduino.h>

#include "buttons.h"
#include "buzzer.h"
#include "config.h"
#include "display_ui.h"
#include "sensors.h"
#include "serial_cli.h"
#include "storage.h"

// ---------- Detection state (local to the loop() state machine) ----------
bool          personPresent = false;
bool          blocked       = false;
byte          confirmCount  = 0;
unsigned long presenceStart = 0;
bool          clearing      = false;   // path reads clear, waiting out settings.clearHoldMs
unsigned long clearSince    = 0;
#if DEBUG_SERIAL
unsigned long lastDebugPrint = 0;
#endif

void setup() {
  initSerialCli();
  loadState();
  loadSettings();
  DEBUG_PRINT(F("People counter starting; saved count="));
  DEBUG_PRINTLN(passCount);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BTN_A_PIN, INPUT_PULLUP);
  pinMode(BTN_B_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LASER_PIN, OUTPUT);
  setLaser(false);
  display.setBrightness(settings.brightness);

  initSensors();

  // Check for mute toggle: both buttons pressed at startup
  // Give 1 second for the user to press buttons after power-on
  delay(1000);
  bool bothPressed = (!digitalRead(BTN_A_PIN) && !digitalRead(BTN_B_PIN));
  if (bothPressed) {
    mutedState = !mutedState;
    saveState();
    DEBUG_PRINT(F("Mute toggled, now: "));
    DEBUG_PRINTLN(mutedState ? F("ON") : F("OFF"));

    // Visual feedback: flash the display
    for (int i = 0; i < 3; i++) {
      display.showNumberDec(mutedState ? 8888 : 0);
      delay(200);
      display.clear();
      delay(200);
    }
    display.showNumberDec(passCount);
  }

  // Conditional calibration: skip if manual threshold is set
  if (manualThreshold == 0) {
    calibrate();  // Auto mode: calibrate normally
  } else {
    threshold = manualThreshold;  // Manual mode: use stored value
    DEBUG_PRINT(F("Manual threshold: "));
    DEBUG_PRINTLN(threshold);
    display.showNumberDec(threshold);  // Brief visual feedback
    delay(1000);
    display.showNumberDec(passCount);  // Return to count display
  }
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

  // NEW: Threshold menu display (4-digit blinking value)
  if (inThresholdMenu) {
    bool blinkOn = ((now - thresholdMenuBlinkStart) / MENU_BLINK_MS) % 2 == 0;
    if (blinkOn) {
      display.showNumberDecEx(menuThresholdValue, 0x00, true);  // Leading zeros
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

  // State change: an arrival needs settings.confirmReads consistent readings, a
  // departure needs settings.clearHoldMs of continuous clear, so an arm swing or a
  // flickering reading during one passage can't re-arm the counter.
  if (detected && !personPresent) {
    if (++confirmCount >= settings.confirmReads) {
      personPresent = true;
      confirmCount = 0;
      presenceStart = millis();
      DEBUG_PRINTLN(F("Presence changed: detected"));
      if (!blocked) {
        passCount++;                   // count on arrival, unless blocked
        saveState();
        DEBUG_PRINT(F("Passage counted; count="));
        DEBUG_PRINTLN(passCount);
        TONE_IF_NOT_MUTED(BUZZER_PIN, COUNT_BEEP_HZ, BEEP_DURATION_MS);
      } else {
        DEBUG_PRINTLN(F("Passage not counted; sensor is blocked"));
      }
    }
  } else if (!detected && personPresent) {
    if (!clearing) {
      clearing = true;
      clearSince = now;
    } else if (now - clearSince >= settings.clearHoldMs) {
      personPresent = false;
      clearing = false;
      blocked = false;                 // path is clear again, resume counting
      DEBUG_PRINTLN(F("Presence changed: clear; counting resumed"));
    }
  } else {
    confirmCount = 0;
    clearing = false;
  }

  // Someone or something stays too long: stop counting until it clears
  if (personPresent && !blocked && millis() - presenceStart > settings.maxPresenceMs) {
    blocked = true;
    DEBUG_PRINTLN(F("Sensor blocked: presence exceeded timeout"));
    playBlockedAlert();
  }

  updateSerialCli(distanceCm, personPresent, blocked);

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
