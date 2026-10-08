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

// Ring buffer of slots spanning the whole 1 KB EEPROM: each save rotates to the
// next slot instead of rewriting the same cell, multiplying write endurance by
// EEPROM_SLOT_COUNT (~128x, so ~100k becomes ~12.8M writes).
const int EEPROM_SLOT_COUNT = 1024 / sizeof(CounterRecord);

int      currentSlot  = 0;
uint16_t lastSequence = 0;

unsigned int passCount        = 0;
SensorType   currentSensor    = SensorType::Ultrasonic;
RoiLevel     currentRoiLevel  = RoiLevel::Large;
bool         mutedState       = false;
uint16_t     manualThreshold  = 0;  // 0 = auto calibration mode

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
