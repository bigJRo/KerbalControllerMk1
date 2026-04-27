# KC-01-1880 Tester Scripts

**Target board:** Xiao RA4M1  
**Purpose:** I2C test controller for KC-01-1880 module validation  

Flash these sketches to the Xiao RA4M1 test board. The Xiao acts as the I2C controller and drives the KC-01-1880 module through its full test protocol.

---

## Wiring (Xiao RA4M1 → KC-01-1880 P1 connector)

| Xiao Pin | KC-01-1882 P1 | Signal |
|---|---|---|
| D4 | Pin 13 | SDA |
| D5 | Pin 14 | SCL |
| D6 | Pin 15 | INT (active LOW, 10kΩ pull-up to 3.3V) |
| 3.3V | Pin 4 | VCC |
| GND | Pin 3 | GND |

---

## Sketches

### `KC01_1880_Tester`
Full 15-test I2C validation suite. Use paired with `KC01_1880_TestFirmware` on the module.

Open Serial Monitor at **115200 baud** to interact.

**Automated tests (run with 'A'):**

| Test | Description |
|---|---|
| T00 | I2C bus scan — confirms module found at 0x2A |
| T01 | GET_IDENTITY — verifies Type ID, firmware version, capability flags |
| T03 | SET_VALUE — writes and reads back 4 values (1234, 9999, 0, 42) |
| T04 | Brightness sweep — steps display and NeoPixels through 5 levels |
| T06 | SLEEP / WAKE — visual confirm display and LEDs off/restored |
| T07 | RESET — confirms display returns to 0, NeoPixels restored |
| T12 | DISABLE / ENABLE — visual confirm display and LEDs off/restored |
| T13 | ACK_FAULT — confirms module still alive after command |
| T14 | INT pin — idle HIGH, asserts on value change, clears after read |

**Manual tests (run individually):**

| Test | Description | Action required |
|---|---|---|
| T02 | Data packet read | Turn encoder a few clicks |
| T05 | BULB_TEST | Visual confirm all segments + NeoPixels white |
| T08 | BTN01 CYCLE | Press BTN01 three times — watch LED: dim→green→amber→dim |
| T09 | BTN02 TOGGLE | Press BTN02 twice — watch LED: dim→blue→dim |
| T10 | BTN03 FLASH | Press BTN03 once — watch LED: flash red→dim |
| T11 | BTN_EN | Press encoder button — display resets to 0 |

**Pair with:** `KC01_1880_ModuleFirmware/KC01_1880_TestFirmware`

---

### `KC01_1880_ColorTester`
NeoPixel colour validation logger. Use paired with `KC01_1880_ColorValidation` on the module.

Open Serial Monitor at **115200 baud** to interact.

**Procedure:**
1. Press **Enter** on the serial monitor to begin
2. Module boots showing colour 0 (OFF) — answer Y/N
3. For each subsequent colour: press **BTN_EN on the module** to advance, the tester detects the INT and prompts automatically
4. Enter **Y** if the colour looks correct, **N** if it needs adjustment
5. For N responses, type a description of what you see and press Enter

25 colours total. Results are logged to serial with PASS/FAIL per colour and a final summary.

**Pair with:** `KC01_1880_ModuleFirmware/KC01_1880_ColorValidation`

---

## IDE Settings

| Setting | Value |
|---|---|
| Board | Xiao RA4M1 (Arduino framework) |
| Port | whichever COM port the Xiao is on |

## Required Libraries

- KerbalModuleCommon v1.1.0
- Wire (bundled with Arduino)
