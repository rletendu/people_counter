#pragma once

#include <Arduino.h>

#define DEBUG_SERIAL 0

#if DEBUG_SERIAL
#define DEBUG_PRINT(value) Serial.print(value)
#define DEBUG_PRINTLN(value) Serial.println(value)
#else
#define DEBUG_PRINT(value) ((void)0)
#define DEBUG_PRINTLN(value) ((void)0)
#endif

// ---------- Pins ----------
const int TRIG_PIN  = 9;
const int ECHO_PIN  = 10;
const int BTN_A_PIN = 3;          // to GND, uses INPUT_PULLUP; reset combo + menu + peek
const int BTN_B_PIN = 2;          // to GND, uses INPUT_PULLUP; reset combo + menu + peek
const int BUZZER_PIN = 6;
const int LASER_PIN = 7;          // aiming laser, via transistor (module draws more than a pin can source directly)
const int TM1637_CLK_PIN = 4;
const int TM1637_DIO_PIN = 5;
// VL53L1X ToF sensor: hardware I2C, fixed by the chip, not software-selectable.
// SDA = A4, SCL = A5. No const here since Wire.begin() takes no pin arguments.

// ---------- Settings ----------
const int           MARGIN_CM       = 20;      // how much closer than the empty wall = detection
const byte          CONFIRM_READS   = 2;       // consistent readings required to change state
const unsigned long MAX_PRESENCE_MS = 10000;   // after this, the sensor is considered blocked
const unsigned long LOOP_DELAY_MS   = 40;      // ~25 measurements per second
const unsigned long CALIBRATION_MIN_MS = 2000; // minimum snake animation duration
const unsigned long SNAKE_STEP_MS   = 80;      // snake animation speed
const unsigned int  CALIBRATION_BEEP_HZ = 1600;
const unsigned int  COUNT_BEEP_HZ   = 2200;
const unsigned int  BLOCKED_BEEP_FIRST_HZ  = 1100;
const unsigned int  BLOCKED_BEEP_SECOND_HZ = 700;
const unsigned long BEEP_DURATION_MS = 35;

// Buttons / menu timings
const unsigned long LONG_PRESS_MS    = 1200; // single button held alone -> open the menu
const unsigned long RESET_HOLD_MS    = 5000; // both buttons held together -> reset the counter
const unsigned long MENU_TIMEOUT_MS  = 4000; // inactivity in the menu -> confirm and exit
const unsigned long MENU_BLINK_MS    = 300;  // menu display blink period
const unsigned long PEEK_DURATION_MS = 1200; // short press (outside the menu) distance preview

// Extra sound feedback
const unsigned int  MENU_ENTER_BEEP_FIRST_HZ  = 1800;
const unsigned int  MENU_ENTER_BEEP_SECOND_HZ = 2400;
const unsigned int  MENU_EXIT_BEEP_HZ         = 1500;
const unsigned int  RESET_BEEP_FIRST_HZ       = 500;
const unsigned int  RESET_BEEP_SECOND_HZ      = 900;
const unsigned int  CLICK_BEEP_HZ             = 3000;
const unsigned long CLICK_DURATION_MS         = 15;

enum class SensorType : uint8_t { Ultrasonic = 0, Tof = 1 };

// ToF region-of-interest width, narrowing the detection cone from the default
// ~27 deg (Large) down to ~20 deg (Medium) or ~15 deg (Narrow, ST's recommended
// minimum) at the cost of less reflected light collected (shorter range / noisier).
enum class RoiLevel : uint8_t { Large = 0, Medium = 1, Narrow = 2 };
const uint8_t ROI_WIDTH_FOR_LEVEL[3] = {16, 8, 4};
