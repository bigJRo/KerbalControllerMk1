#include <TinyWire.h>              // i2c libary for ATtiny816
#include <ShiftRegister74HC595.h>  // Output Shift Register Library

#define clockPin 3    // 74HC595 Outputs LED Serial Clock
#define latchPin 4    // 74HC595 Outputs LED Latch
#define dataOutPin 0  // 74HC595 Outputs LED Data Line

const uint8_t i2c_addr = 30;
uint8_t led_bytes[2] = { 0, 0 };

ShiftRegister74HC595<2> lights(dataOutPin, clockPin, latchPin);  // Object for display output shift registers

void setup() {
  TinyWire.begin(i2c_addr);
  TinyWire.onReceive(onI2CReceive);

  lights.setAllLow();  // Ensure all LEDs are off at startup
}

void loop() {
  lights.setAll(led_bytes); // Set all Output Shift Register bits
}

void onI2CReceive(int howMany) {
  led_bytes[0] = TinyWire.read();
  led_bytes[1] = TinyWire.read();
}
