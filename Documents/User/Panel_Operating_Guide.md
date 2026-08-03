# KCMk1 Panel Operating Guide

**Document type:** User  
**Location:** `Documents/User/Panel_Operating_Guide.md`  
**Version:** 0.1 (stub)  
**Status:** Placeholder — to be completed when hardware build is finalized

---

## Contents (planned)

1. Panel overview (per Hardware Reference §8.3)
   - Panel A1 — Annunciator (Caution & Warning) + Info Display 1, power button
   - Panel A2 — UI control, function control, action control, throttle, translation joystick, GPWS input, Sys Info Display
   - Panel B1 — Resource Display + Info Display 2, dual encoder
   - Panel B2 — stability control, vehicle control, time control, auxiliary control, rotation joystick, pre-warp time, staging button, control-mode select switch
2. Button LED states (the seven `KMC_LED_*` states — see I2C Protocol Specification §10.2)
   - Off (system unavailable)
   - Enabled (dim warm-white — system available/backlit)
   - Active (full-brightness colored — system engaged)
   - Warning (amber, 500 ms flash)
   - Alert (red, 150 ms flash)
   - Armed (cyan, full-brightness static)
   - Partial Deploy (amber, full-brightness static)
3. Panel A1 controls
4. Panel A2 controls
5. Panel B1 controls
6. Panel B2 controls
7. Control mode switching (Spacecraft / Aircraft / Rover / EVA)
8. Custom action groups — what each does
9. Joystick axis reference
10. Encoder functions

---

> **Note:** This document is the user-facing companion to the developer reference `Documents/Developer/Module_UI_Reference.md`. It covers the same controls but describes what they do in KSP terms rather than technical implementation terms. To be written once the physical controller build is complete.
