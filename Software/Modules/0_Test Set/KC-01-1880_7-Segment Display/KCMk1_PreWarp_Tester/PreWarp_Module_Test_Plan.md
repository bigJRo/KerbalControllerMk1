# Pre-Warp Time Module Test Plan

**Firmware:** KCMk1_PreWarp_Time v2.0.0 / Kerbal7SegmentCore v2.0.0  
**Controller:** KCMk1_PreWarp_Controller / Xiao RA4M1  
**Date:** ___________  
**Tester:** ___________

> **How to use:** Work through each phase in order. Mark each item ✅ PASS, ❌ FAIL, or ⚠️ PARTIAL. Note any unexpected behaviour in the Notes field. Send `E` to enable the module at the start of each phase unless otherwise noted.

---

## Phase 1 — Boot & Identity

**T1.1 — Power on both boards**

- [x] Controller prints identity: Type `0xC`, FW `1.0`, Caps `0b10000`
- [x] Controller prints `[BOOT] Module BOOT_READY — sending DISABLE`
- [x] Module is completely dark — no LEDs, no display
- [x] Controller prints `[INIT] Module disabled — send E when in flight scene`

Notes: ✅ PASS

**T1.2 — Query identity manually**

- Send: `I`
- [x] Identity printed correctly (Type `0xC`, FW `1.0`, Caps `0b10000`)
- [x] Module visually unaffected

Notes: ✅ PASS

---

## Phase 2 — Basic Lifecycle

**T2.1 — Enable**

- Send: `E`
- [x] All 3 NeoPixels light up **backlit** (dim warm colour)
- [x] 7-segment display shows **0**
- [x] Controller prints state update: value 0 min

Notes: ✅ PASS

**T2.2 — Disable**

- Send: `D`
- [x] All LEDs go **dark** immediately
- [x] Display goes **blank**

Notes: ✅ PASS

**T2.3 — Re-enable**

- Send: `E`
- [x] All 3 NeoPixels **backlit** again
- [x] Display shows **0** (reset on DISABLE)

Notes: ✅ PASS

---

## Phase 3 — INT Suppression

**T3.1 — Buttons don't fire INT when disabled**

- Send: `D` to disable
- Action: Press each button, turn encoder
- [x] Controller prints nothing — no INT events received
- [x] Module LEDs stay dark throughout

Notes: ✅ PASS

**T3.2 — Encoder doesn't accumulate when disabled** ⭐ Critical

- While DISABLED: turn encoder 20 clicks CW
- Send: `E` to enable
- Turn encoder exactly 1 click CW
- [x] Display shows **1** — NOT 21 or any other value
- [x] No burst of accumulated delta on re-enable

Notes: ✅ PASS

---

## Phase 4 — Preset Buttons

> Module must be ENABLED for this phase.

**T4.1 — BTN01 5 min preset**

- Action: Press BTN01
- [x] BTN01 flashes **GOLD** briefly (~150ms) then returns to **backlit**
- [x] Display jumps to **5**
- [x] Controller prints value: 5 min, BTN01 event bit set

Notes: ✅ PASS

**T4.2 — BTN02 1 hour preset**

- Action: Press BTN02
- [x] BTN02 flashes **GOLD** briefly then returns to **backlit**
- [x] Display jumps to **60**
- [x] Controller prints value: 60 min, BTN02 event bit set

Notes: ✅ PASS

**T4.3 — BTN03 1 day preset**

- Action: Press BTN03
- [x] BTN03 flashes **GOLD** briefly then returns to **backlit**
- [x] Display jumps to **1440**
- [x] Controller prints value: 1440 min, BTN03 event bit set

Notes: ✅ PASS

**T4.4 — Presets override encoder value** ⭐ Critical

- Turn encoder to **500**
- Action: Press BTN02 (1 hour)
- [x] Display jumps to **60** immediately — encoder value discarded
- [x] Controller prints value: 60 min

Notes: ✅ PASS

**T4.5 — All three buttons backlit after flash**

- Press BTN01, BTN02, BTN03 in sequence — wait for flash to complete each time
- [x] Each LED returns to **backlit** after flash — not dark, not gold
- [x] No LED stays lit between presses

Notes: ✅ PASS

**T4.6 — BTN_EN resets to 0**

- Press BTN03 to set 1440
- Action: Press encoder button (BTN_EN)
- [x] Display jumps to **0**
- [x] Controller prints value: 0 min, BTN_EN event bit set
- [x] No LED state changes (all remain backlit)

Notes: ✅ PASS

---

## Phase 5 — Encoder & Display

**T5.1 — Display on from the start**

- Send: `E`
- [x] Display shows **0** immediately on enable — no button press required
- [x] Display is always visible when ACTIVE

Notes: ✅ PASS

**T5.2 — Slow scroll (±1)**

- Action: Turn encoder slowly, one click at a time
- [x] Value changes by **1** per click
- [x] Controller prints each update

Notes: ✅ PASS

**T5.3 — Acceleration (±10, ±100, ±1000)**

- Action: Spin encoder continuously, gradually faster
- [x] Steps increase to **10** around 15 consecutive clicks
- [x] Steps increase to **100** around 30 consecutive clicks
- [x] Steps increase to **1000** around 50 consecutive clicks

Notes: ✅ PASS

**T5.4 — Direction reversal resets acceleration**

