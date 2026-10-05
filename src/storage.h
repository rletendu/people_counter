#pragma once

#include "config.h"

// Persisted state (mirrored to EEPROM by saveState()/loadState()).
extern unsigned int passCount;
extern SensorType   currentSensor;
extern RoiLevel     currentRoiLevel;

void saveState();
void loadState();
