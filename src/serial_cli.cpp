#include "serial_cli.h"

#include <avr/pgmspace.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "display_ui.h"
#include "sensors.h"
#include "storage.h"

const unsigned long WATCH_PERIOD_MS = 250;

static char          lineBuf[32];
static uint8_t       lineLen      = 0;
static bool          lineOverflow = false;
static bool          watching     = false;
static unsigned long lastWatch    = 0;
static bool          laserOn      = false;
static unsigned long laserSince   = 0;

// Last measurement handed over by loop(), shown by "status".
static int  lastDistanceCm = 0;
static bool lastPresent    = false;
static bool lastBlocked    = false;

// What to do after a numeric setting changed.
enum : uint8_t { AFTER_NONE, AFTER_RECALIBRATE, AFTER_BRIGHTNESS, AFTER_TOF_BUDGET };

// Numeric settings, table-driven so "set", "status" and "help" share one list.
// Kept in flash; copied to RAM one entry at a time with memcpy_P.
struct NumParam {
  char     name[11];
  char     unit[3];
  void    *field;
  bool     wide;      // uint16_t field instead of uint8_t
  uint16_t minValue;
  uint16_t maxValue;
  uint8_t  after;
};

static const NumParam NUM_PARAMS[] PROGMEM = {
  {"margin",     "cm", &settings.marginCm,      false, MARGIN_MIN_CM,       MARGIN_MAX_CM,       AFTER_RECALIBRATE},
  {"confirm",    "",   &settings.confirmReads,  false, CONFIRM_MIN,         CONFIRM_MAX,         AFTER_NONE},
  {"clear",      "ms", &settings.clearHoldMs,   true,  CLEAR_HOLD_MIN_MS,   CLEAR_HOLD_MAX_MS,   AFTER_NONE},
  {"block",      "ms", &settings.maxPresenceMs, true,  MAX_PRESENCE_MIN_MS, MAX_PRESENCE_MAX_MS, AFTER_NONE},
  {"volume",     "%",  &settings.volumePercent, false, 0,                   VOLUME_MAX,          AFTER_NONE},
  {"brightness", "",   &settings.brightness,    false, 0,                   BRIGHTNESS_MAX,      AFTER_BRIGHTNESS},
  {"ping",       "ms", &settings.usPingMs,      false, US_PING_MIN_MS,      US_PING_MAX_MS,      AFTER_NONE},
  {"budget",     "ms", &settings.tofBudgetMs,   false, TOF_BUDGET_MIN_MS,   TOF_BUDGET_MAX_MS,   AFTER_TOF_BUDGET},
};
const uint8_t NUM_PARAM_COUNT = sizeof(NUM_PARAMS) / sizeof(NUM_PARAMS[0]);

static bool is(const char *token, PGM_P word) {
  return token != nullptr && strcmp_P(token, word) == 0;
}

// Whole-token integer within [minValue, maxValue]; prints the error itself.
static bool parseNumber(const char *token, long minValue, long maxValue, long &out) {
  if (token == nullptr) {
    Serial.println(F("missing value"));
    return false;
  }
  char *end;
  out = strtol(token, &end, 10);
  if (end == token || *end != '\0' || out < minValue || out > maxValue) {
    Serial.print(F("invalid value, expected "));
    Serial.print(minValue);
    Serial.print('-');
    Serial.println(maxValue);
    return false;
  }
  return true;
}

static uint16_t readParam(const NumParam &p) {
  return p.wide ? *static_cast<uint16_t *>(p.field) : *static_cast<uint8_t *>(p.field);
}

static void printParam(const NumParam &p) {
  Serial.print(p.name);
  Serial.print('=');
  Serial.print(readParam(p));
  if (p.unit[0]) {
    Serial.print(' ');
    Serial.print(p.unit);
  }
  Serial.println();
}

static const __FlashStringHelper *onOff(bool on) {
  return on ? F("on") : F("off");
}

static void printSensor() {
  Serial.print(F("sensor="));
  Serial.print(currentSensor == SensorType::Tof ? F("tof") : F("us"));
  Serial.print(F(" (ToF "));
  Serial.print(isTofReady() ? F("found") : F("missing"));
  Serial.println(')');
}

static void printRoi() {
  static const char ROI_NAMES[3][7] PROGMEM = {"large", "medium", "narrow"};
  Serial.print(F("roi="));
  Serial.println(reinterpret_cast<const __FlashStringHelper *>(
      ROI_NAMES[static_cast<uint8_t>(currentRoiLevel)]));
}

static void printThreshold() {
  Serial.print(F("threshold="));
  Serial.print(threshold);
  Serial.println(manualThreshold != 0 ? F(" cm (manual)") : F(" cm (auto)"));
}

