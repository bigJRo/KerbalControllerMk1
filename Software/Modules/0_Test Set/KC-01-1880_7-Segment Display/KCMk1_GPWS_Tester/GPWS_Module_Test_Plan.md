# GPWS Module Controller Test Plan

**Firmware:** KCMk1_GPWS_Input v2.0 / Kerbal7SegmentCore v2.0.0  
**Controller:** KCMk1_GPWS_Controller / Xiao RA4M1  
**Date:** ___________  
**Tester:** ___________

> **How to use:** Work through each phase in order. Mark each item ✅ PASS, ❌ FAIL, or ⚠️ PARTIAL. Note any unexpected behaviour in the Notes field.

---

## Phase 1 — Boot & Identity

**T1.1 — Power on both boards**

- [ ] Controller prints identity: Type `0xB`, FW `1.0`, Caps `0b10000`
- [ ] Controller prints `[BOOT] Module BOOT_READY — sending DISABLE` or `[INIT] No startup INT — sending DISABLE`
- [ ] Module is completely dark — no LEDs, no display
- [ ] Controller prints `[INIT] Module disabled — send E when in flight scene`

Notes: ___________

**T1.2 — Query identity manually**

- Send: `I`
- [ ] Identity printed correctly (Type `0xB`, FW `1.0`, Caps `0b10000`)
- [ ] Module visually unaffected

Notes: ___________

---

## Phase 2 — Basic Lifecycle

**T2.1 — Enable**

- Send: `E`
- [ ] All 3 NeoPixels light up backlit (dim warm colour)
- [ ] 7-segment display stays blank (GPWS mode is OFF)
- [ ] Controller prints current state: GPWS OFF, all alarms off, threshold 200m

Notes: ___________

**T2.2 — Disable**

- Send: `D`
- [ ] All LEDs go dark immediately
- [ ] Display stays blank

Notes: ___________

**T2.3 — Re-enable**

- Send: `E`
- [ ] All 3 NeoPixels backlit again
- [ ] Display blank, threshold still 200m

Notes: ___________

---

## Phase 3 — INT Suppression

**T3.1 — Buttons don't fire INT when disabled**

- Send: `D` to disable
- Action: Press BTN01 several times, turn encoder
- [ ] Controller prints nothing — no INT events received
- [ ] Module LEDs stay dark throughout

Notes: ___________

**T3.2 — Encoder doesn't accumulate when disabled** ⭐ Critical

- While still DISABLED: turn encoder 20 clicks CW
- Send: `E` to enable
- Action: Press BTN01 to enable GPWS (display should show 200m)
- Turn encoder exactly 1 click CW
- [ ] Display shows **201m** — NOT 221m or any other value
- [ ] No burst of delta on re-enable

Notes: ___________

---

## Phase 4 — GPWS Mode Operation

> Module must be ENABLED for this phase. Send `E` if needed.

**T4.1 — BTN01 cycles through modes**

- Action: Press BTN01 once
- [ ] BTN01 goes **green**
- [ ] Display shows 200m
- [ ] Controller prints GPWS mode: ACTIVE

- Action: Press BTN01 again
- [ ] BTN01 goes **amber** (proximity tone only)
- [ ] Controller prints mode update

- Action: Press BTN01 again
- [ ] BTN01 goes **backlit**
- [ ] Display **blanks**
- [ ] Controller prints GPWS mode: OFF

Notes: ___________

**T4.2 — BTN02 and BTN03 don't fire INT when GPWS OFF**

- Ensure BTN01 is backlit (GPWS OFF)
- Action: Press BTN02 and BTN03 several times, turn encoder
- [ ] Controller prints nothing — no INT received

Notes: ___________

**T4.3 — BTN02 toggle when GPWS ACTIVE**

- Press BTN01 to enable GPWS (green)
- Action: Press BTN02
- [ ] BTN02 goes **green**
- [ ] Controller prints proximity alarm: ENABLED

- Action: Press BTN02 again
- [ ] BTN02 goes **backlit**
- [ ] Controller prints proximity alarm: off

Notes: ___________

**T4.4 — BTN02 forces prox mode when GPWS ACTIVE** ⭐ Critical

- Ensure BTN01 is **green** (GPWS ACTIVE), BTN02 is backlit
- Action: Press BTN02
- [ ] BTN01 changes **green → amber** (prox mode)
- [ ] BTN02 goes **green** (proximity alarm active)
- [ ] Controller prints both mode change AND proximity alarm ENABLED

Notes: ___________

**T4.5 — BTN03 toggle**

- Press BTN01 to reach GPWS ACTIVE (green)
- Action: Press BTN03
- [ ] BTN03 goes **green**
- [ ] Controller prints RDV radar: ENABLED

- Action: Press BTN03 again
- [ ] BTN03 goes **backlit**
- [ ] Controller prints RDV radar: off

Notes: ___________

**T4.6 — Cycling BTN01 off resets BTN02 and BTN03** ⭐ Critical

- Enable GPWS (BTN01 green), activate BTN02 and BTN03 (both green)
- Action: Press BTN01 once to cycle to PROX, then once more to return to OFF
- [ ] BTN01 goes **backlit**
- [ ] BTN02 goes **backlit** (reset automatically)
- [ ] BTN03 goes **backlit** (reset automatically)
- [ ] Display **blanks**

Notes: ___________

---

## Phase 5 — Encoder & Display

**T5.1 — Display only shows when GPWS active**

- Send: `E`, ensure BTN01 is backlit (GPWS OFF)
- [ ] Display is blank
- Action: Press BTN01 to enable GPWS
- [ ] Display shows **200m** immediately

