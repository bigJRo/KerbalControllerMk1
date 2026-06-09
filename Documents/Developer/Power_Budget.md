# KCMk1 Power Budget

**Organization:** Jeb's Controller Works  
**Author:** J. Rostoker  
**Version:** 1.2  
**Date:** 2026-06-08  
**Location:** `docs/developer/Power_Budget.md`

*12V supply: 10A rated · MPM3610 converter efficiency: ~88% · All currents in mA*

---

## Revision History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | — | Initial release — per-module and per-panel power budget, system rollup, supply headroom. |
| 1.1 | 2026-06-06 | Main display carriers updated from Teensy 4.0 + RA8875 5" 800×480 to Teensy 4.1 + LT7683 7" 1024×600 (ER-TFT070A2-6-5633): 450/500 mA → 480/520 mA @ 5V (227/246 mA @ 12V input). Recomputed A1, B1, A+B subtotal/total, A+B+C total, and supply headroom. Added per-carrier MPM3610 headroom note. Panel C displays left at the 5" assumption pending decision. |
| 1.2 | 2026-06-08 | Small display carriers (Sys Info Display, Indicator Module) updated from XIAO RA4M1 + 1.9" ST7789 320×170 to XIAO RP2350 + 2.0" ST7789 240×320 (ER-TFTM020-2) + microSD. Datasheet-confirmed display draw (≤90 mA) removes the prior ±50% estimate; per-line figures revised to 130 mA typ / 280 mA peak @ 5V-equivalent (peak includes microSD write spike). Recomputed A2, B2, A+B and A+B+C totals and headroom. |

---

## 1. Per-Module Power Budget

All figures derived from measured data and component datasheets. Panel C is a potential growth scenario and is not part of the current build.

\* 12V input = (5V load ÷ 0.88) × (5/12) + (3.3V load ÷ 0.88) × (3.3/12) + 12V direct load

### Panel A1

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| Annunciator | 480 | 520 | — | — | — | — | 227 | 246 | Teensy 4.1 + LT7683 7" 1024×600 TFT |
| Info 1 Display | 480 | 520 | — | — | — | — | 227 | 246 | Teensy 4.1 + LT7683 7" 1024×600 TFT |
| GPWS Module | 229 | 465 | 6 | 12 | — | — | 112 | 228 | ATtiny816 + 7-seg + 3× NeoPixel |
| **A1 Total** | **1,189** | **1,505** | **6** | **12** | **—** | **—** | **566** | **720** | 0.57A / 6.8W typ — 0.72A / 8.6W peak |

### Panel A2

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| UI Ctrl | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M + Ctrl Mode toggle |
| Action Ctrl | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| Func. Ctrl | 415 | 892 | — | — | — | — | 197 | 424 | ATtiny816 + 12× PB26-13M + 8× switch inputs |
| Throttle | 115 | 165 | 5 | 10 | 550 | 1,100 | 608 | 1,147 | ATtiny816 + 5× LED + L293D + motorized pot |
| Translation | 50 | 112 | 10 | 20 | — | — | 27 | 57 | ATtiny816 + joystick |
| Sys Info Display | 130 | 280 | — | — | — | — | 62 | 134 | XIAO RP2350 + ST7789 2.0" 240×320 + microSD (peak incl. SD write spike) |
| **A2 Total** | **1,520** | **3,213** | **15** | **30** | **550** | **1,100** | **1,278** | **2,600** | 1.28A / 15.3W typ — 2.60A / 31.2W peak |

### Panel B1

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| Info 2 Display | 480 | 520 | — | — | — | — | 227 | 246 | Teensy 4.1 + LT7683 7" 1024×600 TFT |
| Resource Display | 480 | 520 | — | — | — | — | 227 | 246 | Teensy 4.1 + LT7683 7" 1024×600 TFT |
| Dual Encoder | 20 | 30 | — | — | — | — | 10 | 14 | ATtiny816 + 2× rotary encoder |
| Abort Button | 15 | 25 | — | — | — | — | 7 | 12 | Single LED button |
| **B1 Total** | **995** | **1,095** | **—** | **—** | **—** | **—** | **471** | **518** | 0.47A / 5.7W typ — 0.52A / 6.2W peak |

### Panel B2

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| Stability Ctrl | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| Vehicle Ctrl | 415 | 892 | — | — | — | — | 197 | 424 | ATtiny816 + 12× PB26-13M + 8× switch inputs |
| Time Ctrl | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| Time Ctrl Dial | 204 | 415 | 6 | 12 | — | — | 100 | 201 | ATtiny816 + 7-seg + encoder |
| Auxiliary Ctrl | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| Rotation | 50 | 112 | 10 | 20 | — | — | 27 | 57 | ATtiny816 + joystick |
| Stage Button | 15 | 25 | — | — | — | — | 7 | 12 | Single LED button |
| Indicator Module | 130 | 280 | — | — | — | — | 62 | 134 | XIAO RP2350 + ST7789 2.0" 240×320 + microSD (peak incl. SD write spike) |
| **B2 Total** | **2,029** | **4,370** | **16** | **32** | **—** | **—** | **969** | **2,085** | 0.97A / 11.6W typ — 2.09A / 25.0W peak |

### Panel C — Potential Growth

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| C Display 1 | 450 | 500 | — | — | — | — | 214 | 238 | 5" Teensy/RA8875 assumed — revisit if C moves to 7" LT7683 |
| C Display 2 | 450 | 500 | — | — | — | — | 214 | 238 | 5" Teensy/RA8875 assumed — revisit if C moves to 7" LT7683 |
| C Button 1 | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| C Button 2 | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| **C Total** | **1,710** | **2,764** | **—** | **—** | **—** | **—** | **813** | **1,314** | 0.81A / 9.8W typ — 1.31A / 15.7W peak |

