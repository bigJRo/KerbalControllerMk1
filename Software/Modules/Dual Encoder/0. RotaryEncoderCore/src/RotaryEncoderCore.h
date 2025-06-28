#ifndef ROTARY_ENCODER_CORE_H
#define ROTARY_ENCODER_CORE_H

#include <Arduino.h>

// Rotary encoder data structure
struct RotaryEncoder {
  volatile int32_t position = 0;                // Encoder count
  volatile bool buttonPressed = false;          // Button press state
  volatile unsigned long buttonPressTime = 0;   // Timestamp of press
  volatile bool buttonHandled = true;           // Has the press been handled

  uint8_t pinA;                                  // Encoder A pin
  uint8_t pinB;                                  // Encoder B pin
  uint8_t buttonPin;                             // Optional button pin (0xFF if unused)
  uint8_t lastState = 0;                         // Last encoder state for transition tracking

  void (*onShortPress)() = nullptr;              // Callback on short press
  void (*onLongPress)() = nullptr;               // Callback on long press
  void (*onChange)(int32_t) = nullptr;           // Callback on position change
};

// Initializes encoder pins and state
void attachEncoder(RotaryEncoder& encoder, uint8_t pinA, uint8_t pinB, uint8_t buttonPin = 0xFF);

// Reads encoder state and updates position (should be called from ISR)
inline void updateEncoder(RotaryEncoder& encoder);

// Polls and debounces button state; triggers callbacks based on press duration
void updateEncoderButton(RotaryEncoder& encoder, unsigned long shortThreshold = 300, unsigned long longThreshold = 1000);

// Inline utility to check if button pin is valid
inline bool isButtonAvailable(const RotaryEncoder& enc) {
  return enc.buttonPin != 0xFF;
}

#endif  // ROTARY_ENCODER_CORE_H
