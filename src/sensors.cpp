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

void applyTofBudget() {
  if (!tofReady) return;
  tofSensor.stopContinuous();
  tofSensor.setMeasurementTimingBudget((uint32_t)settings.tofBudgetMs * 1000);
  tofSensor.startContinuous(settings.tofBudgetMs);
}

bool isTofReady() {
  return tofReady;
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
    tofSensor.setMeasurementTimingBudget((uint32_t)settings.tofBudgetMs * 1000);
    DEBUG_PRINT(F("Timing budget set to "));
    DEBUG_PRINT(settings.tofBudgetMs);
    DEBUG_PRINTLN(F(" ms"));
    applyRoiSize();
    DEBUG_PRINT(F("ROI size applied for level "));
    DEBUG_PRINTLN(static_cast<int>(currentRoiLevel));
    tofSensor.startContinuous(settings.tofBudgetMs);
    DEBUG_PRINTLN(F("VL53L1X ready and started continuous mode"));
  } else {
    DEBUG_PRINTLN(F("VL53L1X not found; ToF mode will stall in calibration"));
  }
}

unsigned int usGlitchCount = 0;

// Returns distance in cm, or 0 if no echo (absorbed or out of range).
// Pings are spaced by at least settings.usPingMs, and an isolated reading
// under US_MIN_VALID_CM is replaced by the previous one (which may be 0, so
// "no reading = presence" still holds); a second short reading in a row is kept.
static int readDistanceUltrasonicCm() {
  static unsigned long lastPingMs = 0;
  static int  lastAccepted = 0;
  static bool lastWasShort = false;

  unsigned long sincePing = millis() - lastPingMs;
  if (sincePing < settings.usPingMs) delay(settings.usPingMs - sincePing);
  lastPingMs = millis();

  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 25000);   // timeout ~4 m
  int d = duration ? duration / 58 : 0;

  bool isShort = (d > 0 && d < US_MIN_VALID_CM);
  if (isShort && !lastWasShort) {
    lastWasShort = true;
    usGlitchCount++;
    DEBUG_PRINT(F("US: short reading ignored, d="));
    DEBUG_PRINTLN(d);
    return lastAccepted;
  }
  lastWasShort = isShort;
  lastAccepted = d;
  return d;
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
  // Power on with an empty passage. Collects CALIBRATION_SAMPLES valid
  // readings and uses their median (a single glitch can't set the threshold),
  // or gives up after CALIBRATION_TIMEOUT_MS so a dead/miswired sensor
  // doesn't freeze the device (the menu stays reachable to switch sensors).
  DEBUG_PRINTLN(F("Calibration started; keep the passage empty."));
  setLaser(true);
  int samples[CALIBRATION_SAMPLES];
  byte sampleCount = 0;
  byte snakeStep = 0;
  unsigned long calibrationStart = millis();
  while (sampleCount < CALIBRATION_SAMPLES || millis() - calibrationStart < CALIBRATION_MIN_MS) {
    if (sampleCount < CALIBRATION_SAMPLES && millis() - calibrationStart >= CALIBRATION_TIMEOUT_MS) break;
    if (sampleCount < CALIBRATION_SAMPLES) {
      int reading = readDistanceCm();
      DEBUG_PRINT(F("Calibration reading="));
      DEBUG_PRINT(reading);
      DEBUG_PRINTLN(F(" cm"));
      // Too-short ultrasonic readings are glitches, never the opposite wall.
      bool valid = reading > 0 &&
                   (currentSensor == SensorType::Tof || reading >= US_MIN_VALID_CM);
      if (valid) samples[sampleCount++] = reading;
    }
    showSnakeFrame(snakeStep++);
    delay(SNAKE_STEP_MS);
  }

  // Median of the valid readings (all of them if the timeout cut it short).
  int d = 0;
  if (sampleCount > 0) {
    for (byte i = 1; i < sampleCount; i++) {
      int v = samples[i];
      byte j = i;
      for (; j > 0 && samples[j - 1] > v; j--) samples[j] = samples[j - 1];
      samples[j] = v;
    }
    d = samples[sampleCount / 2];
  }

  if (d <= settings.marginCm) {
    // No reading, or a wall closer than the margin (threshold would be <= 0 and
    // nothing could ever be detected): keep the previous threshold (0 at boot =
    // nothing counted until a successful recalibration via the menu or a reboot).
    DEBUG_PRINT(F("Calibration failed; keeping threshold="));
    DEBUG_PRINTLN(threshold);
    playBlockedAlert();
  } else {
    threshold = d - settings.marginCm;
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

// Shared by the button menu and the serial console: apply + persist a sensor /
// ROI choice, recalibrating when the active measurement actually changed.
void commitSensorRoi(SensorType sensor, RoiLevel roi) {
  bool sensorChanged = (sensor != currentSensor);
  bool roiChanged = (roi != currentRoiLevel);
  currentSensor = sensor;
  currentRoiLevel = roi;
  if (roiChanged) applyRoiSize();
  saveState();
  DEBUG_PRINT(F("Sensor/ROI committed; sensor="));
  DEBUG_PRINT(currentSensor == SensorType::Tof ? F("ToF") : F("Ultrasonic"));
  DEBUG_PRINT(F(", roiWidth="));
  DEBUG_PRINTLN(ROI_WIDTH_FOR_LEVEL[static_cast<uint8_t>(currentRoiLevel)]);
  if (sensorChanged || (roiChanged && currentSensor == SensorType::Tof)) {
    delay(LOOP_DELAY_MS);
    recalibrateIfAuto();
  }
}

// 0 = back to auto calibration (calibrates if we were in manual mode),
// anything else = fixed threshold in cm.
void commitManualThreshold(uint16_t value) {
  uint16_t oldValue = manualThreshold;
  manualThreshold = value;
  saveState();
  if (oldValue != 0 && manualThreshold == 0) {
    delay(LOOP_DELAY_MS);
    calibrate();
  } else if (manualThreshold != 0) {
    threshold = manualThreshold;
    DEBUG_PRINT(F("Threshold set to manual value: "));
    DEBUG_PRINTLN(threshold);
  }
}

// A manual threshold never gets overwritten by a calibration.
bool recalibrateIfAuto() {
  if (manualThreshold != 0) return false;
  calibrate();
  return true;
}
