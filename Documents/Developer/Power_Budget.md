# KCMk1 Power Budget

**Organization:** Jeb's Controller Works  
**Author:** J. Rostoker  
**Version:** 1.0  
**Location:** `docs/developer/Power_Budget.md`

*12V supply: 10A rated · MPM3610 converter efficiency: ~88% · All currents in mA*

---

## 1. Per-Module Power Budget

All figures derived from measured data and component datasheets. Panel C is a potential growth scenario and is not part of the current build.

\* 12V input = (5V load ÷ 0.88) × (5/12) + (3.3V load ÷ 0.88) × (3.3/12) + 12V direct load

### Panel A1

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| Annunciator | 450 | 500 | — | — | — | — | 214 | 238 | Teensy 4.0 + RA8875 5" TFT |
| Info 1 Display | 450 | 500 | — | — | — | — | 214 | 238 | Teensy 4.0 + RA8875 5" TFT |
| GPWS Module | 229 | 465 | 6 | 12 | — | — | 112 | 228 | ATtiny816 + 7-seg + 3× NeoPixel |
| **A1 Total** | **1,129** | **1,465** | **6** | **12** | **—** | **—** | **540** | **704** | 0.54A / 6.5W typ — 0.70A / 8.4W peak |

### Panel A2

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| UI Ctrl | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M + Ctrl Mode toggle |
| Action Ctrl | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| Func. Ctrl | 415 | 892 | — | — | — | — | 197 | 424 | ATtiny816 + 12× PB26-13M + 8× switch inputs |
| Throttle | 115 | 165 | 5 | 10 | 550 | 1,100 | 608 | 1,147 | ATtiny816 + 5× LED + L293D + motorized pot |
| Translation | 50 | 112 | 10 | 20 | — | — | 27 | 57 | ATtiny816 + joystick |
| Sys Info Display | 180 | 240 | — | — | — | — | 85 | 114 | XIAO RA4M1 + ST7789 1.9" TFT ±50% |
| **A2 Total** | **1,570** | **3,173** | **15** | **30** | **550** | **1,100** | **1,301** | **2,580** | 1.30A / 15.6W typ — 2.58A / 31.0W peak |

### Panel B1

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| Info 2 Display | 450 | 500 | — | — | — | — | 214 | 238 | Teensy 4.0 + RA8875 5" TFT |
| Resource Display | 450 | 500 | — | — | — | — | 214 | 238 | Teensy 4.0 + RA8875 5" TFT |
| Dual Encoder | 20 | 30 | — | — | — | — | 10 | 14 | ATtiny816 + 2× rotary encoder |
| Abort Button | 15 | 25 | — | — | — | — | 7 | 12 | Single LED button |
| **B1 Total** | **935** | **1,055** | **—** | **—** | **—** | **—** | **445** | **502** | 0.44A / 5.3W typ — 0.50A / 6.0W peak |

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
| Indicator Module | 180 | 240 | — | — | — | — | 85 | 114 | XIAO RA4M1 + ST7789 1.9" TFT ±50% |
| **B2 Total** | **2,079** | **4,330** | **16** | **32** | **—** | **—** | **992** | **2,065** | 0.99A / 11.9W typ — 2.07A / 24.8W peak |

### Panel C — Potential Growth

| Module | 5V typ | 5V peak | 3.3V typ | 3.3V peak | 12V direct typ | 12V direct peak | 12V input typ | 12V input peak | Notes |
|--------|--------|---------|----------|-----------|----------------|-----------------|--------------|---------------|-------|
| C Display 1 | 450 | 500 | — | — | — | — | 214 | 238 | Teensy 4.0 + RA8875 5" TFT (assumed) |
| C Display 2 | 450 | 500 | — | — | — | — | 214 | 238 | Teensy 4.0 + RA8875 5" TFT (assumed) |
| C Button 1 | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| C Button 2 | 405 | 882 | — | — | — | — | 192 | 419 | ATtiny816 + 12× PB26-13M |
| **C Total** | **1,710** | **2,764** | **—** | **—** | **—** | **—** | **813** | **1,314** | 0.81A / 9.8W typ — 1.31A / 15.7W peak |

> † ST7789 1.9" display modules (Sys Info Display, Indicator Module) carry ±50% uncertainty margin pending measured data.

> ‡ Throttle motor peak assumes L293D stall at 500 mA. Actual stall current depends on motorized pot specification.

> § Peak assumes all PB26-13M NeoPixel buttons at full white simultaneously — a conservative worst case.

---

## 2. System Rollup by Panel

| Panel | 5V typ (mA) | 5V peak (mA) | 12V direct typ (mA) | 12V direct peak (mA) | 12V input typ (mA) | 12V input peak (mA) |
|-------|-------------|--------------|--------------------|--------------------|-------------------|-------------------|
| A1 | 1,129 | 1,465 | — | — | 540 | 704 |
| A2 | 1,570 | 3,173 | 550 | 1,100 | 1,301 | 2,580 |
| B1 | 935 | 1,055 | — | — | 445 | 502 |
| B2 | 2,079 | 4,330 | — | — | 992 | 2,065 |
| **A+B subtotal** | **5,713** | **10,023** | **550** | **1,100** | **3,278** | **5,851** |
| Control overhead | — | — | — | — | 50 | 50 |
| **A+B TOTAL** | **5,713** | **10,023** | **550** | **1,100** | **3,328** | **5,901** |
| C (growth) | 1,710 | 2,764 | — | — | 813 | 1,314 |
| **A+B+C TOTAL** | **7,423** | **12,787** | **550** | **1,100** | **4,141** | **7,215** |

---

## 3. Supply Headroom

| Scenario | Typical (A) | Typical power (W) | Peak (A) | Peak power (W) |
|----------|-------------|-------------------|---------|----------------|
| A+B only | 3.33 | 39.9 | 5.90 | 70.8 |
| A+B+C growth | 4.14 | 49.7 | 7.22 | 86.6 |
| 12V supply rated | 10.0 | 120 | 10.0 | 120 |
| **Headroom A+B** | **6.67** | **80.1** | **4.10** | **49.2** |
| **Headroom A+B+C** | **5.86** | **70.3** | **2.79** | **33.4** |

> Peak headroom with A+B only (4.10A) and 28% with C group are conservative — simultaneous full NeoPixel peak across all modules is an unlikely operating condition.

---

## 4. Key Assumptions & Open Items

- All PB26-13M NeoPixel button modules: typical = mixed brightness ~25 mA/pixel; peak = full white ~56 mA/pixel.
- 5" TFT displays (Teensy 4.0 + RA8875): 450 mA typical / 500 mA peak from measured data.
- ST7789 1.9" displays (XIAO RA4M1): 180 mA typical / 240 mA peak — **±50% uncertainty**, update when measured.
- L293D H-bridge (Throttle): 60 mA logic quiescent on 5V; 550 mA typical / 1,100 mA peak on 12V motor supply.
- GPWS and Time Ctrl Dial 7-segment displays assumed always-on.
- Control circuit overhead fixed at 50 mA on 12V (master controller, soft-latch, miscellaneous).
- Switch groups (Switch Group 1 on A2 via Func. Ctrl, Switch Group 2 on B2 via Vehicle Ctrl): passive inputs only, negligible power addition.
- Panel C assumes 2× full 5" Teensy/RA8875 displays and 2× standard 12-button PB26-13M modules — revise if hardware differs.

---

## Related Documents

- `docs/developer/Hardware_Reference.md` — power rail architecture, supply specification
- `docs/developer/Module_Interface_Reference.md` — per-module hardware specifications
