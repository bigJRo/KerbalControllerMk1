# KCMk1 Power Budget

**Organization:** Jeb's Controller Works  
**Author:** J. Rostoker  
**Version:** 1.2  
**Date:** 2026-06-27  
**Location:** `docs/developer/Power_Budget.md`

*12V supply: 10A rated · MPM3610 converter efficiency: ~88% · All currents in mA*

---

## Revision History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | — | Initial release — per-module and per-panel power budget, system rollup, supply headroom. |
| 1.1 | 2026-06-06 | Main display carriers updated from Teensy 4.0 + RA8875 5" 800×480 to Teensy 4.1 + LT7683 7" 1024×600 (ER-TFT070A2-6-5633): 450/500 mA → 480/520 mA @ 5V (227/246 mA @ 12V input). Recomputed A1, B1, A+B subtotal/total, A+B+C total, and supply headroom. Added per-carrier MPM3610 headroom note. Panel C displays left at the 5" assumption pending decision. |
| 1.2 | 2026-06-27 | Enclosure panel redesign reconciliation. GPWS Module relocated A1→A2; Abort Button relocated B1→A2; Status Indicator Display (XIAO RA4M1, B2) removed from design; mode select switch added to B2 (passive, negligible). Sys Info Display re-spec'd from XIAO RA4M1 + 1.9" 320×170 to XIAO RP2350 + 2.0" 240×320 (ER-TFTM020-2) + microSD — **power not yet measured**; A2 row and all dependent system totals/headroom flagged TBD with interim placeholder figures. Recomputed A1, B1, B2 panel totals. |

---

## 1. Per-Module Power Budget

All figures derived from measured data and component datasheets. Panel C is a potential growth scenario and is not part of the current build.

\* 12V input = (5V load ÷ 0.88) × (5/12) + (3.3V load ÷ 0.88) × (3.3/12) + 12V direct load

### Panel A1

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| Annunciator | 480 | 520 | — | — | — | — | 227 | 246 | Teensy 4.1 + LT7683 7" 1024×600 TFT |
| Info 1 Display | 480 | 520 | — | — | — | — | 227 | 246 | Teensy 4.1 + LT7683 7" 1024×600 TFT |
| **A1 Total** | **960** | **1,040** | **—** | **—** | **—** | **—** | **454** | **492** | 0.45A / 5.4W typ — 0.49A / 5.9W peak. Power button draw negligible |

### Panel A2

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| UI Ctrl | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M + Ctrl Mode toggle |
| Action Ctrl | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| Func. Ctrl | 415 | 892 | — | — | — | — | 197 | 424 | ATtiny816 + 12× PB26-13M + 8× switch inputs |
| Throttle | 115 | 165 | 5 | 10 | 550 | 1,100 | 608 | 1,147 | ATtiny816 + 5× LED + L293D + motorized pot |
| Translation | 50 | 112 | 10 | 20 | — | — | 27 | 57 | ATtiny816 + joystick |
| GPWS Module | 229 | 465 | 6 | 12 | — | — | 112 | 228 | ATtiny816 + 7-seg + 3× NeoPixel (relocated from A1) |
| Abort Button | 15 | 25 | — | — | — | — | 7 | 12 | Single LED button (relocated from B1) |
| Sys Info Display | TBD | TBD | TBD | TBD | — | — | TBD | TBD | XIAO RP2350 + ST7789 2.0" 240×320 (ER-TFTM020-2) + microSD — **measured figures needed** |
| **A2 Total** | **TBD** | **TBD** | **TBD** | **TBD** | **550** | **1,100** | **TBD** | **TBD** | Recompute once Sys Info Display (RP2350) measured. Sum of itemized non-display rows: 1,634 typ / 3,423 peak @ 5V |

