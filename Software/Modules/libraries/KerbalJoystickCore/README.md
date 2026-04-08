# KerbalJoystickCore

**Version:** 1.0.0  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  
**Hardware:** KC-01-1831/1832 Joystick Module v1.0  
**Target MCU:** ATtiny816 (megaTinyCore)  

---

## Overview

KerbalJoystickCore is the Arduino library for Kerbal Controller Mk1 analog joystick modules. Each module runs on an ATtiny816 and acts as an I2C target device, reporting joystick axis data and button state to the system controller.

The library manages:

- 3 analog axes via ATtiny816 internal ADC (10-bit, 0-1023)
- Startup calibration — reads center position at boot
- Deadzone filtering — suppresses output within ±KJC_DEADZONE counts of center
- Change threshold filtering — suppresses output when movement < KJC_CHANGE_THRESHOLD counts
- Split-range INT16 scaling via Arduino map()
- Option E hybrid INT assertion strategy
- 2 NeoPixel RGB buttons (WS2811, tinyNeoPixel_Static) on PC1
- 1 joystick pushbutton (direct GPIO, no LED)
- I2C target communication (Kerbal Controller Mk1 I2C protocol)

This is **not** a KerbalButtonCore (KBC) module. It shares the same I2C wire protocol but uses a device-specific 8-byte data packet.

---

## Hardware

**PCB:** KC-01-1831 (schematic) / KC-01-1832 (board)  
**MCU:** ATtiny816-MNR

### Pin Assignment

| ATtiny816 Pin | Net Name | Function |
|---|---|---|
| PA1 (pin 20) | INT | Interrupt output (active low) |
| PA6 (pin 7) | BUTTON_JOY | Joystick pushbutton (active high) |
| PA7 (pin 8) | AXIS3 | Analog input — Z rotation |
| PB0 (pin 14) | SCL | I2C clock |
| PB1 (pin 13) | SDA | I2C data |
| PB3 (pin 11) | BUTTON02 | NeoPixel button 2 |
| PB4 (pin 10) | AXIS1 | Analog input — X axis |
| PB5 (pin 9) | AXIS2 | Analog input — Y axis |
| PC0 (pin 15) | BUTTON01 | NeoPixel button 1 |
| PC1 (pin 16) | NEOPIX_CMD | NeoPixel data output |

### Extension Connector (U4)

The 6-pin screw terminal exposes AXIS1, AXIS2, AXIS3, BUTTON_JOY, VCC, and GND for the physical joystick connection.

---

## I2C Protocol

**Address:** Set per sketch (0x28 = Rotation, 0x29 = Translation)  
**Speed:** 400 kHz recommended  
**Packet format:** Device-specific, 8 bytes (see Section 9.2 of protocol spec)

### Data Packet (module → controller, 8 bytes)

```
Byte 0:   Button state  (bit0=BTN_JOY, bit1=BTN01, bit2=BTN02)
Byte 1:   Change mask   (same bit layout)
Byte 2-3: AXIS1         (int16, big-endian, -32768 to +32767)
Byte 4-5: AXIS2         (int16, big-endian, -32768 to +32767)
Byte 6-7: AXIS3         (int16, big-endian, -32768 to +32767)
```

### INT Assertion — Option E Hybrid Strategy

| Source | Behavior |
|---|---|
| Button change | Immediate INT, no throttling |
| Axis outside deadzone + above change threshold | INT asserted if quiet period elapsed |
| Joystick at rest (within deadzone) | No INT |
| Joystick held steady (below change threshold) | No INT |

---

## ADC Processing Pipeline

All threshold comparisons happen in raw ADC counts before map() is called:

```
1. Read raw ADC (10-bit, 0-1023)
2. Deadzone: if |raw - center| <= KJC_DEADZONE → output 0, suppress if already 0
3. Change threshold: if |raw - lastSentRaw| <= KJC_CHANGE_THRESHOLD → suppress
4. Split map to INT16:
     raw >= center: map(raw, center, 1023,  0,      32767)
     raw <  center: map(raw, 0,      center, -32768, 0    )
5. Store lastSentRaw and return scaled INT16
```

---

## Default Constants

| Constant | Default | Description |
|---|---|---|
| `KJC_DEADZONE` | 32 | Deadzone radius in ADC counts (~3.1%) |
| `KJC_CHANGE_THRESHOLD` | 8 | Minimum change in ADC counts to report (~0.8%) |
| `KJC_QUIET_PERIOD_MS` | 10 | Minimum ms between axis-driven INT assertions |
| `KJC_CALIBRATION_SAMPLES` | 16 | ADC samples averaged at startup |
| `KJC_POLL_INTERVAL_MS` | 5 | ADC and button poll rate |
| `KJC_ENABLED_BRIGHTNESS` | 32 | ENABLED state dim white brightness |

All `#ifndef`-guarded constants can be overridden by defining them before the include in the sketch.

---

## Quick Start

```cpp
#include <Wire.h>
#include <KerbalJoystickCore.h>

const RGBColor activeColors[KJC_NEO_COUNT] = {
    KJC_GREEN,   // BTN01
    KJC_AMBER,   // BTN02
};

void setup() {
    Wire.begin(0x28);
    kjcBegin(0x09, KJC_CAP_JOYSTICK, activeColors);
}

void loop() {
    kjcUpdate();
}
```

Do not touch the joystick during the first ~80ms after power-on. Calibration reads center position at startup.

---

## Module Registry

| Module | Type ID | I2C Address | BTN01 | BTN02 |
|---|---|---|---|---|
| Joystick Rotation | `0x09` | `0x28` | Reset Trim — GREEN | Trim Set — AMBER |
| Joystick Translation | `0x0A` | `0x29` | Cycle Camera — MAGENTA | Camera Reset — GREEN |

---

## IDE Settings

| Setting | Value |
|---|---|
| Board | ATtiny816 (megaTinyCore) |
| Clock | 10 MHz or higher |
| tinyNeoPixel Port | **Port C** — NeoPixel is on PC1 |
| Programmer | jtag2updi or SerialUPDI |

---

## File Structure

```
KerbalJoystickCore/
├── README.md
├── library.properties
├── src/
│   ├── KerbalJoystickCore.h/.cpp  — top-level include and update loop
│   ├── KJC_Config.h               — pins, constants, thresholds
│   ├── KJC_Colors.h               — RGB palette and helpers
│   ├── KJC_ADC.h/.cpp             — analog axis reading and processing
│   ├── KJC_Buttons.h/.cpp         — button input and NeoPixel LED control
│   └── KJC_I2C.h/.cpp             — I2C target and INT management
└── examples/
    ├── BasicJoystickModule/       — minimal sketch template
    └── DiagnosticDump/            — serial debug and hardware verification
```

---

## License

Licensed under the GNU General Public License v3.0 (GPL-3.0).  
See https://www.gnu.org/licenses/gpl-3.0.html

Code written by J. Rostoker for Jeb's Controller Works.
