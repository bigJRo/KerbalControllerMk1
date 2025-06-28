#include <RotaryEncoder.h>

RotaryEncoder encoder;

void onShort() {
  Serial.println("Short press");
}
void onLong() {
  Serial.println("Long press");
}

void setup() {
  Serial.begin(9600);
  attachEncoder(encoder, PIN_PA3, PIN_PA2, PIN_PA1);  // Active-high button
  encoder.onShortPress = onShort;
  encoder.onLongPress = onLong;
}

void loop() {
  updateEncoderButton(encoder);

  static int32_t lastPos = 0;
  if (encoder.position != lastPos) {
    Serial.print("Position: ");
    Serial.println(encoder.position);
    lastPos = encoder.position;
  }
}
