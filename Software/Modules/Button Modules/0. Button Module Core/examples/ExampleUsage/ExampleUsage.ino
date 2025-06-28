#include <ButtonModuleCore.h>

constexpr uint8_t panel_addr = 0x23;

const char commandNames[16][16] PROGMEM = {
  "Cmd1", "Cmd2", "Cmd3", "Cmd4",
  "Cmd5", "Cmd6", "Cmd7", "Cmd8",
  "Cmd9", "Cmd10", "Cmd11", "Cmd12",
  "Lock1", "Lock2", "Lock3", "Lock4"
};

void setup() {
  beginModule(panel_addr);
}

void loop() {
  if (updateLED) {
    // Example: handle_ledUpdate();
    updateLED = false;
  }

  readButtonStates();
}
