// Host stand-in for the Teensy Bounce library: the calls Controller_Main makes.
#pragma once
#include <Arduino.h>
class Bounce {
public:
  Bounce(uint8_t, unsigned long) {}
  bool update() { return false; }
  bool fallingEdge() { return false; }
  bool risingEdge() { return false; }
  uint8_t read() { return 0; }
};