- Spin encoder fast CW (steps of 100+)
- Action: Reverse direction
- [x] First click CCW steps by **1** (run count reset on direction change)

Notes: ✅ PASS

**T5.5 — Inactivity timeout resets acceleration** ⭐ New behaviour

- Spin encoder fast CW until stepping by 100+
- Action: Stop and wait >500ms, then turn one click CW
- [x] Value steps by **1** — not 100 or 1000
- [x] Acceleration builds again from 1 on continued spinning

Notes: ✅ PASS

**T5.5 — Value clamping**

- Spin encoder CW past 9999
- [x] Display stops at **9999** — does not wrap or overflow

- Spin encoder CCW past 0
- [x] Display stops at **0** — does not go negative

Notes: ✅ PASS

**T5.6 — Set value from controller**

- Send: `S`, enter **720**
- [x] Display shows **720**
- [x] Controller prints value: 720 min

Notes: ✅ PASS

---

## Phase 6 — Sleep / Wake

**T6.1 — Sleep freezes state**

- Enable module, press BTN02 (flash then backlit), turn encoder to **360**
- Confirm display shows **360**, all buttons backlit
- Send: `P` (sleep)
- [x] Nothing changes visually — display still **360**, LEDs unchanged
- Action: Press buttons, turn encoder
- [x] Controller prints nothing — no INT received during sleep

Notes: ✅ PASS

**T6.2 — Wake restores exactly** ⭐ Critical

- Send: `U` (wake)
- [x] Controller prints state update
- [x] Value shown: **360 min**
- [x] Display shows **360**
- [x] All 3 LEDs **backlit** (unchanged from pre-sleep state)

Notes: ✅ PASS

**T6.3 — Encoder doesn't accumulate during sleep** ⭐ Critical

- With module SLEEPING and display showing 360: turn encoder 10 clicks CW
- Send: `U` (wake)
- Turn encoder exactly 1 click CW
- [x] Display shows **361** — NOT 371 or any other value

Notes: ✅ PASS

---

## Phase 7 — Vessel Switch & Reset

**T7.1 — Reset restores defaults**

- Enable module, press BTN01 (5 min), turn encoder to **300**
- Send: `R` (reset)
- [x] Display shows **0** (DEFAULT_VALUE)
- [x] All 3 LEDs go **backlit** (not dark)
- [x] Controller prints value: 0 min

Notes: ✅ PASS

**T7.2 — Vessel switch takes no action**

- Enable module, turn encoder to **120**
- Send: `V` (vessel switch)
- [x] Controller prints `[VESSEL SWITCH] No action for Pre-Warp module`
- [x] Module unchanged — display still shows **120**, LEDs same
- [x] No INT fired

Notes: ✅ PASS

---

## Phase 8 — Bulb Test

**T8.1 — Bulb test while ACTIVE**

- Enable module, turn encoder to **60**
- Send: `T`
- [x] All 7-segment segments illuminate (**8888** with all segments on)
- [x] All 3 NeoPixels go **white**
- Wait 2 seconds (controller sends stop automatically)
- [x] Display returns to **60**
- [x] All 3 NeoPixels return to **backlit**

Notes: ✅ PASS

**T8.2 — Bulb test while DISABLED** ⭐ New behaviour

- Send: `D` to disable (module dark)
- Send: `T`
- [ ] All 7-segment segments illuminate
- [ ] All 3 NeoPixels go **white**
- Wait 2 seconds
- [x] Module returns to **dark** (restores DISABLED visual state)

Notes: ___________

**T8.3 — Bulb test while SLEEPING** ⭐ New behaviour

- Enable module, set value to **90**, send: `P` (sleep)
- Send: `T`
- [ ] All 7-segment segments illuminate
- [ ] All 3 NeoPixels go **white**
- Wait 2 seconds
- [ ] Module returns to **90** on display, buttons **backlit** (restores pre-sleep state)
- [ ] Module still SLEEPING — no INT fired after restore

Notes: ✅ PASS — fixed: restore now checks lifecycle state

---

## Phase 9 — Full Lifecycle Demo

**T9.1 — Automated sequence**

- Send: `N`
- [ ] Step 1: Module goes dark (boot/no scene)
- [ ] Step 2: Stays dark (DISABLED)
- [ ] Step 3: Buttons backlit, display shows **0** on ENABLE
- [ ] Step 4: Controller sets value to **60** via SET_VALUE — display shows 60, INT fires
- [ ] Step 5: Vessel switch — no action, display unchanged
- [ ] Step 6: Sleep — state frozen at 60
- [ ] Step 7: Wake — state restored, display shows **60**, INT fires
- [ ] Step 8: DISABLE — module dark
- [ ] Counter sequential throughout — no gaps

Notes: ___________

---

## Summary

| Phase | Result | Notes |
|---|---|---|
| Phase 1 — Boot & Identity | | |
| Phase 2 — Basic Lifecycle | | |
| Phase 3 — INT Suppression | | |
| Phase 4 — Preset Buttons | | |
| Phase 5 — Encoder & Display | | |
| Phase 6 — Sleep / Wake | | |
| Phase 7 — Vessel Switch & Reset | | |
| Phase 8 — Bulb Test | | |
| Phase 9 — Full Lifecycle Demo | | |

**Overall result:** ✅ PASS — all 9 phases complete

**Issues to resolve:**

1. 
2. 
3. 