### Panel B1

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| Info 2 Display | 480 | 520 | — | — | — | — | 227 | 246 | Teensy 4.1 + LT7683 7" 1024×600 TFT |
| Resource Display | 480 | 520 | — | — | — | — | 227 | 246 | Teensy 4.1 + LT7683 7" 1024×600 TFT |
| Dual Encoder | 20 | 30 | — | — | — | — | 10 | 14 | ATtiny816 + 2× rotary encoder |
| **B1 Total** | **980** | **1,070** | **—** | **—** | **—** | **—** | **464** | **506** | 0.46A / 5.6W typ — 0.51A / 6.1W peak |

### Panel B2

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| Stability Ctrl | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| Vehicle Ctrl | 415 | 892 | — | — | — | — | 197 | 424 | ATtiny816 + 12× PB26-13M + 8× switch inputs |
| Time Ctrl | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| Time Ctrl Dial | 204 | 415 | 6 | 12 | — | — | 100 | 201 | ATtiny816 + 7-seg + encoder |
| Auxiliary Ctrl | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| Rotation | 50 | 112 | 10 | 20 | — | — | 27 | 57 | ATtiny816 + joystick |
| Stage Button | 15 | 25 | — | — | — | — | 7 | 12 | Single LED button. Mode select switch is passive — negligible |
| **B2 Total** | **1,899** | **4,090** | **16** | **32** | **—** | **—** | **907** | **1,951** | 0.91A / 10.9W typ — 1.95A / 23.4W peak |

### Panel C — Potential Growth

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| C Display 1 | 450 | 500 | — | — | — | — | 214 | 238 | 5" Teensy/RA8875 assumed — revisit if C moves to 7" LT7683 |
| C Display 2 | 450 | 500 | — | — | — | — | 214 | 238 | 5" Teensy/RA8875 assumed — revisit if C moves to 7" LT7683 |
| C Button 1 | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| C Button 2 | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| **C Total** | **1,710** | **2,764** | **—** | **—** | **—** | **—** | **813** | **1,314** | 0.81A / 9.8W typ — 1.31A / 15.7W peak |

