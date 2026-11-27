# KBC_DevBoard_Target

**Version:** 1.2.0  
**Date:** 2026-11-27  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  
**Hardware:** KC-01-9101 Module Development Board v1.0 (ATtiny816)  
**Library:** KerbalButtonCore v2.5.1 or later  
**Tester:** `../KBC_Tester_Controller` on the KC-01-9001 Module Tester (XIAO SAMD21)  

---

## Overview

The I2C target half of the KerbalButtonCore library test suite. It is a plain KerbalButtonCore module with no test logic of its own: `KBC_Tester_Controller` drives it over real I2C and checks the library's responses.

| Parameter | Value |
|---|---|
| I2C Address | `0x20` |
| Module Type ID | `0x01` (KBC_TYPE_UI_CONTROL) |
| Capability Flags | `0x01` (KBC_CAP_EXTENDED_STATES) |
| Identity | 8 bytes (I2C spec §6): the KerbalButtonCore version, 2.5.1, at bytes 1/2/4 and this sketch's own version, 1.2.0, at bytes 5–7 (`SKETCH_VERSION_*` in the sketch, passed with `kbc.setSketchVersion()`) |
| Button colours | One distinct colour per RGB button (indices 0–11), to check NeoPixel addressing by eye |

The tester's identity test expects exactly this type, these capability flags and library version, and a sketch version that is not 0xFF (unknown).

## Dev board pin differences

The sketch replaces three KerbalButtonCore pin settings for the KC-01-9101 board: INT on PA4 (PA1 on production), the NeoPixel data on PB4 (PA4 on production) and GRB colour order for the SK6812mini LEDs (RGB for the WS2811 on production). `KBC_Config.h` guards these with `#ifndef` (KerbalButtonCore 2.5.1), so the sketch defines them before it includes `KerbalButtonCore.h`. Remove the overrides to build for production hardware.

## IDE settings

| Setting | Value |
|---|---|
| Board | ATtiny816 (megaTinyCore) |
| Clock | 10 MHz internal |
| tinyNeoPixel Port | **Port B** (the dev board's NeoPixel is on PB4) |
| millis/micros | Enabled (default timer) |

## Version History

| Version | Date | Notes |
|---|---|---|
| 1.0.0 | 2026-04-09 | Initial release |
| 1.1.0 | 2026-11-26 | Ported to the current KerbalButtonCore API (2.5.0); 1.0.0 no longer compiled. The type and capability flags go to the constructor, `begin()` takes no arguments, and the sketch's own version, 1.1.0, is reported at identity bytes 5–7 (`SKETCH_VERSION_*` constants, passed with `kbc.setSketchVersion()` in `setup()`), so the tester's "Sketch version reported" check passes. The colour array is now `KBC_BUTTON_COUNT` (16) entries, as the library reads. The INT, NeoPixel and colour-order overrides now take effect (before, `KBC_Config.h` replaced them with the production values); the discrete-LED overrides were removed with the library's discrete-LED driver. |
| 1.2.0 | 2026-11-27 | The INT, NeoPixel and colour-order overrides are plain `#define`s before the include, since KerbalButtonCore 2.5.1 guards them; the `KBC_Config.h` include and the `#undef`s are gone. Needs KerbalButtonCore 2.5.1 or later. |
