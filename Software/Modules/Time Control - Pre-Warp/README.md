# KCMk1_PreWarp_Time

**Module:** Pre-Warp Time  
**Version:** 1.0  
**Date:** 2026-04-08  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  
**Hardware:** KC-01-1881/1882 7-Segment Display Module v2.0  
**Library:** Kerbal7SegmentCore v1.0.0  

---

## Overview

The Pre-Warp Time module allows the operator to set a pre-warp time duration in minutes using the rotary encoder or three preset buttons. The value is transmitted to the main controller for use in time warp sequencing. Three GOLD preset buttons provide one-press access to the most commonly used durations. The encoder allows fine-grained control with three-tier acceleration for navigating the full 0-9999 minute range.

---

## Module Identity

| Parameter | Value |
|---|---|
| I2C Address | `0x2B` |
| Module Type ID | `0x0C` |
| Capability Flags | `0x10` (K7SC_CAP_DISPLAY) |
| Data Packet Size | 6 bytes |
| NeoPixel Buttons | 3 (SK6812MINI-EA, GRBW) |
| GPIO Buttons | 1 (BTN_EN — encoder pushbutton, no LED) |
| Display | 4-digit MAX7219, 0-9999 |
| Encoder | PEC11R-4220F-S0024, hardware debounced |

---

## Panel Layout

Physical layout (top to bottom, center justified):

![Pre-Warp Time Panel Layout](panel_layout.svg)

Flash color shown. All buttons return to dim white backlight after 150ms. ENABLED state uses the SK6812MINI-EA white channel only.

---

## Button Reference

| Button | Pin | Function | Behavior | Preset Value |
|---|---|---|---|---|
| BTN01 | PC1 | 5 min | GOLD flash → sets value | 5 |
| BTN02 | PC0 | 1 hour | GOLD flash → sets value | 60 |
| BTN03 | PA4 | 1 day | GOLD flash → sets value | 1440 |
| BTN_EN | PC3 | 0 min | Momentary — no LED | 0 |

### Button Flash Behavior

Preset buttons flash GOLD for 150ms when pressed then automatically return to dim white. The display jumps to the preset value immediately on press. The flash provides tactile confirmation without leaving a persistent LED state that would imply the button is toggled on.

### Color Design Notes

- **GOLD** — used for all three preset buttons. Consistent with the Time Control module's warp-family color theme. The uniform color reinforces that all three are the same type of action (time preset) rather than different functions.
- **White channel ENABLED** — clean neutral backlight, same as GPWS module.

---

## Display Reference

| Parameter | Value |
|---|---|
| Range | 0–9999 |
| Units | Minutes |
| Default value | 0 |
| Leading zeros | None — shows `60` not `0060` |
| Decimal point | Not used |
| Reset action | Press BTN_EN — returns to 0 |

### Preset Reference

| Button | Label | Value | Equivalent |
|---|---|---|---|
| BTN01 | 5 min | 5 | 5 minutes |
| BTN02 | 1 hour | 60 | 60 minutes |
| BTN03 | 1 day | 1440 | 24 hours |
| BTN_EN | 0 min | 0 | Zero (warp now) |

### Encoder Acceleration

| Speed | Click interval | Step size |
|---|---|---|
| Slow | > 150 ms | ±1 |
| Medium | 50–150 ms | ±10 |
| Fast | < 50 ms | ±100 |

Value clamps at 0 and 9999.

---

## I2C Protocol

### Data Packet (6 bytes, module → controller)

```
Byte 0:   Button events  (bit0=BTN01 pressed, bit1=BTN02 pressed,
                          bit2=BTN03 pressed, bit3=BTN_EN pressed)
Byte 1:   Change mask    (same bit layout)
Byte 2:   Module state   (always 0x00 — no persistent toggle state
                          on this module)
Byte 3:   Reserved       (always 0x00)
Byte 4:   Value HIGH     (display value in minutes, big-endian)
Byte 5:   Value LOW      (display value in minutes, big-endian)
```

INT asserts on any button press or display value change. The controller reads the display value (bytes 4-5) to determine the configured pre-warp duration.

### Additional Command

`K7SC_CMD_SET_VALUE (0x09)` — controller can set the display value directly:
```
[ADDR+W] [0x09] [VALUE_HIGH] [VALUE_LOW]
```
Value is big-endian uint16, clamped to 0-9999.

---

## Wiring

| Signal | ATtiny816 Pin | Function |
|---|---|---|
| CLK | PA7 (pin 8) | MAX7219 SPI clock |
| LOAD | PA6 (pin 7) | MAX7219 SPI latch |
| DATA | PA5 (pin 6) | MAX7219 SPI data |
| BTN03 | PA4 (pin 5) | 1 day preset button |
| BTN02 | PC0 (pin 15) | 1 hour preset button |
| BTN01 | PC1 (pin 16) | 5 min preset button |
| NEOPIX_CMD | PC2 (pin 17) | SK6812 data output |
| BTN_EN | PC3 (pin 18) | Encoder pushbutton |
| ENC_A | PB4 (pin 10) | Encoder channel A |
| ENC_B | PB5 (pin 9) | Encoder channel B |
| INT | PA1 (pin 20) | Interrupt output (active low) |
| SCL | PB0 (pin 14) | I2C clock |
| SDA | PB1 (pin 13) | I2C data |

---

## Installation

### Prerequisites

1. Arduino IDE with megaTinyCore installed
2. Kerbal7SegmentCore library installed (`Sketch → Include Library → Add .ZIP Library`)
3. tinyNeoPixel_Static included with megaTinyCore — no separate install needed

### Arduino IDE Settings

| Setting | Value |
|---|---|
| Board | ATtiny816 (megaTinyCore) |
| Clock | 10 MHz or higher |
| tinyNeoPixel Port | **Port C** — NeoPixel is on PC2 |
| Programmer | jtag2updi or SerialUPDI |

### Flash Procedure

1. Open `KCMk1_PreWarp_Time.ino` in Arduino IDE
2. Confirm IDE settings — especially **Port C** for NeoPixel
3. Connect UPDI programmer to the module's UPDI header
4. Click Upload

### Verify Operation

After flashing the display should show `0` and all three buttons should illuminate dim white. Press BTN01 and confirm display jumps to 5 with a GOLD flash. Press BTN02 and confirm display shows 60. Press BTN03 and confirm 1440. Turn encoder and confirm increments/decrements with acceleration. Use the `DiagnosticDump` example from Kerbal7SegmentCore for full serial verification.

---

## I2C Bus Position

| Address | Module |
|---|---|
| `0x20`–`0x25` | Standard button modules |
| `0x26` | EVA Module |
| `0x27` | Reserved |
| `0x28` | Joystick Rotation |
| `0x29` | Joystick Translation |
| `0x2A` | GPWS Input Panel |
| `0x2B` | **Pre-Warp Time** — this module |

---

## Revision History

| Version | Date | Notes |
|---|---|---|
| 1.0 | 2026-04-08 | Initial release |
