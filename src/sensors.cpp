#include "sensors.h"

#include <Wire.h>
#include <VL53L1X.h>

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
