/********************************************************************************************************************************
  XIAO SAMD21 Host Interface for Kerbal Controller Mk1 - Assembly Test Module

  Behavior:
    1) On interrupt from the module (active LOW), read 6-byte status over I2C:
         button_state_bits, button_pressed_bits, button_released_bits
    2) For newly pressed buttons:
         - set the corresponding button_active bit = 1
    3) For newly released buttons:
         - FIRST clear any previously latched led_bits and button_active_bits
         - THEN set the corresponding led_bit = 1 for the released button
    4) Send updated led_bits and button_active_bits back to the module (4-byte write)

  ButtonModuleCore v1.1 protocol (slave):
    - Read 6 bytes:
        [0] state LSB, [1] state MSB
        [2] pressed LSB, [3] pressed MSB
        [4] released LSB, [5] released MSB
    - Write 4 bytes:
        [0] led_bits LSB, [1] led_bits MSB
        [2] button_active_bits LSB, [3] button_active_bits MSB

  Notes:
    - Module INT_OUT is active LOW and is cleared by the module after the host reads.
    - led_bits and button_active_bits are cleared on the next button release event.
    - button_active_bits represents "currently active" buttons (set on press, cleared on release).

  (c) 2026 Jeb's Controller Works / J. Rostoker - GPLv3
********************************************************************************************************************************/
#include <Arduino.h>
#include <Wire.h>

// -------------------- User Config --------------------
constexpr uint8_t MODULE_ADDR    = 0x2B;  // Assembly Test Module I2C address
constexpr uint8_t MODULE_INT_PIN = 6;     // XIAO SAMD21 pin connected to module INT_OUT (active LOW)
// -----------------------------------------------------

volatile bool moduleInterruptFlag = false;

// Local cached control words (what we last told the module)
uint16_t led_bits = 0;
uint16_t button_active_bits = 0;

void IRAM_ATTR onModuleInterrupt() {
  moduleInterruptFlag = true;
}

static bool readModuleStatus(uint16_t &state, uint16_t &pressed, uint16_t &released) {
  Wire.requestFrom((int)MODULE_ADDR, 6);
  if (Wire.available() != 6) {
    while (Wire.available()) (void)Wire.read();
    return false;
  }

  const uint8_t s0 = Wire.read();
  const uint8_t s1 = Wire.read();
  const uint8_t p0 = Wire.read();
  const uint8_t p1 = Wire.read();
  const uint8_t r0 = Wire.read();
  const uint8_t r1 = Wire.read();

  state    = (uint16_t)((uint16_t)s1 << 8) | s0;
  pressed  = (uint16_t)((uint16_t)p1 << 8) | p0;
  released = (uint16_t)((uint16_t)r1 << 8) | r0;
  return true;
}

static void writeModuleControl(uint16_t newLedBits, uint16_t newActiveBits) {
  Wire.beginTransmission(MODULE_ADDR);
  Wire.write((uint8_t)(newLedBits & 0xFF));
  Wire.write((uint8_t)(newLedBits >> 8));
  Wire.write((uint8_t)(newActiveBits & 0xFF));
  Wire.write((uint8_t)(newActiveBits >> 8));
  Wire.endTransmission();
}

void setup() {
  Wire.begin();  // XIAO SAMD21 as I2C master

  pinMode(MODULE_INT_PIN, INPUT_PULLUP);  // INT_OUT is active LOW
  attachInterrupt(digitalPinToInterrupt(MODULE_INT_PIN), onModuleInterrupt, FALLING);

  // Push initial cleared state
  writeModuleControl(led_bits, button_active_bits);
}

void loop() {
  if (!moduleInterruptFlag) return;

  noInterrupts();
  moduleInterruptFlag = false;
  interrupts();

  uint16_t state = 0, pressed = 0, released = 0;
  if (!readModuleStatus(state, pressed, released)) {
    return;
  }

  bool changed = false;

  // Newly pressed buttons -> set active bits
  if (pressed) {
    const uint16_t newActive = (uint16_t)(button_active_bits | pressed);
    if (newActive != button_active_bits) {
      button_active_bits = newActive;
      changed = true;
    }
  }

  // Newly released buttons -> clear all latched state, then set LED bit for this release
  if (released) {
    // Clear all previous indications
    if (led_bits != 0 || button_active_bits != 0) {
      led_bits = 0;
      button_active_bits = 0;
      changed = true;
    }

    // Latch the released button as an LED indication
    if ((led_bits | released) != led_bits) {
      led_bits = released;
      changed = true;
    }
  }

  if (changed) {
    writeModuleControl(led_bits, button_active_bits);
  }
}