> † ST7789 2.0" display carriers (Sys Info Display, Indicator Module): XIAO RP2350 + ER-TFTM020-2 (≤90 mA datasheet) + microSD. Typical ~130 mA, peak ~280 mA @ 5V-equivalent including a transient microSD write spike (~100 mA). All loads on the carrier 3.3V (AP2112K); the figure is reflected to the 5V column through the LDO/buck. Replaces the prior ±50% estimate.

> ‡ Throttle motor peak assumes L293D stall at 500 mA. Actual stall current depends on motorized pot specification.

> § Peak assumes all PB26-13M NeoPixel buttons at full white simultaneously — a conservative worst case.

> ¶ Panel C displays are still shown at the 5" Teensy/RA8875 figures (450/500 mA). If C also moves to the 7" LT7683 carrier, each C display becomes 480/520 mA @ 5V (227/246 @ 12V input), raising the C total to ~1,770/2,804 mA @ 5V and ~838/1,330 mA @ 12V input. Revise when the C-group decision is made.

---

## 2. System Rollup by Panel

| Panel | 5V typ (mA) | 5V peak (mA) | 12V direct typ (mA) | 12V direct peak (mA) | 12V input typ (mA) | 12V input peak (mA) |
|-------|-------------|--------------|--------------------|--------------------|-------------------|-------------------|
| A1 | 1,189 | 1,505 | — | — | 566 | 720 |
| A2 | 1,520 | 3,213 | 550 | 1,100 | 1,278 | 2,600 |
| B1 | 995 | 1,095 | — | — | 471 | 518 |
| B2 | 2,029 | 4,370 | — | — | 969 | 2,085 |
| **A+B subtotal** | **5,733** | **10,183** | **550** | **1,100** | **3,284** | **5,923** |
| Control overhead | — | — | — | — | 50 | 50 |
| **A+B TOTAL** | **5,733** | **10,183** | **550** | **1,100** | **3,334** | **5,973** |
| C (growth) | 1,710 | 2,764 | — | — | 813 | 1,314 |
| **A+B+C TOTAL** | **7,443** | **12,947** | **550** | **1,100** | **4,147** | **7,287** |

---

## 3. Supply Headroom

| Scenario | Typical (A) | Typical power (W) | Peak (A) | Peak power (W) |
|----------|-------------|-------------------|---------|----------------|
| A+B only | 3.33 | 40.0 | 5.97 | 71.7 |
| A+B+C growth | 4.15 | 49.8 | 7.29 | 87.4 |
| 12V supply rated | 10.0 | 120 | 10.0 | 120 |
| **Headroom A+B** | **6.67** | **80.0** | **4.03** | **48.3** |
| **Headroom A+B+C** | **5.85** | **70.2** | **2.71** | **32.6** |

> Peak headroom with A+B only (4.03A) and with the C group (2.71A) is conservative — simultaneous full NeoPixel peak across all modules is an unlikely operating condition.

---

## 4. Key Assumptions & Open Items

- All PB26-13M NeoPixel button modules: typical = mixed brightness ~25 mA/pixel; peak = full white ~56 mA/pixel.
- 7" TFT displays (Teensy 4.1 + LT7683 1024×600, ER-TFT070A2-6-5633): 480 mA typical / 520 mA peak @ 5V (datasheet §4.6 max 520 mA @ 5V; typical estimated with backlight below full). Panel VDD on the 5V rail only — never 3.3V (730 mA @ 3.3V exceeds the carrier AP2112K). VDDIO 3.3V; Teensy 4.1 interfaces directly, no level shifters.
- **Per-carrier MPM3610 headroom (5V):** each display carrier sources its full 5V load from one local MPM3610 (1.2 A rated): ~660 mA typ / ~760 mA peak (panel 480/520 + Teensy 4.1 ~100/120 + 3.3V logic via AP2112K ~80/120 reflected to the 5V input). Peak headroom ~440 mA (37%). One MPM3610 per carrier is adequate; do not share one MPM3610 across two displays. Continuous load is higher than the former 5" carriers (~760 mA vs ~550 mA) — size MPM3610 thermal copper accordingly.
- ST7789 2.0" display carriers (XIAO RP2350 + ER-TFTM020-2 + microSD): ~130 mA typical / ~280 mA peak (5V-equivalent). Display ≤90 mA (datasheet §4.3); peak includes a transient microSD write spike. Display and microSD powered from the carrier AP2112K 3.3V rail; XIAO fed from 5V via an SS14 Schottky.
- L293D H-bridge (Throttle): 60 mA logic quiescent on 5V; 550 mA typical / 1,100 mA peak on 12V motor supply.
- GPWS and Time Ctrl Dial 7-segment displays assumed always-on.
- Control circuit overhead fixed at 50 mA on 12V (master controller, soft-latch, miscellaneous).
- Switch groups (Switch Group 1 on A2 via Func. Ctrl, Switch Group 2 on B2 via Vehicle Ctrl): passive inputs only, negligible power addition.
- Panel C assumes 2× 5" Teensy/RA8875 displays (pending decision on whether C moves to 7" LT7683) and 2× standard 12-button PB26-13M modules — revise if hardware differs.

---

## Related Documents

- `docs/developer/Hardware_Reference.md` — power rail architecture, supply specification
- `docs/developer/Module_Interface_Reference.md` — per-module hardware specifications
