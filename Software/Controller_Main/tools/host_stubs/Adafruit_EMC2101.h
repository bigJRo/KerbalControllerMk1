// Host stand-in for Adafruit_EMC2101 (fan controller): the calls Controller_Main makes.
#pragma once
#include <Arduino.h>
class Adafruit_EMC2101 {
public:
  bool begin(uint8_t = 0x4C, TwoWire * = nullptr) { return true; }
  int8_t getInternalTemperature() { return 0; }
  float getExternalTemperature() { return 0; }
  uint16_t getFanRPM() { return 0; }
  uint8_t getDutyCycle() { return 0; }
  bool setDutyCycle(uint8_t) { return true; }
};
