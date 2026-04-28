# Kerbal7SegmentCore

**Version:** 2.0.0  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  
**Hardware:** KC-01-1881/1882 7-Segment Display Module v2.0  
**Target MCU:** ATtiny816 (megaTinyCore, 20 MHz internal)  

---

## Overview

Kerbal7SegmentCore is the Arduino library for Kerbal Controller Mk1 7-segment display modules (KC-01-1880). Each module runs on an ATtiny816, acts as an I2C target device, and provides:

- 4-digit 7-segment display via MAX7219 (software SPI, bit-banged)
- Rotary encoder with click-count-based acceleration (sketch-implemented)
- Three SK6812MINI-EA NeoPixel buttons
- Encoder pushbutton (BTN_EN)
- I2C target communication (Kerbal Controller Mk1 protocol v2.0)
- INT pin assertion for event-driven controller polling

**Design philosophy:** The library is a hardware interface layer only. All application logic — lifecycle, LED state, button behaviour, value tracking — belongs in the sketch. The library reports what the hardware did via `inputState` and `cmdState`. The sketch decides what it means and what to do about it.

**Dependency:** KerbalModuleCommon v1.1.0 (colour palette, command constants, type IDs)

---

## Hardware

**PCB:** KC-01-1881 (schematic) / KC-01-1882 (board)  
**MCU:** ATtiny816-MNR  
**Display IC:** MAX7219CWG+T  
**Display:** FJ4401AG 4-digit 7-segment (common cathode)  
**LEDs:** SK6812MINI-EA (NEO_GRB 3-byte mode — white channel not connected on this board)  
**Encoder:** PEC11R-4220F-S0024 with hardware RC debounce (C1/C2 10nF)

### Pin Assignment (verified against KC-01-1881 schematic v2.0)

| ATtiny816 | Net | Signal | Notes |
|---|---|---|---|
| PA1 (pin 20) | BUTTON01 | BTN01 input | Active HIGH, pull-down R9 |
| PA5 (pin 6) | LOAD_DATA | MAX7219 LOAD | Active high latch |
| PA6 (pin 7) | DATA_IN | MAX7219 DATA | Bits shifted MSB first |
| PA7 (pin 8) | CLK | MAX7219 CLK | |
| PB0 (pin 14) | SCL | I2C clock | |
| PB1 (pin 13) | SDA | I2C data | |
| PB3 (pin 11) | BUTTON_EN | Encoder pushbutton | Active HIGH, pull-down R6 |
| PB4 (pin 10) | ENC_A | Encoder channel A | Pull-up R4, debounce C1 |
| PB5 (pin 9) | ENC_B | Encoder channel B | Pull-up R2, debounce C2 |
| PC0 (pin 15) | INT | Interrupt output | Active LOW |
| PC1 (pin 16) | BUTTON03 | BTN03 input | Active HIGH, pull-down R11 |
| PC2 (pin 17) | BUTTON02 | BTN02 input | Active HIGH, pull-down R10 |
| PC3 (pin 18) | NEOPIX_CMD | SK6812 data chain | LED2→LED3→LED4 |

### NeoPixel Note

The SK6812MINI-EA on this board responds to **NEO_GRB 3-byte** framing only. The white channel is not connected. All colour values are passed as `(r, g, b)` directly.

---

## Dependencies

- **KerbalModuleCommon v1.1.0** — colour palette, `KMC_CMD_*` constants, `KMC_TYPE_*` IDs
- **tinyNeoPixel** — bundled with megaTinyCore, no separate install needed
- **Wire** — bundled with megaTinyCore

`Wire.begin()` is called internally by `k7scBegin()` — do **not** call it in the sketch.

---

## Installation

1. Install **KerbalModuleCommon v1.1.0** from zip (Sketch → Include Library → Add .ZIP)
2. Install **Kerbal7SegmentCore v2.0.0** from zip
3. Arduino IDE settings:
   - Board: `ATtiny816` (megaTinyCore)
   - Clock: `20 MHz internal`
   - Programmer: `serialUPDI` or `jtag2updi`

---

## Quick Start

