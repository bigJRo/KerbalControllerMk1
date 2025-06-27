#include <ButtonModuleCore.h>

void setup() {
  ButtonModuleCore::initHardware();
  ButtonModuleCore::debugBlink(LED_BUILTIN, 5);
}

void loop() {
  uint16_t val = ButtonModuleCore::readStableAnalog(A0);
  Serial.println(val);
  delay(500);
}