static void printMuted() {
  Serial.print(F("mute="));
  Serial.println(onOff(mutedState));
}

static void printCount() {
  Serial.print(F("count="));
  Serial.println(passCount);
}

static void printStatus() {
  printSensor();
  printRoi();
  printThreshold();
  printMuted();
  for (uint8_t i = 0; i < NUM_PARAM_COUNT; i++) {
    NumParam p;
    memcpy_P(&p, &NUM_PARAMS[i], sizeof(p));
    printParam(p);
  }
  printCount();
  Serial.print(F("distance="));
  Serial.print(lastDistanceCm);
  Serial.print(F(" cm present="));
  Serial.print(lastPresent);
  Serial.print(F(" blocked="));
  Serial.println(lastBlocked);
  Serial.print(F("glitches="));
  Serial.println(usGlitchCount);
}

static void printHelp() {
  Serial.println(F(
      "Commands:\n"
      "  status             state and settings\n"
      "  set <param> <val>  change a setting (saved)\n"
      "  cal                recalibrate (auto threshold only)\n"
      "  count <n> | reset  set / clear the counter\n"
      "  watch              live readings (on/off)\n"
      "  laser on|off       aiming laser (auto off after 30 s)\n"
      "  defaults           restore default settings (count kept)\n"
      "  reboot             restart the device (recalibrates)\n"
      "Parameters:\n"
      "  sensor us|tof\n"
      "  roi large|medium|narrow\n"
      "  threshold auto|10-400 cm\n"
      "  mute on|off"));
  for (uint8_t i = 0; i < NUM_PARAM_COUNT; i++) {
    NumParam p;
    memcpy_P(&p, &NUM_PARAMS[i], sizeof(p));
    Serial.print(F("  "));
    Serial.print(p.name);
    Serial.print(' ');
    Serial.print(p.minValue);
    Serial.print('-');
    Serial.print(p.maxValue);
    Serial.print(' ');
    Serial.println(p.unit);
  }
}

// Software restart: jump back to the reset vector. Not a watchdog reset on
// purpose: the old Nano bootloader (nano_old) leaves the watchdog running after
// one and reboot-loops forever. A jump doesn't reset the hardware, so every
// peripheral that keeps interrupts or outputs alive is shut down first; the C
// runtime, init() and setup() redo the rest. Nothing to save first: the count
// and settings are already written to EEPROM whenever they change.
static void reboot() {
  Serial.println(F("rebooting..."));
  Serial.flush();
  setLaser(false);
  noTone(BUZZER_PIN);   // Timer2
  Serial.end();         // UART + its interrupts
  TWCR = 0;             // TWI (VL53L1X I2C), Wire.begin() sets it up again
  cli();
  asm volatile("jmp 0");
}

static void setLaserFromCli(bool on) {
  laserOn = on;
  laserSince = millis();
  setLaser(on);
}

static void applyAfter(uint8_t after) {
  switch (after) {
    case AFTER_RECALIBRATE: recalibrateIfAuto(); break;
    case AFTER_BRIGHTNESS:  display.setBrightness(settings.brightness); break;
    case AFTER_TOF_BUDGET:  applyTofBudget(); break;
  }
}

static void handleSet(const char *name, const char *value) {
  if (is(name, PSTR("sensor"))) {
    if (is(value, PSTR("us"))) {
      commitSensorRoi(SensorType::Ultrasonic, currentRoiLevel);
    } else if (is(value, PSTR("tof"))) {
      commitSensorRoi(SensorType::Tof, currentRoiLevel);
    } else {
      Serial.println(F("expected: us or tof"));
      return;
    }
    printSensor();
    printThreshold();
    return;
  }
  if (is(name, PSTR("roi"))) {
    RoiLevel roi;
    if (is(value, PSTR("large")) || is(value, PSTR("1"))) {
      roi = RoiLevel::Large;
    } else if (is(value, PSTR("medium")) || is(value, PSTR("2"))) {
      roi = RoiLevel::Medium;
    } else if (is(value, PSTR("narrow")) || is(value, PSTR("3"))) {
      roi = RoiLevel::Narrow;
    } else {
      Serial.println(F("expected: large, medium or narrow"));
      return;
    }
    commitSensorRoi(currentSensor, roi);
    printRoi();
    printThreshold();
    return;
  }
  if (is(name, PSTR("threshold"))) {
    long cm = 0;
    if (!is(value, PSTR("auto")) &&
        !parseNumber(value, MANUAL_THRESHOLD_MIN_CM, MANUAL_THRESHOLD_MAX_CM, cm)) {
      return;
    }
    commitManualThreshold(static_cast<uint16_t>(cm));
    printThreshold();
    return;
  }
  if (is(name, PSTR("mute"))) {
    if (is(value, PSTR("on"))) {
      mutedState = true;
    } else if (is(value, PSTR("off"))) {
      mutedState = false;
    } else {
      Serial.println(F("expected: on or off"));
      return;
    }
    saveState();
    printMuted();
    return;
  }
  for (uint8_t i = 0; i < NUM_PARAM_COUNT; i++) {
    NumParam p;
    memcpy_P(&p, &NUM_PARAMS[i], sizeof(p));
    if (name == nullptr || strcmp(name, p.name) != 0) continue;
    long v;
    if (!parseNumber(value, p.minValue, p.maxValue, v)) return;
    if (p.wide) {
      *static_cast<uint16_t *>(p.field) = static_cast<uint16_t>(v);
    } else {
      *static_cast<uint8_t *>(p.field) = static_cast<uint8_t>(v);
    }
    saveSettings();
    applyAfter(p.after);
    printParam(p);
    if (p.after == AFTER_RECALIBRATE) printThreshold();
    return;
  }
  Serial.println(F("unknown parameter (help)"));
}

