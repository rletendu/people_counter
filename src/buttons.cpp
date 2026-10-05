#include "buttons.h"

#include "buzzer.h"
#include "sensors.h"
#include "storage.h"

bool          inMenu              = false;
unsigned long menuBlinkStart      = 0;
SensorType    menuSensorSelection = SensorType::Ultrasonic;  // toggled by button A
RoiLevel      menuRoiSelection    = RoiLevel::Large;         // cycled by button B
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
