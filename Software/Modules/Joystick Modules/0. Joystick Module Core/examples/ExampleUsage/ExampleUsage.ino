#include <JoystickModuleCore.h>

constexpr uint8_t panel_addr = 0x23;

void setup() {
  beginModule(panel_addr);
}

void loop() {
  if (updateLED) {
    // handle_ledUpdate();  <-- Defined in your module sketch
    updateLED = false;
  }

  uint8_t pins[NUM_BUTTONS] = { BUTTON01, BUTTON02, BUTTON_JOY };
  readJoystickInputs(pins);
}