```cpp
#include <KerbalModuleCommon.h>
#include <Kerbal7SegmentCore.h>

#define I2C_ADDRESS    0x2A
#define DEFAULT_VALUE  200

static int16_t _value = DEFAULT_VALUE;

void setup() {
    k7scBegin(I2C_ADDRESS, KMC_TYPE_GPWS_INPUT, KMC_CAP_DISPLAY);
    buttonsClearAll();
    displayShutdown();
}

void loop() {
    k7scUpdate();

    // Drain input when not active
    if (cmdState.lifecycle != K7SC_ACTIVE) {
        inputState.buttonPressed  = 0;
        inputState.buttonReleased = 0;
        inputState.buttonChanged  = 0;
        inputState.encoderDelta   = 0;
        inputState.encoderChanged = false;
        encoderClearDelta();
        return;
    }

    // Handle encoder
    if (inputState.encoderChanged) {
        int32_t next = (int32_t)_value + inputState.encoderDelta;
        if (next < K7SC_VALUE_MIN) next = K7SC_VALUE_MIN;
        if (next > K7SC_VALUE_MAX) next = K7SC_VALUE_MAX;
        _value = (int16_t)next;
        displaySetValue(_value);

        uint8_t payload[2] = {
            (uint8_t)(_value >> 8),
            (uint8_t)(_value & 0xFF)
        };
        k7scQueuePacket(payload, 2);
    }

    // Clear consumed input
    inputState.buttonPressed  = 0;
    inputState.buttonReleased = 0;
    inputState.buttonChanged  = 0;
    inputState.encoderDelta   = 0;
    inputState.encoderChanged = false;
}
```

---

## State Structs

Two structs form the complete interface between the library and the sketch.

### K7SCCommandState — commands from master controller

Set by the library when I2C commands arrive. Sketch reads and acts.

```cpp
struct K7SCCommandState {
    K7SCLifecycle lifecycle;   // persistent — ACTIVE/SLEEPING/DISABLED/BOOT_READY
    bool isBulbTest;           // persistent — CMD_BULB_TEST 0x01/0x00
    bool isReset;              // one-shot   — CMD_RESET received
    bool hasNewValue;          // one-shot   — CMD_SET_VALUE received
    int16_t newValue;          // value from CMD_SET_VALUE (signed)
    bool hasLEDState;          // one-shot   — CMD_SET_LED_STATE received
    uint8_t ledState;          // raw byte from CMD_SET_LED_STATE
};
```

**Persistent fields** stay set until the master changes them. The sketch should not clear them.  
**One-shot fields** are auto-cleared by the library after one loop iteration.

### K7SCInputState — local hardware input

Accumulated between sketch reads. Sketch clears fields after consuming them.

```cpp
struct K7SCInputState {
    uint8_t buttonPressed;   // rising edge bitmask — bit0=BTN01 … bit3=BTN_EN
    uint8_t buttonReleased;  // falling edge bitmask
    uint8_t buttonChanged;   // any edge (pressed | released)
    int16_t encoderDelta;    // net clicks since last clear (+CW, -CCW)
    bool    encoderChanged;  // true if any encoder movement since last clear
};
```

---

## API Reference

### Top-level

```cpp
void k7scBegin(uint8_t i2cAddress, uint8_t typeId, uint8_t capFlags);
void k7scUpdate();   // call every loop — polls hardware, clears one-shot flags
```

### Packet transmission

```cpp
// Sketch assembles payload, library prepends 3-byte header and asserts INT.
// Last-write-wins if called multiple times before master reads.
void k7scQueuePacket(const uint8_t* payload, uint8_t length);
```

### Display

```cpp
void     displaySetValue(uint16_t value);        // 0–9999, no leading zeros
uint16_t displayGetValue();
void     displaySetIntensity(uint8_t intensity); // 0–15
void     displayTest();                          // all segments on
void     displayTestEnd();
void     displayBlank();
void     displayRestore();
void     displayShutdown();                      // low power
void     displayWake();
```

### Encoder

```cpp
void encoderPoll();          // called by k7scUpdate()
void encoderClearDelta();    // discard accumulated ISR clicks + clear inputState
```

### Buttons / NeoPixels

```cpp
void buttonSetPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void buttonsShow();
void buttonsClearAll();
```

---

