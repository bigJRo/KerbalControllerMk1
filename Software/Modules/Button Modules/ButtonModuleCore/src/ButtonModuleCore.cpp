#include "ButtonModuleCore.h"

void ButtonModuleCore::initHardware() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void ButtonModuleCore::debugBlink(uint8_t pin, uint8_t times) {
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(100);
    digitalWrite(pin, LOW);
    delay(100);
  }
}

uint16_t ButtonModuleCore::readStableAnalog(uint8_t pin, uint8_t samples) {
  uint32_t total = 0;
  for (uint8_t i = 0; i < samples; i++) {
    total += analogRead(pin);
    delay(2);
  }
  return total / samples;
}
