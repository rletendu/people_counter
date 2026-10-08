#include "sensors.h"

#include <Wire.h>
#include <VL53L1X.h>

#include "buzzer.h"
#include "config.h"
#include "display_ui.h"
#include "storage.h"

VL53L1X tofSensor;                 // I2C: SDA = A4, SCL = A5
bool    tofReady = false;          // true if the VL53L1X answered during setup()

int threshold = 0;

void setLaser(bool on) {
  digitalWrite(LASER_PIN, on ? HIGH : LOW);
}

void applyRoiSize() {
  if (!tofReady) return;
  uint8_t width = ROI_WIDTH_FOR_LEVEL[static_cast<uint8_t>(currentRoiLevel)];
  tofSensor.setROISize(width, width);
}

void initSensors() {
  DEBUG_PRINTLN(F("Initializing sensors..."));
  Wire.begin();
  DEBUG_PRINTLN(F("I2C Wire.begin() done"));
  tofSensor.setTimeout(500);
  DEBUG_PRINTLN(F("Calling tofSensor.init()..."));
  tofReady = tofSensor.init();
  if (tofReady) {
    DEBUG_PRINTLN(F("tofSensor.init() succeeded"));
    tofSensor.setDistanceMode(VL53L1X::Long);
    DEBUG_PRINTLN(F("Distance mode set to Long"));
    tofSensor.setMeasurementTimingBudget(TOF_TIMING_BUDGET_US);
    DEBUG_PRINT(F("Timing budget set to "));
    DEBUG_PRINT(TOF_TIMING_BUDGET_US);
    DEBUG_PRINTLN(F(" us"));
    applyRoiSize();
    DEBUG_PRINT(F("ROI size applied for level "));
    DEBUG_PRINTLN(static_cast<int>(currentRoiLevel));
    tofSensor.startContinuous(TOF_TIMING_BUDGET_US / 1000);
    DEBUG_PRINTLN(F("VL53L1X ready and started continuous mode"));
  } else {
    DEBUG_PRINTLN(F("VL53L1X not found; ToF mode will stall in calibration"));
  }
}

// Returns distance in cm, or 0 if no echo (absorbed or out of range)
static int readDistanceUltrasonicCm() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 25000);   // timeout ~4 m
  return duration ? duration / 58 : 0;
}

// Returns distance in cm, or 0 if the ToF sensor isn't ready / has no valid reading
static int readDistanceTofCm() {
  if (!tofReady) {
    DEBUG_PRINTLN(F("ToF: not ready"));
    return 0;
  }
  tofSensor.read();
  if (tofSensor.timeoutOccurred()) {
    DEBUG_PRINTLN(F("ToF: timeout occurred"));
    return 0;
  }
  if (tofSensor.ranging_data.range_status != VL53L1X::RangeValid) {
    DEBUG_PRINT(F("ToF: invalid range_status="));
    DEBUG_PRINTLN(tofSensor.ranging_data.range_status);
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
  // Power on with an empty passage. Retry until a valid echo is received,
  // or give up after CALIBRATION_TIMEOUT_MS so a dead/miswired sensor
  // doesn't freeze the device (the menu stays reachable to switch sensors).
  DEBUG_PRINTLN(F("Calibration started; keep the passage empty."));
  setLaser(true);
  int d = 0;
  byte snakeStep = 0;
  unsigned long calibrationStart = millis();
  while (d == 0 || millis() - calibrationStart < CALIBRATION_MIN_MS) {
    if (d == 0 && millis() - calibrationStart >= CALIBRATION_TIMEOUT_MS) break;
    if (d == 0) {
      d = readDistanceCm();
      DEBUG_PRINT(F("Calibration reading="));
      DEBUG_PRINT(d);
      DEBUG_PRINTLN(F(" cm"));
    }
    showSnakeFrame(snakeStep++);
    delay(SNAKE_STEP_MS);
  }
  if (d == 0) {
    // Timed out: keep the previous threshold (0 at boot = nothing ever counted
    // until a successful recalibration via the menu or a reboot).
    DEBUG_PRINT(F("Calibration timed out; keeping threshold="));
    DEBUG_PRINTLN(threshold);
    playBlockedAlert();
  } else {
    threshold = d - MARGIN_CM;
    DEBUG_PRINT(F("Initial distance="));
    DEBUG_PRINT(d);
    DEBUG_PRINT(F(" cm; calibration threshold="));
    DEBUG_PRINT(threshold);
    DEBUG_PRINTLN(F(" cm"));
    TONE_IF_NOT_MUTED(BUZZER_PIN, CALIBRATION_BEEP_HZ, BEEP_DURATION_MS);
  }

  // Visual feedback: blink the reference distance for 2 seconds with rapid colon toggle
  unsigned long feedbackStart = millis();
  while (millis() - feedbackStart < CALIBRATION_FEEDBACK_MS) {
    bool colonOn = ((millis() - feedbackStart) / CALIBRATION_BLINK_MS) % 2 == 0;
    if (d == 0) {
      // No valid reading: show dashes (consistent with peek mode)
      const uint8_t dashes[4] = {SEG_G, SEG_G, SEG_G, SEG_G};
      display.setSegments(dashes);
    } else {
      // Show distance with alternating colon
      display.showNumberDecEx(d, colonOn ? 0x40 : 0x00);
    }
    delay(LOOP_DELAY_MS);  // 40ms refresh for smooth animation
  }

  setLaser(false);
}
