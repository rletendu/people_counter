#pragma once

#include <Arduino.h>
#include <TM1637Display.h>

extern TM1637Display display;   // pins: see TM1637_CLK_PIN / TM1637_DIO_PIN in config.h

void showSnakeFrame(byte step);
