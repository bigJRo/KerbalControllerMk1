# ButtonModuleCore – Kerbal Controller Mk1 Core Library

This library provides the shared firmware core for **Kerbal Controller Mk1** button modules built around an **ATtiny816** (megaTinyCore).

## What it provides

- 16-button input scanning via two chained **74HC165** shift registers (`ShiftIn`)
- 12 **NeoPixel RGB** outputs (`tinyNeoPixel`)
- 4 **discrete** LED outputs
- I2C slave protocol:
  - Master reads: **button state** + **pressed edges** + **released edges** (6 bytes)
  - Master writes: **LED control** bits (16-bit) and optional **button_active_bits** (16-bit)
- Startup **bulb test** pattern

## Pin mapping (ATtiny816 / megaTinyCore)

These are the canonical defines used by the library (see `src/ButtonModuleCore.h`).

| Signal | Arduino macro | Pin |
|---|---:|---|
| I2C SDA | `SDA` | `PIN_PB1` |
| I2C SCL | `SCL` | `PIN_PB0` |
| Interrupt out (to host) | `INT_OUT` | `PIN_PA1` |
| Shift register load | `load` | `PIN_PA7` |
| Shift register clock enable (active low) | `clockEnable` | `PIN_PA6` |
| Shift register clock in | `clockIn` | `PIN_PB5` |
| Shift register data in | `dataInPin` | `PIN_PA5` |
| NeoPixel data out | `neopixCmd` | `PIN_PA4` |
| Discrete LED 13 | `led_13` | `PIN_PB3` |
| Discrete LED 14 | `led_14` | `PIN_PC0` |
| Discrete LED 15 | `led_15` | `PIN_PC2` |
| Discrete LED 16 | `led_16` | `PIN_PC1` |

## Usage

In your module firmware:

1. Call `beginModule(panel_addr)` from `setup()`.
2. Call `readButtonStates()` frequently in `loop()`.
3. When `updateLED` becomes true, use the received control words:
   - `led_bits` (16-bit)
   - `button_active_bits` (16-bit)

How `led_bits` and `button_active_bits` map onto the 12 NeoPixels and 4 discrete LEDs is **module-specific**. This core library stores the values and sets `updateLED` when they change.

## I2C protocol

### Master read (module -> master)

The module responds with **6 bytes**:

| Bytes | Field | Description |
|---:|---|---|
| 0..1 | `button_state_bits` | Current sampled state of all 16 buttons (LSB first) |
| 2..3 | `button_pressed_bits` | Edge bits latched for transitions **0 -> 1** since last master read |
| 4..5 | `button_released_bits` | Edge bits latched for transitions **1 -> 0** since last master read |

**Pressed/released are one-shot:** they are cleared after being sent to the master.

### Master write (master -> module)

Accepted payloads:

- **2 bytes (legacy):**
  - `led_bits` (LSB, MSB)

- **4 bytes:**
  - `led_bits` (LSB, MSB)
  - `button_active_bits` (LSB, MSB)

When either value changes, the core sets `updateLED = true`.

## Bulb test

`beginModule()` runs `bulbTest()` during startup. The current bulb test implementation cycles all NeoPixels through a basic color sequence, then displays a fixed final pattern, and finally clears the outputs.

