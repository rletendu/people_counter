#pragma once

#include "config.h"

// Read by main.cpp::loop() to decide what to render.
extern bool          inMenu;
extern unsigned long menuBlinkStart;
extern SensorType    menuSensorSelection;
extern RoiLevel      menuRoiSelection;
extern bool          resetCountdownActive;
extern int           resetCountdownValue;
extern bool          peekActive;
extern unsigned long peekUntil;

// Reads both buttons, drives the reset combo / menu / distance-peek state
// machine. Called once per loop() iteration, before anything else.
void updateButtons();
