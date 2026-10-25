// Host stand-in for Adafruit_INA219 (current sensor): the calls Controller_Main makes.
#pragma once
#include <Arduino.h>
class Adafruit_INA219 {
public:
  Adafruit_INA219(uint8_t = 0x40) {}
  bool begin(TwoWire * = nullptr) { return true; }
  float getBusVoltage_V() { return 0; }
  float getShuntVoltage_mV() { return 0; }
  float getCurrent_mA() { return 0; }
  float getPower_mW() { return 0; }
};
