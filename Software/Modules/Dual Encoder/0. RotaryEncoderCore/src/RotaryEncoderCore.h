#ifndef ROTARY_ENCODER_CORE_H
#define ROTARY_ENCODER_CORE_H

#include <Arduino.h>

struct RotaryEncoder {
  volatile int32_t position = 0;
  volatile bool buttonPressed = false;
  volatile unsigned long buttonPressTime = 0;
  volatile bool buttonHandled = true;

  uint8_t pinA;
  uint8_t pinB;
  uint8_t buttonPin;

  void (*onShortPress)() = nullptr;
  void (*onLongPress)() = nullptr;
};

// Setup encoder pins
void attachEncoder(RotaryEncoder& encoder, uint8_t pinA, uint8_t pinB, uint8_t buttonPin = 0xFF);

// Interrupt-driven update
void updateEncoder(RotaryEncoder& encoder);

// Call from loop() to check button logic
void updateEncoderButton(RotaryEncoder& encoder, unsigned long shortThreshold = 300, unsigned long longThreshold = 1000);

#endif