## Lifecycle

```
Power on → BOOT_READY (library asserts INT) → master reads → DISABLE
DISABLED  : all dark, defaults, input suppressed
ACTIVE    : sketch running, pilot can interact
SLEEPING  : state frozen, input suppressed, no visual change
```

The sketch detects transitions by comparing `cmdState.lifecycle` to its previous value each loop.

---

## Packet Protocol

### Universal header (bytes 0–2, built by library)

| Byte | Content |
|---|---|
| 0 | Status: bits 1:0 = lifecycle, bit 2 = fault, bit 3 = data_changed |
| 1 | Module Type ID |
| 2 | Transaction counter (uint8, wraps 255→0) |

### Payload (bytes 3+, assembled by sketch)

Content and length are module-specific. The library treats the payload as opaque bytes.

### Identity packet (4 bytes, on GET_IDENTITY)

| Byte | Content |
|---|---|
| 0 | Module Type ID |
| 1 | Firmware major |
| 2 | Firmware minor |
| 3 | Capability flags |

---

## Encoder Acceleration

Step size is based on consecutive clicks in the same direction. Direction reversal resets the count — the first click of a new direction always steps by 1.

| Consecutive clicks | Step |
|---|---|
| 1–14 | 1 |
| 15–29 | 10 |
| 30–49 | 100 |
| 50+ | 1000 |

Thresholds and step sizes are `#ifndef`-guarded in `K7SC_Config.h` — override before `#include` in sketch.

---

## Tunable Constants

All `#ifndef`-guarded in `K7SC_Config.h`:

| Constant | Default | Description |
|---|---|---|
| `K7SC_ENC_MEDIUM_COUNT` | 15 | Clicks before step→10 |
| `K7SC_ENC_FAST_COUNT` | 30 | Clicks before step→100 |
| `K7SC_ENC_TURBO_COUNT` | 50 | Clicks before step→1000 |
| `K7SC_STEP_SLOW` | 1 | Step at slow speed |
| `K7SC_STEP_MEDIUM` | 10 | Step at medium speed |
| `K7SC_STEP_FAST` | 100 | Step at fast speed |
| `K7SC_STEP_TURBO` | 1000 | Step at turbo speed |
| `K7SC_BTN_DEBOUNCE_MS` | 30 | Button stable-time before edge accepted (ms) |
| `K7SC_MAX_INTENSITY` | 8 | MAX7219 startup intensity (0–15) |
| `K7SC_POLL_INTERVAL_MS` | 5 | Button and encoder poll rate (ms) |

---

## Changelog

### v2.0.0 (2026-04-28)

**Breaking changes — complete API redesign:**

- `ButtonConfig`, `BTN_MODE_CYCLE`, `BTN_MODE_TOGGLE`, `BTN_MODE_FLASH` removed — button state machines now belong in the sketch
- `k7scBegin()` no longer takes `btnConfigs` or `initialValue` — sketch manages these
- `encoderSetValue()`, `buttonsGetStateByte()`, `buttonsGetEncoderPress()` removed
- `k7scGetPendingEvents()`, `k7scI2CIsSleeping()`, `k7scSetINTEnabled()`, `k7scSuppressINT()` removed
- `k7scQueuePacket()` replaces library-managed packet building — sketch assembles payload

**New:**
- `K7SCCommandState` struct — all master commands visible to sketch via `cmdState`
- `K7SCInputState` struct — all hardware events visible to sketch via `inputState`
- `encoderClearDelta()` — atomically discard ISR-accumulated clicks
- `buttonSetPixel()` / `buttonsShow()` — direct NeoPixel control

**Architecture:**
- Library is now a pure hardware interface — no lifecycle management, no LED state machine, no INT gating
- Encoder reports raw signed delta; sketch implements acceleration
- Buttons report edges only; sketch implements all state logic
- INT assertion is entirely sketch-controlled via `k7scQueuePacket()`

### v1.1.0 (2026-04-27)
- Hardware corrections, interrupt-driven encoder, click-count acceleration, time-based debounce

---

## License

GNU General Public License v3.0 — https://www.gnu.org/licenses/gpl-3.0.html  
Code by J. Rostoker, Jeb's Controller Works.
