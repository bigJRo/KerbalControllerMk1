#include "RotaryEncoderCore.h"

void attachEncoder(RotaryEncoder& encoder, uint8_t pinA, uint8_t pinB, uint8_t buttonPin) {
  encoder.pinA = pinA;
  encoder.pinB = pinB;
  encoder.buttonPin = buttonPin;
  encoder.lastState = (digitalRead(pinA) << 1) | digitalRead(pinB);  // Initialize lastState

  pinMode(pinA, INPUT);
  pinMode(pinB, INPUT);
  if (buttonPin != 0xFF) {
    pinMode(buttonPin, INPUT);  // Active-high: no pull-up
  }
}

void updateEncoder(RotaryEncoder& encoder) {
  uint8_t state = (digitalRead(encoder.pinA) << 1) | digitalRead(encoder.pinB);
  if ((encoder.lastState == 0b00 && state == 0b01) ||
      (encoder.lastState == 0b01 && state == 0b11) ||
      (encoder.lastState == 0b11 && state == 0b10) ||
      (encoder.lastState == 0b10 && state == 0b00)) {
    encoder.position++;
  } else if ((encoder.lastState == 0b00 && state == 0b10) ||
             (encoder.lastState == 0b10 && state == 0b11) ||
             (encoder.lastState == 0b11 && state == 0b01) ||
             (encoder.lastState == 0b01 && state == 0b00)) {
    encoder.position--;
  }
  encoder.lastState = state;
}

void updateEncoderButton(RotaryEncoder& encoder, unsigned long shortThreshold, unsigned long longThreshold) {
  if (encoder.buttonPin == 0xFF) return;

  bool pressed = digitalRead(encoder.buttonPin);  // Active-high logic

  if (pressed && !encoder.buttonPressed) {
    encoder.buttonPressed = true;
    encoder.buttonPressTime = millis();
    encoder.buttonHandled = false;
  } else if (!pressed && encoder.buttonPressed) {
    unsigned long duration = millis() - encoder.buttonPressTime;
    encoder.buttonPressed = false;

    if (!encoder.buttonHandled) {
      if (duration < shortThreshold) {
        if (encoder.onShortPress) encoder.onShortPress();
      } else if (duration >= longThreshold) {
        if (encoder.onLongPress) encoder.onLongPress();
      }
    }
    encoder.buttonHandled = true;
  }
}
