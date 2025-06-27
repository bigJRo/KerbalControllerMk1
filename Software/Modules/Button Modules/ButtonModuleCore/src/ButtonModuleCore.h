#ifndef BUTTON_MODULE_CORE_H
#define BUTTON_MODULE_CORE_H

#include <Arduino.h>

class ButtonModuleCore {
public:
  static void initHardware();
  static void debugBlink(uint8_t pin, uint8_t times = 3);
  static uint16_t readStableAnalog(uint8_t pin, uint8_t samples = 10);
};

#endif  // BUTTON_MODULE_CORE_H
