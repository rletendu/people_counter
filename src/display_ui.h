#pragma once

#include <Arduino.h>
#include <TM1637Display.h>

extern TM1637Display display;   // CLK = D4, DIO = D5

void showSnakeFrame(byte step);
