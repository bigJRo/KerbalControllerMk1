# KC-01-1880 Module Firmware

**Target board:** ATtiny816 (megaTinyCore, 20 MHz internal)  
**Module:** KC-01-1880 7-Segment Display Module  

Flash these sketches to the ATtiny816 on the KC-01-1880 module via UPDI.

---

## Sketches

### `kc01_1880_demo`
Standalone demo — no I2C master required.

- Encoder turns increment/decrement the display value with acceleration
- BTN_EN resets the display to 0
- NeoPixels cycle R→G→B once per second in the background

Use this to quickly verify the module works after flashing the library, or as a starting point for standalone (non-I2C) use.

---

### `KC01_1880_TestFirmware`
Full I2C test firmware — use paired with `KC01_1880_Tester` on the Xiao RA4M1.

Button configuration for the I2C test suite:
- BTN01: 3-state cycle (ENABLED → GREEN → AMBER → ENABLED)
- BTN02: toggle (ENABLED ↔ BLUE)
- BTN03: flash RED 300ms on press
- BTN_EN: reset display to 0

Exposes all button modes and encoder behaviour over I2C so the tester can validate the full protocol.

**Pair with:** `KC01_1880_TesterScripts/KC01_1880_Tester`

---

### `KC01_1880_ColorValidation`
NeoPixel colour validation firmware — use paired with `KC01_1880_ColorTester` on the Xiao RA4M1.

- Boots showing colour 0 (OFF) on all 3 pixels, display shows index
- Each BTN_EN press advances to the next colour in the KMC_* palette
- 25 colours total (see KerbalModuleCommon README for full palette)

**Pair with:** `KC01_1880_TesterScripts/KC01_1880_ColorTester`

---

### `KC01_1880_ConstructionTest`
Standalone post-assembly construction test — no I2C master or Xiao required.

Run this immediately after soldering a new board to verify wiring, components, and solder joints.

**Boot sequence (automatic):**
1. Display sweeps 0000 → 1111 → ... → 9999 (segment driver check)
2. Display shows unique patterns: `1234`, `5678`, `1357`, `2468` (digit line check)
3. Display shows `8888` at full brightness for 1 second (all-segments bulb test)
4. Each NeoPixel individually lit R → G → B (per-channel, per-pixel check)
5. All pixels white briefly (data chain check)

**Interactive mode (after boot):**

| Input | Response | Verifies |
|---|---|---|
| Encoder CW/CCW | Display counts up/down | ENC_A, ENC_B, interrupt wiring |
| BTN_EN press | Display resets to 0 | Encoder pushbutton, PB3 |
| BTN01 held | Pixel 0 → white | BTN01 → LED2 mapping |
| BTN02 held | Pixel 1 → white | BTN02 → LED3 mapping |
| BTN03 held | Pixel 2 → white | BTN03 → LED4 mapping |
| Background | All pixels cycle R→G→B | All 3 channels, all 3 pixels |

---

### `KC01_1880_MaxPowerTest`
Maximum power draw test — for current measurement and power budgeting.

Holds the module at worst-case current draw:
- All 4 digits showing `8888` (all segments lit) at MAX7219 intensity 15
- All 3 NeoPixels at full white (255, 255, 255) at maximum brightness

Connect an ammeter in series with VCC and record the reading. No interaction required — the sketch holds this state indefinitely.

---

## IDE Settings

| Setting | Value |
|---|---|
| Board | ATtiny816 (megaTinyCore) |
| Clock | 20 MHz internal |
| tinyNeoPixel Port | Port C |
| Programmer | serialUPDI |

## Required Libraries

- KerbalModuleCommon v1.1.0
- Kerbal7SegmentCore v1.1.0
- tinyNeoPixel_Static (bundled with megaTinyCore)
