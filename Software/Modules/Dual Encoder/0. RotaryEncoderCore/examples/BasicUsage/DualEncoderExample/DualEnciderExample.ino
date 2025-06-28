#include "RotaryEncoderCore.h"

RotaryEncoder encoder1, encoder2;

void setup() {
  attachEncoder(encoder1, PIN_PA1, PIN_PB3, PIN_PA5);
  attachEncoder(encoder2, PIN_PC3, PIN_PC2, PIN_PC1);

  initEncoder(encoder1);
  initEncoder(encoder2);

  encoder1.onChange = [] (int32_t pos) { Serial.print("Enc1: "); Serial.println(pos); };
  encoder2.onChange = [] (int32_t pos) { Serial.print("Enc2: "); Serial.println(pos); };
}

void loop() {
  updateEncoder(encoder1);
  updateEncoderButton(encoder1);

  updateEncoder(encoder2);
  updateEncoderButton(encoder2);
}
