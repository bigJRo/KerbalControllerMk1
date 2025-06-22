# Vehicle Control Module for Kerbal Controller Mk1

This Arduino-based firmware runs on an ATtiny816 and controls a custom Kerbal Space Program controller panel. It supports:

- Shift register button inputs (16 buttons)
- NeoPixel LED outputs (12 RGB addressable LEDs)
- 4 discrete output LEDs
- I2C communication with a host Arduino using a 4-byte protocol

## Features

- Efficient use of PROGMEM for RGB color mapping
- Modular color table and LED overlay logic
- I2C slave implementation with interrupt output
- Fully documented in-line for clarity and maintenance

## I2C Protocol

### Master Reads (4 bytes)
- Byte 0: Button bits 0–7
- Byte 1: Button bits 8–15
- Byte 2: LED control bits 0–7
- Byte 3: LED control bits 8–15

### Master Writes (2 bytes)
- Byte 0: LED control bits 0–7
- Byte 1: LED control bits 8–15

## Dependencies

- `Wire.h` for I2C
- `ShiftIn.h` for button input
- `tinyNeoPixel.h` for addressable LED control
- `avr/pgmspace.h` for PROGMEM access (AVR-based microcontrollers)

## Build Instructions

1. Use the [Arduino IDE](https://www.arduino.cc/en/software).
2. Install **megaTinyCore** via the Boards Manager (for ATtiny816 support).
3. Install the following libraries:
   - **ShiftIn** (by Paul Stoffregen)
   - **tinyNeoPixel** (Adafruit fork for ATtiny support)
4. Select **ATtiny816** under Tools > Board.
5. Configure fuse bits as appropriate (e.g., 20 MHz internal clock).
6. Upload the sketch using a UPDI programmer (e.g., Serial UPDI or jtag2updi).

## Wiring Diagram (Summary)

| Component       | ATtiny816 Pin | Purpose              |
|----------------|---------------|----------------------|
| Shift Register | 5, 6, 7, 13    | Data, Clock, Load    |
| NeoPixels      | 12            | Data out             |
| I2C            | 8 (SCL), 9 (SDA) | Communication      |
| INT_OUT        | 4             | Host notification    |
| Discrete LEDs  | 10, 11, 14, 15| Output indicators    |

> Use 100Ω–330Ω resistors where appropriate for LED or signal protection.

## Example Host-Side Code (Arduino)

```cpp
#include <Wire.h>

#define MODULE_ADDR 0x23

void setup() {
  Wire.begin();
  Serial.begin(9600);
}

void loop() {
  // Request button state
  Wire.requestFrom(MODULE_ADDR, 4);
  if (Wire.available() == 4) {
    uint8_t btn_lsb = Wire.read();
    uint8_t btn_msb = Wire.read();
    uint8_t led_lsb = Wire.read();
    uint8_t led_msb = Wire.read();

    uint16_t buttonState = (btn_msb << 8) | btn_lsb;
    Serial.print("Buttons: ");
    Serial.println(buttonState, BIN);

    // Example: turn on first 8 LEDs if first button is pressed
    uint16_t ledState = (buttonState & 0x0001) ? 0x00FF : 0x0000;
    Wire.beginTransmission(MODULE_ADDR);
    Wire.write((uint8_t)(ledState & 0xFF));
    Wire.write((uint8_t)(ledState >> 8));
    Wire.endTransmission();
  }

  delay(100);
}
```

## License

Licensed under GPL-3.0. Original code references work by [CodapopKSP](https://github.com/CodapopKSP/UntitledSpaceCraft).

Final implementation by J. Rostoker for Jeb's Controller Works.
