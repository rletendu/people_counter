#pragma once

#include "config.h"

// Read by main.cpp::loop() to decide what to render.
extern bool          inMenu;
extern bool          inThresholdMenu;           // NEW: threshold menu state
extern unsigned long menuBlinkStart;
extern unsigned long thresholdMenuBlinkStart;   // NEW: threshold menu blink timing
extern SensorType    menuSensorSelection;
extern RoiLevel      menuRoiSelection;
extern uint16_t      menuThresholdValue;        // NEW: threshold value being edited (0-200 cm)
extern bool          resetCountdownActive;
extern int           resetCountdownValue;
extern bool          peekActive;
extern unsigned long peekUntil;

// Reads both buttons, drives the reset combo / menu / distance-peek state
// machine. Called once per loop() iteration, before anything else.
void updateButtons();