> † The A2 Sys Info Display (XIAO RP2350 + 2.0" 240×320 ST7789, ER-TFTM020-2, + microSD) power figure is **not yet measured** — all A2 and system totals depending on it are flagged TBD. The former RA4M1 1.9" estimate (180/240 mA @ 5V, ±50%) is retained only as an interim placeholder for headroom.

> ‡ Throttle motor peak assumes L293D stall at 500 mA. Actual stall current depends on motorized pot specification.

> § Peak assumes all PB26-13M NeoPixel buttons at full white simultaneously — a conservative worst case.

> ¶ Panel C displays are still shown at the 5" Teensy/RA8875 figures (450/500 mA). If C also moves to the 7" LT7683 carrier, each C display becomes 480/520 mA @ 5V (227/246 @ 12V input), raising the C total to ~1,770/2,804 mA @ 5V and ~838/1,330 mA @ 12V input. Revise when the C-group decision is made.

---

## 2. System Rollup by Panel

| Panel | 5V typ (mA) | 5V peak (mA) | 12V direct typ (mA) | 12V direct peak (mA) | 12V input typ (mA) | 12V input peak (mA) |
|-------|-------------|--------------|--------------------|--------------------|-------------------|-------------------|
| A1 | 960 | 1,040 | — | — | 454 | 492 |
| A2 | TBD | TBD | 550 | 1,100 | TBD | TBD |
| B1 | 980 | 1,070 | — | — | 464 | 506 |
| B2 | 1,899 | 4,090 | — | — | 907 | 1,951 |
| **A+B subtotal** | **TBD** | **TBD** | **550** | **1,100** | **TBD** | **TBD** |
| Control overhead | — | — | — | — | 50 | 50 |
| **A+B TOTAL** | **TBD** | **TBD** | **550** | **1,100** | **TBD** | **TBD** |
| C (growth) | 1,710 | 2,764 | — | — | 813 | 1,314 |
| **A+B+C TOTAL** | **TBD** | **TBD** | **550** | **1,100** | **TBD** | **TBD** |

> **Pending A2 Sys Info Display (XIAO RP2350 + 2.0" ST7789) measured power.** A+B and A+B+C totals cannot be finalized until this row is measured. For reference, the A2 non-display itemized sum is 1,634 typ / 3,423 peak @ 5V; add the RP2350 display row and recompute A2, the A+B subtotal, both totals, and §3 headroom. The previous RA4M1/1.9" Sys Info Display drew 180/240 mA @ 5V (±50%); the RP2350 + 2.0" + microSD figure is expected in a similar range but must be confirmed.

---

## 3. Supply Headroom

| Scenario | Typical (A) | Typical power (W) | Peak (A) | Peak power (W) |
|----------|-------------|-------------------|---------|----------------|
| A+B only † | 3.30 | 39.5 | 5.82 | 69.8 |
| A+B+C growth † | 4.11 | 49.3 | 7.13 | 85.6 |
| 12V supply rated | 10.0 | 120 | 10.0 | 120 |
| **Headroom A+B** † | **6.70** | **80.5** | **4.18** | **50.2** |
| **Headroom A+B+C** † | **5.89** | **70.7** | **2.87** | **34.4** |

> † **Interim figures** — computed using the previous RA4M1 Sys Info Display draw (85/114 mA @ 12V input) as a placeholder for the not-yet-measured XIAO RP2350 + 2.0" display. Recompute when the RP2350 row is measured. The net change from the prior revision is small: A1 lost GPWS, B2 lost the Status Indicator Display, A2 gained GPWS + Abort, B1 lost Abort — these largely offset, so headroom moved only marginally.

> Peak headroom with A+B only and with the C group is conservative — simultaneous full NeoPixel peak across all modules is an unlikely operating condition.

---

## 4. Key Assumptions & Open Items

- All PB26-13M NeoPixel button modules: typical = mixed brightness ~25 mA/pixel; peak = full white ~56 mA/pixel.
- 7" TFT displays (Teensy 4.1 + LT7683 1024×600, ER-TFT070A2-6-5633): 480 mA typical / 520 mA peak @ 5V (datasheet §4.6 max 520 mA @ 5V; typical estimated with backlight below full). Panel VDD on the 5V rail only — never 3.3V (730 mA @ 3.3V exceeds the carrier AP2112K). VDDIO 3.3V; Teensy 4.1 interfaces directly, no level shifters.
- **Per-carrier MPM3610 headroom (5V):** each display carrier sources its full 5V load from one local MPM3610 (1.2 A rated): ~660 mA typ / ~760 mA peak (panel 480/520 + Teensy 4.1 ~100/120 + 3.3V logic via AP2112K ~80/120 reflected to the 5V input). Peak headroom ~440 mA (37%). One MPM3610 per carrier is adequate; do not share one MPM3610 across two displays. Continuous load is higher than the former 5" carriers (~760 mA vs ~550 mA) — size MPM3610 thermal copper accordingly.
- ST7789 2.0" Sys Info Display (XIAO RP2350, A2): **figure not yet measured** — interim placeholder 180 mA typ / 240 mA peak @ 5V carried from the former RA4M1 1.9" carrier; update when measured. Status Indicator Display (formerly B2) removed from design.
- L293D H-bridge (Throttle): 60 mA logic quiescent on 5V; 550 mA typical / 1,100 mA peak on 12V motor supply.
- GPWS and Time Ctrl Dial 7-segment displays assumed always-on.
- Control circuit overhead fixed at 50 mA on 12V (master controller, soft-latch, miscellaneous).
- Switch groups (Switch Group 1 on A2 via Func. Ctrl, Switch Group 2 on B2 via Vehicle Ctrl): passive inputs only, negligible power addition.
- Panel C assumes 2× 5" Teensy/RA8875 displays (pending decision on whether C moves to 7" LT7683) and 2× standard 12-button PB26-13M modules — revise if hardware differs.

---

## Related Documents

- `docs/developer/Hardware_Reference.md` — power rail architecture, supply specification
- `docs/developer/Module_Interface_Reference.md` — per-module hardware specifications