Notes: ___________

**T5.2 — Slow scroll (±1)**

- GPWS enabled, display showing
- Action: Turn encoder slowly, one click at a time
- [ ] Value changes by **1** per click
- [ ] Controller prints each update

Notes: ___________

**T5.3 — Acceleration (±10, ±100, ±1000)**

- Action: Spin encoder continuously, gradually faster
- [ ] Steps increase to **10** around 15 consecutive clicks
- [ ] Steps increase to **100** around 30 consecutive clicks
- [ ] Steps increase to **1000** around 50 consecutive clicks

Notes: ___________

**T5.4 — Direction reversal resets acceleration**

- Spin encoder fast CW (steps of 100+)
- Action: Reverse direction
- [ ] First click CCW steps by **1** (run count reset on direction change)

Notes: ___________

**T5.5 — Value clamping**

- Spin encoder CW past 9999
- [ ] Display stops at **9999**, does not wrap or overflow

- Spin encoder CCW past 0
- [ ] Display stops at **0**, does not go negative

Notes: ___________

**T5.6 — BTN_EN resets threshold**

- Turn encoder to ~500m
- Action: Press encoder button (BTN_EN)
- [ ] Display jumps to **200m**
- [ ] Controller prints threshold: 200m

Notes: ___________

**T5.7 — Set value from controller**

- Send: `S`, enter **750**
- [ ] Display shows **750m**
- [ ] Controller prints threshold: 750m

Notes: ___________

---

## Phase 6 — Sleep / Wake

**T6.1 — Sleep freezes state**

- Enable GPWS, activate BTN02 (green), turn encoder to **350m**
- Send: `P` (sleep)
- [ ] Nothing changes visually — display still **350m**, LEDs unchanged
- Action: Press buttons, turn encoder
- [ ] Controller prints nothing — no INT received during sleep

Notes: ___________

**T6.2 — Wake restores exactly** ⭐ Critical

- Send: `U` (wake)
- [ ] Controller prints state update
- [ ] Threshold shown: **350m**
- [ ] BTN02 still **green**
- [ ] BTN01 state same as before sleep
- [ ] Display shows **350m**

Notes: ___________

---

## Phase 7 — Vessel Switch & Reset

**T7.1 — Reset restores backlit buttons and default value**

- Enable GPWS, activate BTN01 (green), BTN02 (green), encoder at 600m
- Send: `R` (reset)
- [ ] All 3 LEDs go **backlit** (NOT black/dark)
- [ ] Display **blanks** (GPWS mode resets to OFF)
- [ ] Controller prints: threshold 200m, GPWS OFF, all alarms off

Notes: ___________

**T7.2 — Vessel switch takes no action**

- Enable GPWS, set threshold to 450m
- Send: `V` (vessel switch)
- [ ] Controller prints `[VESSEL SWITCH] No action for GPWS module`
- [ ] Module unchanged — display still shows 450m, LEDs same
- [ ] No INT fired

Notes: ___________

---

## Phase 8 — Bulb Test

**T8.1 — Bulb test lights everything**

- Send: `T`
- [ ] All 7-segment segments illuminate (displays **8888** with all segments on)
- [ ] All 3 NeoPixels go **white**
- Wait 2 seconds
- [ ] Module returns to previous LED and display state automatically

Notes: ___________

---

## Phase 9 — Full Lifecycle Demo

**T9.1 — Automated sequence**

- Send: `N`
- [ ] Step 1: Module goes dark (boot/no scene)
- [ ] Step 2: Stays dark (DISABLED)
- [ ] Step 3: Buttons backlit on ENABLE
- [ ] Step 4: Threshold set to 350m
- [ ] Step 5: Vessel switch — resets then restores 350m
- [ ] Step 6: Sleep — state frozen
- [ ] Step 7: Wake — state restored
- [ ] Step 8: DISABLE — module dark

Notes: ___________

---

## Summary

| Phase | Result | Notes |
|---|---|---|
| Phase 1 — Boot & Identity | ✅ PASS | |
| Phase 2 — Basic Lifecycle | ✅ PASS | |
| Phase 3 — INT Suppression | ✅ PASS | |
| Phase 4 — GPWS Mode Operation | ✅ PASS | T4.2 required firmware fix |
| Phase 5 — Encoder & Display | ✅ PASS | T5.7 required controller fix |
| Phase 6 — Sleep / Wake | ✅ PASS | |
| Phase 7 — Vessel Switch & Reset | ✅ PASS | Vessel switch simplified to no-op |
| Phase 8 — Bulb Test | ✅ PASS | |
| Phase 9 — Full Lifecycle Demo | ✅ PASS | |

---

## Supplemental Tests

**T10.1 — Rapid button presses**

- Enable GPWS (BTN01 green)
- Action: Press BTN01 rapidly 3-4 times in quick succession
- [x] BTN01 cycles correctly through all states without getting stuck or double-counting
- [x] Counter increments sequentially with no gaps
- [x] Final state matches number of presses

Notes: ✅ PASS

**T10.2 — DISABLE from prox mode with all buttons active**

- Enable GPWS, force prox mode (BTN01 amber, BTN02 green), activate BTN03 (green)
- Send: `D`
- [x] All LEDs go completely dark immediately
- Send: `E`
- [x] All 3 LEDs come up backlit — not amber, not green
- [x] Full reset to defaults on re-enable

Notes: ✅ PASS

**Overall result:** ✅ PASS — all 9 phases + 2 supplemental tests complete

**Issues to resolve:**

1. 
2. 
3. 
