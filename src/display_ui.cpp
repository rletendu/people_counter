#include "display_ui.h"

#include "config.h"

TM1637Display display(TM1637_CLK_PIN, TM1637_DIO_PIN);

// Snake animation: a 3-segment tail runs around the outline of the 4 digits.
// Path (digit, segment bit): top a, right b/c, bottom d (backwards), left e/f.
void showSnakeFrame(byte step) {
  const byte PATH_LEN = 12;
  const byte SNAKE_LEN = 3;
  const byte pathDigit[PATH_LEN] = {0, 1, 2, 3, 3, 3, 3, 2, 1, 0, 0, 0};
  const uint8_t pathSeg[PATH_LEN] = {0x01, 0x01, 0x01, 0x01, 0x02, 0x04,
                                     0x08, 0x08, 0x08, 0x08, 0x10, 0x20};
  uint8_t segs[4] = {0, 0, 0, 0};
  for (byte i = 0; i < SNAKE_LEN; i++) {
    byte p = (step + PATH_LEN - i) % PATH_LEN;
    segs[pathDigit[p]] |= pathSeg[p];
  }
  display.setSegments(segs);
}

void showLockedText() {
  const uint8_t segs[4] = {SEG_D | SEG_E | SEG_F,                          // L
                           SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,  // O
                           SEG_A | SEG_D | SEG_E | SEG_F,                  // C
                           0};
  display.setSegments(segs);
}

void showUnlockedText() {
  const uint8_t segs[4] = {SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,  // O
                           SEG_A | SEG_B | SEG_E | SEG_F | SEG_G,          // P
                           SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,          // E
                           SEG_C | SEG_E | SEG_G};                         // n
  display.setSegments(segs);
}

// Three blinking small "o" (lower half of a 0) + the steady seconds digit:
// the counter is about to go back to zero.
void showResetCountdown(uint8_t seconds) {
  const uint8_t SMALL_O = SEG_C | SEG_D | SEG_E | SEG_G;
  uint8_t o = ((millis() / RESET_BLINK_MS) % 2 == 0) ? SMALL_O : 0;
  const uint8_t segs[4] = {o, o, o, display.encodeDigit(seconds)};
  display.setSegments(segs);
}
