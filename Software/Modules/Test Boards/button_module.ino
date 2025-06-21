/********************************************************************************************************************************
  Modular Tester Code for Button Module on Kerbal Controller Mk1
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/

#include <Wire.h>

#define panel_addr 0x20

uint16_t button_state = 0;        // Current button states (16 bits)
uint16_t button_state_prev = 0;   // Previous button states for edge detection
uint16_t led_bits = 0;            // Current LED bitmask to send (master’s local copy)
uint16_t led_bits_slave = 0;      // LED bitmask received from slave (for sync)

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Wire.begin();  // Join I2C bus as master

  Serial.println("I2C Master with LED Sync Started");
}

void loop() {
  // Request 4 bytes from slave: 2 for buttons, 2 for LEDs
  Wire.requestFrom(panel_addr, 4);

  if (Wire.available() == 4) {
    uint8_t btn_low = Wire.read();
    uint8_t btn_high = Wire.read();
    uint8_t led_low = Wire.read();
    uint8_t led_high = Wire.read();

    button_state = ((uint16_t)btn_high << 8) | btn_low;
    led_bits_slave = ((uint16_t)led_high << 8) | led_low;

    // Detect button presses (rising edges)
    for (uint8_t i = 0; i < 16; i++) {
      bool curr = (button_state & (1 << i)) != 0;
      bool prev = (button_state_prev & (1 << i)) != 0;

      if (curr && !prev) {
        // Toggle LED bit locally
        led_bits ^= (1 << i);
        Serial.print("Button ");
        Serial.print(i);
        Serial.println(" pressed, toggling LED bit locally");
      }
    }
    button_state_prev = button_state;

    // If local led_bits differs from slave’s led_bits, update slave
    if (led_bits != led_bits_slave) {
      Wire.beginTransmission(panel_addr);
      Wire.write((uint8_t)(led_bits & 0xFF));        // LSB
      Wire.write((uint8_t)((led_bits >> 8) & 0xFF)); // MSB
      Wire.endTransmission();
      Serial.println("LED bits updated on slave");
    } else {
      // Sync local led_bits to slave's to avoid mismatch
      led_bits = led_bits_slave;
    }

    // Debug output
    Serial.print("Buttons: 0x");
    if (btn_high < 0x10) Serial.print('0');
    Serial.print(btn_high, HEX);
    if (btn_low < 0x10) Serial.print('0');
    Serial.print(btn_low, HEX);

    Serial.print("  LEDs (slave): 0x");
    if ((led_bits_slave >> 8) < 0x10) Serial.print('0');
    Serial.println(led_bits_slave, HEX);
  } else {
    Serial.println("No data from slave");
  }

  delay(50);
}
