# KCMk1_SystemConfig

**Kerbal Controller Mk1 — Shared System Configuration** · v1.0.0
Shared hardware constants and cross-panel thresholds for all KCMk1 display panels.
Part of the KCMk1 controller system.

---

## Overview

KCMk1_SystemConfig is a header-only library containing all `#define` constants that are shared across the KCMk1 display panel sketches (Annunciator, ResourceDisp, InfoDisp). It has no source file, no variables, and no functions — it is a pure configuration header.

Installing it as a library means all three sketches can include it with angle brackets and there is a single file to edit when a shared constant needs to change.

---

## Installation

Place the `KCMk1_SystemConfig` folder in your Arduino `libraries/` directory alongside `KerbalDisplayCommon` and `KerbalDisplayAudio`. Each sketch's main header already includes it:

```cpp
#include <KCMk1_SystemConfig.h>
```

---

## Contents

| Group | Constants |
|-------|-----------|
| Display | `KCM_SCREEN_W`, `KCM_SCREEN_H` |
| I2C addressing | `KCM_I2C_INT_PIN`, `KCM_I2C_ADDR_ANNUNCIATOR` (0x10), `KCM_I2C_ADDR_RESDISP` (0x11), `KCM_I2C_ADDR_INFODISP` (0x12) |
| I2C sync bytes | `KCM_I2C_SYNC_ANNUNCIATOR` (0xAC), `KCM_I2C_SYNC_RESDISP` (0xAD), `KCM_I2C_SYNC_INFODISP` (0xAE) |
| Serial | `KCM_SERIAL_BAUD` (115200) |
| Touch filter | `KCM_TOUCH_DEBOUNCE_MS`, `KCM_TOUCH_DEAD_ZONE_PX`, `KCM_TOUCH_JITTER_MAX_PX`, `KCM_TOUCH_TITLE_DEBOUNCE_MS` |
| Default modes | `KCM_DEFAULT_DEBUG_MODE`, `KCM_DEFAULT_DEMO_MODE`, `KCM_DEFAULT_DISPLAY_ROTATION` |
| Cross-panel thresholds | `KCM_GROUND_PROX_S`, `KCM_HIGH_G_ALARM_POS`, `KCM_HIGH_G_ALARM_NEG`, `KCM_LOW_DV_MS`, `KCM_LOW_BURN_S` |

The five cross-panel thresholds must stay identical between the Annunciator C&W logic and the InfoDisp display thresholds. Edit here only — the local constants in each sketch are aliases to these values.

---

## Version History

| Version | Notes |
|---------|-------|
| **1.0.0** | Initial release. Extracted from inline sketch definitions during KCMk1 implementation Phase 3. |

---

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
