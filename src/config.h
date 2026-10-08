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

// Runtime-tunable settings, adjustable from the serial console and persisted
// in their own EEPROM block (see storage.cpp). Defaults/bounds are below.
struct Settings {
  uint16_t magic;
  uint8_t  marginCm;        // how much closer than the empty wall = detection
  uint8_t  confirmReads;    // consistent readings required to confirm an arrival
  uint16_t clearHoldMs;     // continuous clear time before the next passage can count
  uint16_t maxPresenceMs;   // after this, the sensor is considered blocked
  uint8_t  volumePercent;   // scales beep duration (perceived volume), 0 = silent
  uint8_t  brightness;      // TM1637 brightness, 0-7
  uint8_t  tofBudgetMs;     // VL53L1X measurement timing budget
  uint8_t  usPingMs;        // minimum time between two ultrasonic pings
  uint8_t  laserFlash;      // 1 = brief laser flash on every counted passage
  uint8_t  checksum;
};

// Defined in storage.cpp
extern bool     mutedState;
extern Settings settings;

// Conditional tone() wrapper: only emit sound if not muted, with volume scaling
// Lower volume = shorter beeps = perceived quieter (not true volume reduction)
#define TONE_IF_NOT_MUTED(pin, freq, dur) \
  do { \
    if (!mutedState) { \
      unsigned long scaledDur = ((unsigned long)(dur) * settings.volumePercent) / 100; \
      if (scaledDur > 0) tone(pin, freq, scaledDur); \
    } \
  } while(0)

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

// ---------- Runtime settings: defaults and bounds (see Settings above) ----------
// VL53L1X timing budget: measurement integration time. Long distance mode
// minimum is 33 ms, 100-140 ms recommended for reliable readings; higher
// values improve range and SNR but slow down each loop() iteration.
const uint8_t  DEFAULT_TOF_BUDGET_MS = 50;     const uint8_t  TOF_BUDGET_MIN_MS = 33;    const uint8_t  TOF_BUDGET_MAX_MS = 200;
const uint8_t  DEFAULT_MARGIN_CM     = 20;     const uint8_t  MARGIN_MIN_CM     = 5;     const uint8_t  MARGIN_MAX_CM     = 100;
const uint8_t  DEFAULT_CONFIRM_READS = 2;      const uint8_t  CONFIRM_MIN       = 1;     const uint8_t  CONFIRM_MAX       = 10;
const uint16_t DEFAULT_CLEAR_HOLD_MS = 500;    const uint16_t CLEAR_HOLD_MIN_MS = 100;   const uint16_t CLEAR_HOLD_MAX_MS = 5000;
const uint16_t DEFAULT_MAX_PRESENCE_MS = 10000; const uint16_t MAX_PRESENCE_MIN_MS = 1000; const uint16_t MAX_PRESENCE_MAX_MS = 60000;
const uint8_t  DEFAULT_VOLUME_PERCENT = 50;    const uint8_t  VOLUME_MAX        = 100;
const uint8_t  DEFAULT_BRIGHTNESS    = 5;      const uint8_t  BRIGHTNESS_MAX    = 7;
// HC-SR04: the datasheet asks for >= 60 ms between pings, otherwise the
// previous ping's reverberation comes back as a bogus very short echo; a bit
// more helps in a small room with hard walls.
const uint8_t  DEFAULT_US_PING_MS    = 70;     const uint8_t  US_PING_MIN_MS    = 60;    const uint8_t  US_PING_MAX_MS    = 150;
const uint8_t  DEFAULT_LASER_FLASH   = 0;      // off: it points the laser at whoever walks by
const uint16_t MANUAL_THRESHOLD_MIN_CM = 10;   const uint16_t MANUAL_THRESHOLD_MAX_CM = 400;

// ---------- Fixed settings ----------
// An isolated ultrasonic reading shorter than this is treated as a glitch
// (two in a row are accepted: something really is right against the sensor).
const int           US_MIN_VALID_CM = 10;
const byte          CALIBRATION_SAMPLES = 5;   // median of this many valid readings
const unsigned long LASER_MAX_ON_MS = 30000;   // serial "laser on" auto-off
// Counted-passage laser flash: hits the person walking by, so it must stay far
// below the ~0.25 s blink reflex that class 2 laser safety relies on.
const unsigned long LASER_FLASH_MS  = 80;
const unsigned long LOOP_DELAY_MS   = 40;      // ~25 measurements per second
const unsigned long CALIBRATION_MIN_MS = 2000; // minimum snake animation duration
const unsigned long CALIBRATION_TIMEOUT_MS = 10000; // give up if no valid reading by then
const unsigned long SNAKE_STEP_MS   = 80;      // snake animation speed
const unsigned int  CALIBRATION_BEEP_HZ = 1600;
const unsigned int  COUNT_BEEP_HZ   = 2200;
const unsigned int  BLOCKED_BEEP_FIRST_HZ  = 1100;
const unsigned int  BLOCKED_BEEP_SECOND_HZ = 700;
const unsigned long BEEP_DURATION_MS = 35;
const unsigned long CALIBRATION_FEEDBACK_MS = 2000;  // post-calibration feedback duration
const unsigned long CALIBRATION_BLINK_MS = 100;      // colon toggle interval (rapid)

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
