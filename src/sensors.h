#pragma once

extern int threshold;   // distance (cm) below which someone is present, set by calibrate()

void initSensors();     // Wire.begin() + VL53L1X setup; always runs, regardless of currentSensor
void setLaser(bool on);
void applyRoiSize();
int  readDistanceCm();  // dispatches to whichever sensor is currently selected; 0 = no reading
void calibrate();
