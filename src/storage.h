#pragma once

#include "config.h"

// Persisted state (mirrored to EEPROM by saveState()/loadState()).
extern unsigned int passCount;
extern SensorType   currentSensor;
extern RoiLevel     currentRoiLevel;
extern bool         mutedState;
extern uint16_t     manualThreshold;  // 0 = auto calibration, >0 = manual threshold in cm

void saveState();
void loadState();

// Runtime settings (see Settings in config.h), separate EEPROM block.
void loadSettings();
void saveSettings();
void resetSettings();   // back to the DEFAULT_* values, doesn't save

