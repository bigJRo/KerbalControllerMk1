# KCMk1_GPWS_Input

**Module:** GPWS Input Panel  
**Version:** 1.0  
**Date:** 2026-04-08  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  
**Hardware:** KC-01-1881/1882 7-Segment Display Module v2.0  
**Library:** Kerbal7SegmentCore v1.0.0  

---

## Overview

The GPWS Input Panel allows the operator to configure ground proximity warning system behavior for Kerbal Space Program. A 4-digit display shows the current altitude or distance threshold. When the vessel crosses this threshold the main controller triggers the configured warning behavior. Three buttons independently control GPWS mode, proximity alarm, and rendezvous radar enable states. All button logic is managed on the module — the main controller receives the current state in every data packet.

---

## Module Identity

| Parameter | Value |
|---|---|
| I2C Address | `0x2A` |
| Module Type ID | `0x0B` |
| Capability Flags | `0x10` (K7SC_CAP_DISPLAY) |
| Data Packet Size | 6 bytes |
| NeoPixel Buttons | 3 (SK6812MINI-EA, GRBW) |
| GPIO Buttons | 1 (BTN_EN — encoder pushbutton, no LED) |
| Display | 4-digit MAX7219, 0-9999 |
| Encoder | PEC11R-4220F-S0024, hardware debounced |

---

## Panel Layout

Physical layout (top to bottom, center justified):

![GPWS Input Panel Layout](panel_layout.svg)

ACTIVE state colors shown. ENABLED state uses the SK6812MINI-EA white channel only for a clean neutral backlight. BTN01 dashed border indicates multi-state behavior.

---

## Button Reference

| Button | Pin | Function | Behavior | Colors |
|---|---|---|---|---|
| BTN01 | PC1 | GPWS Enable | 3-state cycle | OFF → GREEN → AMBER → OFF |
| BTN02 | PC0 | Proximity Alarm | Toggle | OFF ↔ GREEN |
| BTN03 | PA4 | Rendezvous Radar | Toggle | OFF ↔ GREEN |
| BTN_EN | PC3 | Reset Threshold | Momentary | No LED — resets display to 200 |

### BTN01 State Meanings

| State | Color | Meaning |
|---|---|---|
| 0 | Dim white | GPWS disabled |
| 1 | GREEN | Full GPWS active — warning at threshold |
| 2 | AMBER | Proximity tone only — audio warning only |

Each press advances to the next state. The main controller reads the current state from bits 0-1 of the state byte (Byte 2 of the data packet).

### Color Design Notes

- **GREEN** — nominal active state for all three buttons. Consistent with system-wide active color.
- **AMBER** — GPWS State 2 signals a reduced/degraded warning mode. Amber is the awareness color used throughout the system for cautionary states.
- **White channel ENABLED** — the SK6812MINI-EA dedicated white LED provides a clean neutral dim backlight distinct from any color state.

---

## Display Reference

| Parameter | Value |
|---|---|
| Range | 0–9999 |
| Units | Meters (altitude or distance threshold) |
| Default value | 200 |
| Leading zeros | None — shows `200` not `0200` |
| Decimal point | Not used |
| Reset action | Press BTN_EN — returns to 200 |

### Encoder Acceleration

| Speed | Click interval | Step size |
|---|---|---|
| Slow | > 150 ms | ±1 |
| Medium | 50–150 ms | ±10 |
| Fast | < 50 ms | ±100 |

Value clamps at 0 and 9999. The hardware RC debounce (10nF caps on ENC_A/B) handles mechanical bounce; software adds a 2ms guard for any residual glitches.

---

## I2C Protocol

### Data Packet (6 bytes, module → controller)

```
Byte 0:   Button events  (bit0=BTN01 pressed, bit1=BTN02 pressed,
                          bit2=BTN03 pressed, bit3=BTN_EN pressed)
Byte 1:   Change mask    (same bit layout — set on any edge)
Byte 2:   Module state   (bits 0-1 = BTN01 cycle state 0/1/2,
                          bit 2 = BTN02 active,
                          bit 3 = BTN03 active)
Byte 3:   Reserved       (always 0x00)
Byte 4:   Value HIGH     (display value, big-endian)
Byte 5:   Value LOW      (display value, big-endian)
```

INT asserts on any button state change or display value change. Controller reads the full 6-byte packet on each INT.

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
| BTN03 | PA4 (pin 5) | Proximity button |
| BTN02 | PC0 (pin 15) | Proximity alarm button |
| BTN01 | PC1 (pin 16) | GPWS enable button |
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

1. Open `KCMk1_GPWS_Input.ino` in Arduino IDE
2. Confirm IDE settings — especially **Port C** for NeoPixel
3. Connect UPDI programmer to the module's UPDI header
4. Click Upload

### Verify Operation

After flashing the display should show `200` and all three buttons should illuminate dim white. Turn the encoder and confirm the display increments/decrements. Press BTN01 repeatedly and confirm the LED cycles GREEN → AMBER → dim white. Use the `DiagnosticDump` example from the Kerbal7SegmentCore library for full serial verification.

---

## I2C Bus Position

| Address | Module |
|---|---|
| `0x20`–`0x25` | Standard button modules |
| `0x26` | EVA Module |
| `0x27` | Reserved |
| `0x28` | Joystick Rotation |
| `0x29` | Joystick Translation |
| `0x2A` | **GPWS Input Panel** — this module |
| `0x2B` | Pre-Warp Time |

---

## Revision History

| Version | Date | Notes |
|---|---|---|
| 1.0 | 2026-04-08 | Initial release |
