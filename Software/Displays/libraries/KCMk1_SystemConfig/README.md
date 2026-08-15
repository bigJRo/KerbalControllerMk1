# KCMk1_SystemConfig

**Kerbal Controller Mk1 — Shared System Configuration** · v1.1.0
Shared hardware pin map and cross-panel thresholds for all KCMk1 display panels.
Part of the KCMk1 controller system.

---

## Overview

KCMk1_SystemConfig is a header-only library containing the `#define` constants shared across the KCMk1 display-panel sketches (Annunciator, ResourceDisp, InfoDisp). It has no source file, no variables, and no functions — it is a pure configuration header.

It is the **authoritative pin map** for the hardware rev 2 display carrier (schematic KC-01-1911/1912): a Teensy 4.1 driving a 7" 1024×600 IPS TFT (BuyDisplay ER-TFT070A2-6-5633). The physical display controller is the **LT7683**, register-compatible with the RA8876; the firmware drives it over the RA8876's 16-bit 8080 parallel bus via the `wwatson4506/TeensyRA8876-8080` FlexIO3 driver. ("RA8876" therefore appears in driver/library/class names; the hardware part is the LT7683.)

Installing it as a library means every sketch includes it with angle brackets and there is a single file to edit when a shared constant changes.

---

## Installation

Place the `KCMk1_SystemConfig` folder in your Arduino `libraries/` directory alongside `KerbalDisplayCommon` and `KerbalDisplayAudio`. Each sketch's main header already includes it:

```cpp
#include <KCMk1_SystemConfig.h>
```

---

## Contents

### Display (hardware rev 2 carrier, KC-01-1912)

| Group | Constants |
|-------|-----------|
| Resolution | `KCM_SCREEN_W` (1024), `KCM_SCREEN_H` (600) |
| Data bus (FlexIO3, driver-owned) | `KCM_TFT_DB0`..`KCM_TFT_DB15` (pins 19,18,14,15,40,41,17,16,22,23,20,21,38,39,26,27) |
| Control lines (plain GPIO) | `KCM_TFT_CS` (34), `KCM_TFT_RESET` (35), `KCM_TFT_WR` (36), `KCM_TFT_RD` (37), `KCM_TFT_RS` (33), `KCM_TFT_WAIT` (32), `KCM_TFT_INT` (31) |
| Backlight | `KCM_TFT_BL` (9), `KCM_BL_BRIGHTNESS_PCT` (70) |
| Timing / bus | `KCM_FRAME_PERIOD_US` (20000), `KCM_TFT_BUS_SPEED_MHZ` (20), `KCM_TFT_BUS_WIDTH` (16) |

The data bus, `/WR`, and `/RD` are owned by the FlexIO3 driver — those defines are documentation. Only `/CS`, `RS`, and `/RESET` are passed to the driver as plain GPIO.

### Touch, audio, SD, serial

| Group | Constants |
|-------|-----------|
| Capacitive touch (FT5316, software I2C) | `KCM_CTP_SCL` (4), `KCM_CTP_SDA` (5), `KCM_CTP_RST` (3), `KCM_CTP_INT` (6), `KCM_CTP_I2C_ADDR` (0x38) |
| Audio | `KCM_AUDIO_TONE_PIN` (29, → PAM8302A amp), `KCM_AUDIO_EN_PIN` (30, amp `/SD` enable), `KCM_AUDIO_BUSY_PIN` (11, DFPlayer BUSY), `KCM_DFPLAYER_SERIAL` (Serial2), `KCM_DFPLAYER_BAUD` (9600) |
| SD card | `KCM_SD_CS` (`BUILTIN_SDCARD`, SDIO) |
| Serial | `KCM_SERIAL_BAUD` (115200) |

### Module / slave I2C bus (Wire2, to the master controller)

| Group | Constants |
|-------|-----------|
| Bus / pins | `KCM_I2C_BUS` (Wire2, SCL2=24 / SDA2=25), `KCM_I2C_INT_PIN` (0), `KCM_I2C_RST_PIN` (1) |
| Slave addresses | `KCM_I2C_ADDR_ANNUNCIATOR` (0x10), `KCM_I2C_ADDR_RESDISP` (0x11), `KCM_I2C_ADDR_INFODISP` (0x12), `KCM_I2C_ADDR_INFODISP_2` (0x13), `KCM_I2C_ADDR_SYSINFODISP` (0x14 — **future work**) |
| Sync / framing bytes | `KCM_I2C_SYNC_ANNUNCIATOR` (0xAC), `KCM_I2C_SYNC_RESDISP` (0xAD), `KCM_I2C_SYNC_INFODISP` (0xAE — shared by Info Display 1 & 2) |

