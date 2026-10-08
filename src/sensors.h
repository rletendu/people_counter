#pragma once

#include "config.h"

extern int threshold;
extern unsigned int usGlitchCount;   // isolated too-short ultrasonic readings ignored since boot   // distance (cm) below which someone is present, set by calibrate()

void initSensors();     // Wire.begin() + VL53L1X setup; always runs, regardless of currentSensor
void setLaser(bool on);
void applyRoiSize();
void applyTofBudget();  // re-applies settings.tofBudgetMs to a running VL53L1X
bool isTofReady();
int  readDistanceCm();  // dispatches to whichever sensor is currently selected; 0 = no reading
void calibrate();

// Commit helpers shared by the button menus and the serial console.
void commitSensorRoi(SensorType sensor, RoiLevel roi);
void commitManualThreshold(uint16_t value);   // 0 = auto calibration
bool recalibrateIfAuto();                     // false (and no-op) in manual threshold mode
