#include <MainDisplayCore.h>

void setup() {
  if (!beginModule(0x23)) {
    Serial.println("Display failed to initialize");
    while (1);
  }
}

void loop() {
  // Example use
}