Info Display 1 (0x12) and Info Display 2 (0x13) run the **same** InfoDisp firmware; the target board is chosen at compile time by `INFO_DISP_UNIT` in the InfoDisp `AAA_Config.ino`. The System Info Display (0x14) is separate hardware and is not yet coded — see `Documents/Developer/Hardware_Reference.md`.

### Touch filter and operating-mode default

| Group | Constants |
|-------|-----------|
| Touch filter | `KCM_TOUCH_DEBOUNCE_MS`, `KCM_TOUCH_DEAD_ZONE_PX`, `KCM_TOUCH_JITTER_MAX_PX`, `KCM_TOUCH_TITLE_DEBOUNCE_MS` |
| Display rotation default | `KCM_DEFAULT_DISPLAY_ROTATION` (0) — default argument to `kcmDisplayBegin()` |

Each sketch owns its own `debugMode` / `demoMode` flags in `AAA_Config.ino` (all default to `false` for production); there are no shared debug/demo default macros.

### Cross-panel aligned thresholds

These must stay identical between the Annunciator C&W logic and the InfoDisp display thresholds. Edit here only — the local constants in each sketch are aliases to these values.

| Constant | Value | Aligned pair |
|----------|-------|--------------|
| `KCM_GROUND_PROX_S` | 10.0 s | CW_GROUND_PROX_S / LNDG_TGRND_ALARM_S |
| `KCM_HIGH_G_ALARM_POS` | 9.0 g | CW_HIGH_G_ALARM / G_ALARM_POS (red) |
| `KCM_HIGH_G_ALARM_NEG` | −5.0 g | CW_HIGH_G_WARN / G_ALARM_NEG (red) |
| `KCM_HIGH_G_WARN_POS` | 4.0 g | G_WARN_POS (InfoDisp yellow tier) |
| `KCM_HIGH_G_WARN_NEG` | −2.0 g | G_WARN_NEG (InfoDisp yellow tier) |
| `KCM_LOW_DV_MS` | 150.0 m/s | CW_LOW_DV_MS / DV_STG_ALARM_MS |
| `KCM_LOW_BURN_S` | 60.0 s | CW_LOW_BURN_S / LNCH_BURNTIME_ALARM_S |
| `KCM_TEMP_ALARM_PCT` | 90 % | CW_HIGH_TEMP core/skin temp % of limit |
| `KCM_EC_LOW_ALARM_FRAC` | 0.05 | CW_BUS_VOLTAGE electric-charge fraction |
| `KCM_RES_LOW_WARN_FRAC` | 0.20 | CW_PROP_LOW / CW_RCS_LOW yellow tier |
| `KCM_CHUTE_MAIN_MAX_Q` | 38300 Pa | CW_CHUTE_ENV main deploy-q limit (~250 m/s @ Kerbin SL) |
| `KCM_CHUTE_DROGUE_MAX_Q` | 153000 Pa | CW_CHUTE_ENV drogue deploy-q limit (~500 m/s @ Kerbin SL) |

The G **alarm** limits are the Annunciator's single HIGH-G warning trip points; the G **warn** limits add InfoDisp's yellow caution tier on the two-tier G gauges.

---

## Version History

| Version | Notes |
|---------|-------|
| **1.1.0** | Documented the full hardware rev 2 pin map (display data/control lines, touch, audio, SD, Wire2 module bus) that the header actually owns. Added Info Display 2 (`KCM_I2C_ADDR_INFODISP_2` = 0x13) and reserved the System Info Display (`KCM_I2C_ADDR_SYSINFODISP` = 0x14, future work). Removed the dead, misleading `KCM_DEFAULT_DEBUG_MODE` / `KCM_DEFAULT_DEMO_MODE` macros (unused, defaulted to `true` while every sketch uses `false`). Standardized chip naming to LT7683 (RA8876-compatible). |
| **1.0.0** | Initial release. Extracted from inline sketch definitions during KCMk1 implementation Phase 3. |

---

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