static void runCommand(char *line) {
  for (char *c = line; *c; c++) *c = tolower(*c);
  char *cmd = strtok(line, " \t");
  if (cmd == nullptr) return;
  char *arg1 = strtok(nullptr, " \t");
  char *arg2 = strtok(nullptr, " \t");

  // Any command stops the watch stream; "watch" itself toggles it.
  bool wasWatching = watching;
  watching = false;

  if (is(cmd, PSTR("help")) || is(cmd, PSTR("?"))) {
    printHelp();
  } else if (is(cmd, PSTR("status"))) {
    printStatus();
  } else if (is(cmd, PSTR("set"))) {
    handleSet(arg1, arg2);
  } else if (is(cmd, PSTR("cal"))) {
    if (recalibrateIfAuto()) {
      printThreshold();
    } else {
      Serial.println(F("manual threshold: no calibration (set threshold auto)"));
    }
  } else if (is(cmd, PSTR("count"))) {
    long n;
    if (parseNumber(arg1, 0, 65535, n)) {
      passCount = static_cast<unsigned int>(n);
      saveState();
      printCount();
    }
  } else if (is(cmd, PSTR("reset"))) {
    passCount = 0;
    saveState();
    printCount();
  } else if (is(cmd, PSTR("watch"))) {
    watching = !wasWatching;
    lastWatch = 0;
    if (!watching) Serial.println(F("watch off"));
  } else if (is(cmd, PSTR("laser"))) {
    if (is(arg1, PSTR("on"))) {
      setLaserFromCli(true);
    } else if (is(arg1, PSTR("off"))) {
      setLaserFromCli(false);
    } else {
      Serial.println(F("expected: on or off"));
      return;
    }
    Serial.print(F("laser="));
    Serial.println(onOff(laserOn));
  } else if (is(cmd, PSTR("defaults"))) {
    resetSettings();
    saveSettings();
    display.setBrightness(settings.brightness);
    applyTofBudget();
    Serial.println(F("default settings restored"));
    recalibrateIfAuto();   // the margin may have changed
    printThreshold();
  } else if (is(cmd, PSTR("reboot"))) {
    reboot();
  } else {
    Serial.println(F("unknown command (help)"));
  }
}

void initSerialCli() {
  Serial.begin(9600);
  Serial.println(F("People counter - type 'help'"));
}

void updateSerialCli(int distanceCm, bool present, bool blocked) {
  lastDistanceCm = distanceCm;
  lastPresent = present;
  lastBlocked = blocked;

  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (lineOverflow) {
        Serial.println(F("line too long"));
      } else if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        runCommand(lineBuf);
      }
      lineLen = 0;
      lineOverflow = false;
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    } else {
      lineOverflow = true;
    }
  }

  unsigned long now = millis();
  if (watching && (lastWatch == 0 || now - lastWatch >= WATCH_PERIOD_MS)) {
    lastWatch = now;
    Serial.print(F("d="));
    Serial.print(distanceCm);
    Serial.print(F(" threshold="));
    Serial.print(threshold);
    Serial.print(F(" present="));
    Serial.print(present);
    Serial.print(F(" blocked="));
    Serial.print(blocked);
    Serial.print(F(" n="));
    Serial.println(passCount);
  }

  // The aiming laser must never stay on: forced off after LASER_MAX_ON_MS.
  if (laserOn && now - laserSince >= LASER_MAX_ON_MS) {
    setLaserFromCli(false);
    Serial.println(F("laser=off (auto)"));
  }
}
