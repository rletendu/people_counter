#include "storage.h"

#include <EEPROM.h>

const uint16_t EEPROM_MAGIC = 0x5048;  // Bumped from 0x5047 for manualThreshold field

struct CounterRecord {
  uint16_t magic;
  uint16_t count;
  uint8_t  sensorMode;   // SensorType
  uint8_t  roiLevel;     // RoiLevel
  uint16_t sequence;     // bumped on every write, to find the newest slot at boot
  uint8_t  muted;        // 0 = unmuted, 1 = muted
  uint16_t manualThreshold;  // 0 = auto calibration, >0 = manual threshold in cm
  uint8_t  checksum;     // catches a slot left half-written by a power loss
};

// Settings live in a fixed block at the very end of the EEPROM: they're only
// written on an explicit serial "set", so they need no wear levelling.
const uint16_t SETTINGS_MAGIC    = 0x5304;  // bumped for buttonLock
const int      SETTINGS_RESERVED = 24;
const int      SETTINGS_ADDR     = 1024 - SETTINGS_RESERVED;
static_assert(sizeof(Settings) <= SETTINGS_RESERVED, "Settings outgrew its EEPROM block");

// Ring buffer of slots spanning the rest of the 1 KB EEPROM: each save rotates
// to the next slot instead of rewriting the same cell, multiplying write
// endurance by EEPROM_SLOT_COUNT (~83x, so ~100k becomes ~8.3M writes).
const int EEPROM_SLOT_COUNT = SETTINGS_ADDR / sizeof(CounterRecord);

int      currentSlot  = 0;
uint16_t lastSequence = 0;

unsigned int passCount        = 0;
SensorType   currentSensor    = SensorType::Ultrasonic;
RoiLevel     currentRoiLevel  = RoiLevel::Large;
bool         mutedState       = false;
uint16_t     manualThreshold  = 0;  // 0 = auto calibration mode
Settings     settings;

uint8_t computeChecksum(const CounterRecord &record) {
  uint8_t sum = 0;
  sum += record.magic & 0xFF;
  sum += record.magic >> 8;
  sum += record.count & 0xFF;
  sum += record.count >> 8;
  sum += record.sensorMode;
  sum += record.roiLevel;
  sum += record.sequence & 0xFF;
  sum += record.sequence >> 8;
  sum += record.muted;
  sum += record.manualThreshold & 0xFF;
  sum += record.manualThreshold >> 8;
  return sum;
}

void saveState() {
  currentSlot = (currentSlot + 1) % EEPROM_SLOT_COUNT;
  lastSequence++;
  CounterRecord record = {EEPROM_MAGIC, static_cast<uint16_t>(passCount),
                           static_cast<uint8_t>(currentSensor),
                           static_cast<uint8_t>(currentRoiLevel), lastSequence,
                           static_cast<uint8_t>(mutedState ? 1 : 0), manualThreshold, 0};
  record.checksum = computeChecksum(record);
  EEPROM.put(currentSlot * sizeof(CounterRecord), record);
}

// Scans every slot of the ring buffer and keeps the newest valid one (highest
// sequence number, with wraparound handled via signed 16-bit comparison).
void loadState() {
  bool found = false;
  CounterRecord best;
  int bestSlot = 0;
  for (int slot = 0; slot < EEPROM_SLOT_COUNT; slot++) {
    CounterRecord record;
    EEPROM.get(slot * sizeof(CounterRecord), record);
    if (record.magic != EEPROM_MAGIC || record.checksum != computeChecksum(record)) {
      continue;
    }
    if (!found || (int16_t)(record.sequence - best.sequence) > 0) {
      best = record;
      bestSlot = slot;
      found = true;
    }
  }
  if (found) {
    passCount = best.count;
    currentSensor = (best.sensorMode == static_cast<uint8_t>(SensorType::Tof))
                        ? SensorType::Tof
                        : SensorType::Ultrasonic;
    currentRoiLevel = (best.roiLevel <= static_cast<uint8_t>(RoiLevel::Narrow))
                          ? static_cast<RoiLevel>(best.roiLevel)
                          : RoiLevel::Large;
    mutedState = (best.muted != 0);
    manualThreshold = best.manualThreshold;
    currentSlot = bestSlot;
    lastSequence = best.sequence;
  } else {
    passCount = 0;
    currentSensor = SensorType::Ultrasonic;
    currentRoiLevel = RoiLevel::Large;
    mutedState = false;
    manualThreshold = 0;  // Default to auto calibration mode
    currentSlot = -1;    // so the first saveState() below lands on slot 0
    lastSequence = 0;
    saveState();
  }
}

static uint8_t settingsChecksum(const Settings &s) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&s);
  uint8_t sum = 0;
  for (size_t i = 0; i < offsetof(Settings, checksum); i++) sum += bytes[i];
  return sum;
}

void resetSettings() {
  settings.marginCm      = DEFAULT_MARGIN_CM;
  settings.confirmReads  = DEFAULT_CONFIRM_READS;
  settings.clearHoldMs   = DEFAULT_CLEAR_HOLD_MS;
  settings.maxPresenceMs = DEFAULT_MAX_PRESENCE_MS;
  settings.volumePercent = DEFAULT_VOLUME_PERCENT;
  settings.brightness    = DEFAULT_BRIGHTNESS;
  settings.tofBudgetMs   = DEFAULT_TOF_BUDGET_MS;
  settings.usPingMs      = DEFAULT_US_PING_MS;
  settings.laserFlash    = DEFAULT_LASER_FLASH;
  settings.buttonLock    = DEFAULT_BUTTON_LOCK;
}

void saveSettings() {
  settings.magic = SETTINGS_MAGIC;
  settings.checksum = settingsChecksum(settings);
  EEPROM.put(SETTINGS_ADDR, settings);   // EEPROM.put only rewrites bytes that changed
}

// Fresh chip or corrupted block -> defaults. Every field is re-clamped anyway,
// so a valid-but-out-of-range value can't put the device in a weird state.
void loadSettings() {
  EEPROM.get(SETTINGS_ADDR, settings);
  if (settings.magic != SETTINGS_MAGIC || settings.checksum != settingsChecksum(settings)) {
    resetSettings();
    saveSettings();
    return;
  }
  settings.marginCm      = constrain(settings.marginCm, MARGIN_MIN_CM, MARGIN_MAX_CM);
  settings.confirmReads  = constrain(settings.confirmReads, CONFIRM_MIN, CONFIRM_MAX);
  settings.clearHoldMs   = constrain(settings.clearHoldMs, CLEAR_HOLD_MIN_MS, CLEAR_HOLD_MAX_MS);
  settings.maxPresenceMs = constrain(settings.maxPresenceMs, MAX_PRESENCE_MIN_MS, MAX_PRESENCE_MAX_MS);
  settings.volumePercent = min(settings.volumePercent, VOLUME_MAX);
  settings.brightness    = min(settings.brightness, BRIGHTNESS_MAX);
  settings.tofBudgetMs   = constrain(settings.tofBudgetMs, TOF_BUDGET_MIN_MS, TOF_BUDGET_MAX_MS);
  settings.usPingMs      = constrain(settings.usPingMs, US_PING_MIN_MS, US_PING_MAX_MS);
  settings.laserFlash    = settings.laserFlash ? 1 : 0;
  settings.buttonLock    = settings.buttonLock ? 1 : 0;
}
