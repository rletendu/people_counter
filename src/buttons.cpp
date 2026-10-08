#include "buttons.h"

#include "buzzer.h"
#include "sensors.h"
#include "storage.h"

bool          inMenu              = false;
bool          inThresholdMenu     = false;   // NEW: threshold menu state
unsigned long menuBlinkStart      = 0;
unsigned long thresholdMenuBlinkStart = 0;   // NEW: threshold menu blink timing
SensorType    menuSensorSelection = SensorType::Ultrasonic;  // toggled by button A
RoiLevel      menuRoiSelection    = RoiLevel::Large;         // cycled by button B
uint16_t      menuThresholdValue  = 0;       // NEW: threshold value being edited (0-200 cm)
bool          resetCountdownActive = false;  // both buttons held, counting down to reset
int           resetCountdownValue  = 0;      // seconds remaining, shown on the display
bool          peekActive = false;
unsigned long peekUntil  = 0;

// Button / menu state private to this file.
static unsigned long pressStartA = 0;
static unsigned long pressStartB = 0;
static bool          wasAPressed = false;
static bool          wasBPressed = false;
static bool          resetArmed  = false;
static bool          comboHandled = false;    // true once a combo (reset) fired, until both released

static int           openerPin           = -1;
static bool          menuIgnoreRelease   = false;
static unsigned long menuLastActivity    = 0;

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

  // NEW: Threshold menu handler (long press on button B)
  if (inThresholdMenu) {
    bool ignoredReleaseB = false;
    if (menuIgnoreRelease && openerPin == BTN_B_PIN && releasedB) {
      menuIgnoreRelease = false;
      ignoredReleaseB = true;
    }

    // Button A = decrement by 5 cm (wrap 0 -> 200)
    if (releasedA) {
      menuThresholdValue = (menuThresholdValue >= 5)
                          ? menuThresholdValue - 5
                          : 200;  // Wrap 0->200
      menuLastActivity = now;
      playClickSound();
    }
    // Button B = increment by 5 cm (wrap 200 -> 0)
    if (!ignoredReleaseB && releasedB) {
      menuThresholdValue = (menuThresholdValue < 200)
                          ? menuThresholdValue + 5
                          : 0;  // Wrap 200->0
      menuLastActivity = now;
      playClickSound();
    }

    // Timeout: commit and exit
    if (now - menuLastActivity >= MENU_TIMEOUT_MS) {
      inThresholdMenu = false;
      openerPin = -1;
      menuIgnoreRelease = false;
      DEBUG_PRINT(F("Threshold menu closed; manualThreshold="));
      DEBUG_PRINTLN(menuThresholdValue);
      playMenuExitSound();
      commitManualThreshold(menuThresholdValue);
    }
    return;
  }

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
      inMenu = false;
      openerPin = -1;
      menuIgnoreRelease = false;
      DEBUG_PRINTLN(F("Sensor/ROI menu closed"));
      playMenuExitSound();
      commitSensorRoi(menuSensorSelection, menuRoiSelection);
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
    // Button A long press -> Sensor/ROI menu
    if (onlyA && now - pressStartA >= LONG_PRESS_MS) {
      inMenu = true;
      openerPin = BTN_A_PIN;
      menuIgnoreRelease = true;
      menuSensorSelection = currentSensor;
      menuRoiSelection = currentRoiLevel;
      menuLastActivity = now;
      menuBlinkStart = now;
      DEBUG_PRINTLN(F("Sensor/ROI menu opened"));
      playMenuEnterSound();
      return;
    }
    // Button B long press -> Threshold menu
    else if (onlyB && now - pressStartB >= LONG_PRESS_MS) {
      inThresholdMenu = true;
      openerPin = BTN_B_PIN;
      menuIgnoreRelease = true;
      // Initialize with current manual threshold, or calibrated threshold rounded to 5cm
      if (manualThreshold != 0) {
        menuThresholdValue = manualThreshold;
      } else {
        menuThresholdValue = (threshold / 5) * 5;  // Round to nearest 5 cm
      }
      if (menuThresholdValue > 200) menuThresholdValue = 200;  // Clamp to max
      menuLastActivity = now;
      thresholdMenuBlinkStart = now;
      DEBUG_PRINTLN(F("Threshold menu opened"));
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
