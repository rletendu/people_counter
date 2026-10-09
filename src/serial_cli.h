#pragma once

// Text command console on the USB serial port (9600 baud), always enabled.
// Type "help" in the serial monitor for the command list.
void initSerialCli();   // Serial.begin() + banner

// Reads pending serial input (non-blocking), runs complete command lines, and
// drives the "watch" stream and the laser auto-off. Called once per loop()
// iteration, after detection, with that iteration's measurement and state.
void updateSerialCli(int distanceCm, bool present, bool blocked);

bool isCliLaserOn();    // "laser on" in progress, so others don't switch it off
