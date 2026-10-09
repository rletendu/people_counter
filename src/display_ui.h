#pragma once

#include <Arduino.h>
#include <TM1637Display.h>

extern TM1637Display display;   // pins: see TM1637_CLK_PIN / TM1637_DIO_PIN in config.h

void showSnakeFrame(byte step);
void showLockedText();     // "LOC": button settings menus locked
void showUnlockedText();   // "OPEn": button settings menus unlocked
void showResetCountdown(uint8_t seconds);  // blinking "ooo" + seconds left before the counter reset
