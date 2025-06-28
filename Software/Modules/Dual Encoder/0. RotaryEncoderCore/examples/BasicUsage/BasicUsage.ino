#include <RotaryEncoderCore.h>

// Pin configuration (adjust based on your hardware)
constexpr uint8_t encoderA = 2;
constexpr uint8_t encoderB = 3;
constexpr uint8_t encoderBtn = 4;

RotaryEncoder encoder;

void setup() {
  Serial.begin(9600);
  attachEncoder(encoder, encoderA, encoderB, encoderBtn);

  encoder.onChange = [](int32_t pos) {
    Serial.print("Encoder position: ");
    Serial.println(pos);
  };

  encoder.onShortPress = []() {
    Serial.println("Short press detected");
  };

  encoder.onLongPress = []() {
    Serial.println("Long press detected");
  };
}

void loop() {
  updateEncoder(encoder);
  updateEncoderButton(encoder);
}
