// Host stand-in for arduino-pico's SPIClass, for tools/sysinfo_host.py.
#pragma once
#include "Arduino.h"
class SPIClass {
 public:
  void setSCK(int p) { sck = p; }
  void setTX(int p) { tx = p; }
  void setRX(int p) { rxp = p; }
  void begin() {}
  int sck = -1, tx = -1, rxp = -1;
};
inline SPIClass SPI;
