# KCMk1 Display Test Plan

**Document type:** Developer / Verification
**Location:** `Documents/Developer/Display_Test_Plan.md`
**Version:** 1.0
**Firmware covered:** Info Display v1.15.1 (both units) · Annunciator v3.7.0 · Resource Display v3.14.1

---

## 1. Purpose and how to use this plan

This is an in-game run-through of every screen and feature on the three TFT display panels. It is organised as a sequence of **mission phases** so the whole plan can be flown in order across a handful of KSP sessions, with each test point checked off as the situation arises. Test points that need a specific vessel type (aircraft, rover, EVA) are grouped into their own sessions.

**Test point IDs** are `<session>-<number>`, e.g. `ASC-04`. Each row lists the action, the expected result and the source of truth (threshold or rule) so a disagreement can be traced to the README or config constant rather than argued.

**Recording results.** Copy the tables into a run log (or print them) and mark each row P / F / N/A, with a note on any failure. The defect-log template in §21 collects failures for triage. Record the firmware version strings from each boot screen at the top of the log.

**Panel names used below.**

| Name | Panel | I2C | Role |
|---|---|---|---|
| **Info 1** | A1 | 0x12 | Vehicle-type PFD panel, sidebar on the left, sixth key `VEH` |
| **Info 2** | B1 | 0x13 | Mission-phase panel, sidebar on the right, sixth key `A/P` (owns every autopilot console) |
| **C&W** | A1 | 0x10 | Annunciator: Main / SOI / Standby |
| **RES** | B1 | 0x11 | Resource Display: Main / Select / Detail / Standby, plus EVA layout |

**Colour vocabulary** (applies to every panel): dark green = nominal, yellow = caution, white-on-red = alarm, dark grey = inactive / not applicable, cyan = pilot-entered or pending, orange = disconnect reason / guard. Sidebar keys are achromatic (white-on-black unselected, black-on-grey selected) and only take colour when they carry live state.

---

## 2. Prerequisites

### 2.1 Software

- KSP1 with **KerbalSimpit** installed and the serial port bound to the controller. All four panels show the standby splash until a flight scene starts.
- Optional mods that enable specific test points (mark N/A if not installed): **TAC Life Support** (RES life-support meters, C&W LIFE SUPP), **Community Resource Pack** (RES XPD / ADV presets), **Alternate Resource Panel** (EC% on the pre-launch board and ROVER screen).
- On the Annunciator: `audioEnabled` true and the DFPlayer microSD populated with clips 001–034 for the audio and GPWS sessions. Both can be skipped, mark those rows N/A.
- All panels flashed with `demoMode = false`, `debugMode = false`, `standaloneMode = false`.

### 2.2 Test craft (build these in the VAB/SPH before starting)

| Craft | Used in | Build notes |
|---|---|---|
| **T1 Rocket** | LNCH, ASC, ORB, MNVR, ENTR, DESC, AP consoles | Two liquid stages plus a pair of SRBs, crewed capsule, heat shield, drogue on CAG 1 (cut on 2), main on CAG 3 (cut on 4), landing gear or legs, RCS + monoprop, antenna. Capable of Kerbin orbit and return |
| **T2 Target** | TGT, DOCK, EVA | Any craft with a docking port left in a ~100 km Kerbin orbit ahead of time. Set it as target from T1 |
| **T3 Lander** | DESC, LDAP, VEH | Mun-capable lander with legs and radar-visible descent, or reuse T1 for a Kerbin powered descent |
| **T4 Aircraft** | ACFT, NAV, ACAP, GPWS | Jet, **vessel type set to Plane** in the VAB/SPH naming dialog. Gear on the normal gear group, airbrakes on CAG 38 (`AIRBRAKE_CAG`) |
| **T5 Rover** | ROVR, RVAP | Wheeled rover, **vessel type set to Rover**, with a battery and a target flag planted a few hundred metres away |
| **T6 EVA** | EVA rows | Any crewed craft in orbit; a Kerbal on EVA with jetpack fuel |

The vessel-type rule is deliberate: the AIRCRAFT and ROVER instruments key off the declared type, not the flight profile. A plane typed as Probe fails the ACFT session by design.

---

## 3. Session BOOT — power-on, boot screens, standby

Power the controller on with KSP at the main menu.

| ID | Action | Expected | Source |
|---|---|---|---|
| BOOT-01 | Watch all four panels power up | Each shows a boot screen with its firmware version in the header. Info panels show one of three random KSP-themed sequences (Mission Log / Loading Tips / Pre-Flight Checklist) that varies from boot to boot | README Boot Sequence |
| BOOT-02 | Wait for the master handshake | Every panel leaves the boot screen once the master sends PROCEED; none hangs on boot | I2C boot handshake |
| BOOT-03 | Observe panels with KSP at the main menu | All four show the full-screen standby splash. Touch does nothing on Info 1 / Info 2 / C&W (RES only advances on touch in demo mode) | Standby screen |
| BOOT-04 | Load a save and go to flight with T1 on the pad | All four leave standby automatically. Info 1 on SPACECRAFT PFD, Info 2 on the LAUNCH pre-launch board, C&W on Main, RES on Main | Scene change → flight |
| BOOT-05 | Return to the Space Center | All four return to standby | Scene change → non-flight |
| BOOT-06 | With a flight already running, power-cycle the controller | After boot, panels come up on the live flight screens rather than sitting on standby | InfoDisp 1.2.1 / C&W 3.5.3 |
| BOOT-07 | Touch each panel immediately after reset while it is settling | No phantom gesture fires (no screen change, no bug set on RES) | Touch boot guard |

---

## 4. Session NAV — sidebar navigation and mode cycling (both Info panels)

Run on the pad or in a stable orbit. Repeat every row on **both** Info panels; note the sidebar is left-edge on Info 1 and right-edge on Info 2.

| ID | Action | Expected | Source |
|---|---|---|---|
| NAV-01 | Press `PFD` from another screen | Jumps to the context PFD for the vessel (SPC for T1). Caption reads `SPC` | Btn 0 |
| NAV-02 | Press `PFD` repeatedly | Info 1 cycles SPC → ACFT → ROVR → SPC (three deep). Info 2 cycles SPC → ACFT → ROVR → VEH → SPC (four deep). Caption follows | Btn 0 ring |
| NAV-03 | Press `LNCH` repeatedly (not on the pad) | Cycles ASC ↔ CIRC, caption `ASC` / `CIRC` | Btn 1 |
| NAV-04 | Press `ORB` from another screen, then repeatedly | First press lands on ORBIT (Apsides). Then ORB → ORB+ → MNVR → ORB, caption `ORB` / `ORB+` / `MNVR` | Btn 2 |
| NAV-05 | Press `TGT` from another screen with no target | Lands on TARGET showing `NO TARGET SET`. Repeated press cycles TGT → DOCK → NAV → TGT | Btn 3 |
| NAV-06 | Press `LNDG` repeatedly | First press = POWERED DESCENT (`DESC`), then DESC ↔ ENTR | Btn 4 |
| NAV-07 | Info 1: press `VEH` | VEHICLE INFO, single mode, caption `VEH` | Btn 5 unit 1 |
| NAV-08 | Info 2: press `A/P` with T1 on the pad | Ascent Autopilot console. Key caption changes from `A/P` to `ASC` while the console is up | Btn 5 unit 2 |
| NAV-09 | Info 2: press `A/P` repeatedly on the pad | Cycles only consoles the situation allows: ASC → ORAP → LDAP → ASC (no ACAP, no RVAP for a rocket) | apConsole ring |
| NAV-10 | Tap the title bar of any screen | Nothing happens (title-bar navigation removed) | v1.0.0 |
| NAV-11 | Tap twice quickly on the same sidebar key | Second press registers after ~150 ms (mode cycling is not held to the 500 ms debounce) | TOUCH_CYCLE_DEBOUNCE_MS |
| NAV-12 | Tap the sidebar with two fingers | Ignored | Count filter |
| NAV-13 | Look at the right end of the title bar | An outlined `AUTO` (grey) or `MAN` (dark green) chip states whether the screen was chosen by the ladder or held by hand | AUTO / MAN chip |
| NAV-14 | Press a key that lands on a screen the ladder did not choose | Chip flips to `MAN`. The screen stays put through telemetry changes that would otherwise move it | Manual latch |
| NAV-15 | On a `MAN` screen, press the key that owns the ladder's current choice | Returns to the automatic screen and chip reads `AUTO` | Latch release (explicit) |
| NAV-16 | On a `MAN` screen, switch vessel (`[` / `]`) or leave and re-enter flight | Latch clears, panel routes by the ladder | Latch release (vessel / scene) |

---

## 5. Session PAD — pre-launch (T1 on the launch pad)

| ID | Action | Expected | Source |
|---|---|---|---|
| PAD-01 | Load T1 onto the pad | **Info 2** shows the LAUNCH pre-launch board automatically (`AUTO` chip, caption `PRE`). **Info 1** shows SPACECRAFT PFD | Mission rule 1 |
| PAD-02 | Read the pre-launch board | Vessel name, type, SAS, RCS, throttle, EC%, crew count, CommNet signal, ΔV.Tot, and drogue / main parachute states all shown. Chutes read `STOWED` | Pre-launch board |
| PAD-03 | Toggle SAS and RCS from the controller | Board fields update to match | Live telemetry |
| PAD-04 | Move the throttle lever | Throttle field follows | Live telemetry |
| PAD-05 | Tap the content area of the pre-launch board | Board dismisses into ASCENT; it does not come back while still on the pad | Tap-to-advance |
| PAD-06 | Press `LNCH` while the pre-launch board is up | Same as PAD-05 (a press dismisses it) | Btn 1 |
| PAD-07 | **C&W** situation column | `PRE-LAUNCH` tile lit, nothing else in the outer column. Regime column shows `FLYING LOW` or nothing consistent with the pad | Vessel situation |
| PAD-08 | **C&W** mode/status grid | `AUDIO` tile matches audioEnabled, `DEMO` dark, `SIMPIT LOST` dark, `THRTL ENA` follows the throttle enable switch, `TRIM` / `THRTL PREC` / `INPUT PREC` / `ENG ARM` follow their controller switches | modeFlags |
| PAD-09 | **C&W** control-mode tile | `SPCFT` in green with a spacecraft icon when the mode switch is on Spacecraft; turns red if the switch is moved to Aircraft or Rover while flying T1 | Mode mismatch |
| PAD-10 | **C&W** bottom readouts | Vessel name, TIMEWARP 1x, STG, TMAX, CREW, COMM, TSKIN, CAP, CTRLGRP all populated. Labels grey uppercase, values in the state colour | Readout label style |
| PAD-11 | **C&W** `ATMO` and `O2 PRESENT` | Both lit on the Kerbin pad | C&W bits 14, 9 |
| PAD-12 | **C&W** `Ap LOW` | Lit (apoapsis below the atmosphere) | Bit 18 |
| PAD-13 | Press the Abort action | `ABORT` lamp red, MASTER ALARM lights and audio sounds. Clear abort: lamp clears, alarm stops | Bit 4, master mask |
| PAD-14 | Tap MASTER ALARM while it is lit | Audio silences; lamp remains until the condition clears. A new warning bit re-arms the alarm | Silence latch (3.1.0) |
| PAD-15 | Tap the SOI thumbnail on C&W Main | SOI screen: KASA meatball, body name `KERBIN`, body image, rows MIN SAFE ALT, SOI RADIUS, REENTRY ALT, HIGH ATMO ALT, LOW SPACE ALT, HIGH SPACE ALT, CONDITION, SURF. GRAVITY. Tap anywhere returns to Main | SOI screen |
| PAD-16 | Trigger a master alarm (abort) while on the SOI screen | Alarm sounds and continues; SOI screen stays up | 3.7.0 alarm audio on every screen |
| PAD-17 | **RES** on first flight of T1 | Main shows the SPCT default set (EC, LF, LOx, MP, SF, O2, Food, Water, Ablator) collapsed to the resources actually aboard. Vessel name in the alert strip is **grey** (default layout) | Default layout / presence |
| PAD-18 | **Info 1** press `VEH` | Vessel name, type, situation `PRE-LAUNCH` (not `RECOVERABLE`), control level, CommNet, crew/capacity, ΔV.Stg, ΔV.Tot | VEH, 1.10.17 |

---

## 6. Session ASC — ascent (launch T1)

| ID | Action | Expected | Source |
|---|---|---|---|
| ASC-01 | Stage to launch | Info 2 stays on LAUNCH and shows ASCENT (`ASC`). It does **not** drop to ORBIT once the clamps release | Mission rule 7 |
| ASC-02 | Read the ASCENT left panel | Altitude ladder with `ALT` (filled) and `Ap` (hollow) markers against a labelled tick scale, ATMO gauge (bar only, no digit window), V.Vrt bar, V.Orb bar each with a boxed value, FPA dial, heading tape | Ascent layout |
| ASC-03 | Read the ASCENT right column | Seven rows: T+Ap, Thrtl, Q, Mach, G, Stg.Brn, ΔV.Stg | Ascent readout |
| ASC-04 | Watch Q through max-Q (~5–8 km) | Q rises to a peak then falls. Turns yellow past 20 kPa, white-on-red past 40 kPa (fly a steep, fast ascent to reach it) | LNCH_Q_WARN/ALARM |
| ASC-05 | Watch the ATMO gauge climbing out | Triangle descends smoothly and is still a quarter of the way up at ~30 km (fourth-root scale) | ATMO bar |
| ASC-06 | Pull > 4 g (heavy TWR or a hard pitch) | G row yellow above 4 g, white-on-red above 9 g. C&W `HIGH G` lights at the same 9 g | G_WARN_POS / G_ALARM_POS |
| ASC-07 | SRBs burning | C&W `SRB ACTIVE` lit while solid fuel is falling; clears within ~3 s of burnout | Bit 23 |
| ASC-08 | Stage the boosters / first stage | ΔV.Stg and Stg.Brn jump to the new stage. RES stage column on LF / LOx / SF snaps to the new stage level (white stage line) | Staging |
| ASC-09 | Let ΔV.Stg fall under 150 m/s, or Stg.Brn under 60 s | ΔV.Stg white-on-red; C&W `LOW ΔV` red and MASTER ALARM on. Both panels agree on the moment | DV_STG_ALARM_MS / CW_LOW_DV |
| ASC-10 | Throttle to zero briefly at ~20 km with Ap still below 70 km | Screen **stays** on ASCENT (coast needs Ap above the orbit-safe line as well) | ASC→CIRC gate |
| ASC-11 | Cut the throttle with Ap above 70 km | Screen switches to CIRCULARIZATION (`CIRC`) and **latches** | LNCH_COAST_THROTTLE + Ap gate |
| ASC-12 | Read the CIRC screen | Orbit diagram with cyan Ap and magenta Pe dots, ΔV burn bar, attitude disc, T+Ign countdown, Ecc and Ap−Pe on the rail. Right third: apsis tape with Ap near the top, Pe hollow and clamped at the bottom while suborbital, orbit-safe floor drawn, rows ΔV.Circ, Burn Dur, Stg.Brn | CIRC layout |
| ASC-13 | Burn at Ap to circularise | Pe marker rises up the tape, becomes solid once on-scale, bracket closes on Ap. Screen stays on CIRC through the burn even though throttle is open | Latch |
| ASC-14 | Ap−Pe value from 100–1000 km gap | Fits its rail box (no overflow) | 1.9.4 |
| ASC-15 | **Info 1** during ascent | SPACECRAFT PFD, row 1 label `V.SRF` in the atmosphere and swapping to `V.ORB` when orbital mode takes over. `SRF`/`ORB` chip at the heading-tape's left edge follows | Vel ref auto |
| ASC-16 | C&W `HIGH Q` | Lights when dynamic pressure passes Kerbin's highQ threshold, clears after max-Q | Bit 19 |
| ASC-17 | C&W situation column | `FLIGHT` then `SUB-ORBIT` as Ap leaves the atmosphere; regime tiles walk FLYING LOW → FLYING HIGH → LOW SPACE | Situation / regime |
| ASC-18 | C&W chirps (audio on) | Alert chirp crossing 3500 m upward, crossing 100 m/s upward, and when Ap crosses the body's min-safe altitude | Audio triggers |
| ASC-19 | C&W `ATMO` clears at 70 km | Lamp goes out at atmosphere exit; `DESCENT` off while climbing | Bits 14, 12 |

---

## 7. Session ORB — in orbit (T1 in a ~100 km Kerbin orbit)

| ID | Action | Expected | Source |
|---|---|---|---|
| ORB-01 | Once Pe is above 70 km with no node and no target | Info 2 auto-routes to ORBIT (`AUTO`). Info 1 stays on SPACECRAFT | Mission rule 11 |
| ORB-02 | Read ORBIT (Apsides) | Orbit + inclination diagram; left SMA, Ecc, PeA, ApA; right Inc, Period, Arg.Pe, T+Pe / T+Ap | ORB |
| ORB-03 | Press `ORB` → `ORB+` | Text-only Advanced Elements: SMA, Ecc, PeA, ApA, Alt.SL, V.Orb, Period / Inc, LAN, Arg.Pe, True Anom, Mean Anom, T+Pe, T+Ap. All values live | ORB+ |
| ORB-04 | C&W `ORBIT STABLE` | Lit (Pe and Ap above the atmosphere, Ap inside the SOI, situation ORBIT). `Ap LOW` and `Pe LOW` dark. `ORBIT` situation tile lit, `HIGH SPACE` / `LOW SPACE` regime correct for the altitude | Bits 20, 18, 6 |
| ORB-05 | Entering orbit for the first time (audio) | Alert chirp on the ORBIT bit | Audio triggers |
| ORB-06 | **Info 1** SPACECRAFT PFD in orbit | EADI ball with pitch ladder, roll pointer, boresight symbol, prograde / retrograde markers, three rate pointers beside the ball; Hdg / Pitch / Roll readouts; right column Alt.SL, V.Orb, ApA, PeA, T+Ap, row 5 `V.VRT`, ΔV.Stg, RCS / SAS buttons | PFD |
| ORB-07 | Tap the `ORB` / `SRF` chip on the PFD | Chip pins the other reference (marker, tapes and row 1 swap). Tap again to return to automatic; chip colour shows held vs auto | 1.11.0 |
| ORB-08 | Toggle SAS modes on the controller | PFD SAS button label follows the Simpit SAS mode (STAB, PROGRADE, TARGET …) | SAS mapping |
| ORB-09 | Fire an RCS pulse in pitch | Rate pointer deflects and settles; ball follows | Rate pointers |
| ORB-10 | Pitch past ±30° | Red chevrons appear on the ladder above 32°, clear below 28° | 1.11.1 |
| ORB-11 | Hold roll at 90° and pitch up | Ball and every marker move in body axes (screen-up is the craft roof) | 1.0.4 |
| ORB-12 | Time-warp | C&W `WARP` tile lit and TIMEWARP readout shows the rate; RES TTE estimates stay in game time | modeFlags / warp correction |
| ORB-13 | Solar panels charging | C&W `ELEC GEN` lit while EC rises; drops within 5 s of a full battery | Bit 21 |
| ORB-14 | Point panels away until EC falls under 5% | C&W `BUS VOLTAGE` red + MASTER ALARM. RES EC meter frame red, `EC LOW` white-on-red in the alert strip, counter red | Bit 3 / RES_ALARM_FRAC |
| ORB-15 | Lower Pe into the aerobrake band then below re-entry altitude | `Pe LOW` yellow companion in the aerobrake zone, red (and MASTER ALARM) below re-entry altitude; ORBIT STABLE goes dark | Bit 6 |
| ORB-16 | Monoprop under 20% | C&W `RCS LOW` yellow; RES MP tape enters the yellow band and `MP CAUT` appears in the strip | Bit 15 / RES_WARN_FRAC |
| ORB-17 | Burn LF only (drain the ratio) | C&W `PROP RATIO` lights when LF:LOx leaves 9:11; RES propellant balance cell in the strip changes state | Bit 16 |
| ORB-18 | Lose CommNet (retract antenna, or occlusion) | C&W `COMM LOST` lit; PFD/VEH CommNet readouts change | Bit 17 |

---

## 8. Session MNVR — maneuver node (T1 in orbit)

| ID | Action | Expected | Source |
|---|---|---|---|
| MNVR-01 | Press `ORB` twice more to reach `MNVR` with no node | `NO MANEUVER` fullscreen (white-on-red) | MNVR splash |
| MNVR-02 | Create a node 30 min ahead | Info 2 stays on ORBIT (node not yet imminent) | Rule 8 gate |
| MNVR-03 | Warp until T+Ign < 10 min | Info 2 auto-routes to MANEUVER (`AUTO`). Does not release until T+Ign passes ~700 s the other way or the node is gone | MNVR_CONTEXT_LEAD_S + release band |
| MNVR-04 | Read MANEUVER | Blue node marker vs fixed crosshair; numeric ΔV.Mnvr, ΔV.Plan, ΔV.Stg, T+Ign, T+Mnvr, Burn Dur, Brg / Elv, RCS / SAS; ΔV burn bar under the reticle | MNVR |
| MNVR-05 | Point the nose at the node | Neon-green alignment box appears inside 5°; Brg / Elv go to zero; rings are the colour bands (green inside 5°, yellow to 10°, red beyond) | 1.0.5 |
| MNVR-06 | Node with two planned nodes | ΔV.Plan equals the sum of both | ΔV.Plan |
| MNVR-07 | Node marker beyond full scale | Marker drawn dimmed at the clamp, not as a solid reading | 1.0.7 |
| MNVR-08 | **Info 1** PFD row 5 while the node is pending | Reads `T+IGN` counting down; once the burn is lit reads `dV.REM`; returns to `V.VRT` when the node is cleared | 1.10.21 |
| MNVR-09 | Execute the burn manually | Screen stays on MANEUVER through negative T+Ign; MNVR clears to ORBIT after the node is deleted and the dwell (4 s) elapses | Rule 8 / CONTEXT_DWELL_MS |

---

## 9. Session ORAP — Orbital Autopilot console (Info 2 only)

Requires Controller_Main running the mission autopilot. All annunciations reflect the **master's echoed state**: a tap shows a cyan pending cue and only turns green when the master confirms.

| ID | Action | Expected | Source |
|---|---|---|---|
| ORAP-01 | In orbit with a node planned, press `A/P` | Lands on ORBITAL AUTOPILOT (situation pick), key caption `ORAP` | apConsoleContextScreen |
| ORAP-02 | Read the grid | BURN column [NODE] [AP] [PE] [INC] with value boxes; TARGET / OPTIONS with [APPR] rate, HOLD AT, WARP AUTO/OFF, STAGE AUTO/OFF; PLAN column ΔV TOT, ΔV REM, T-IGN, BURN, ACCEL, STG ΔV, RANGE | Screen_ORBT_AP header |
| ORAP-03 | Tap [NODE] | Button goes cyan (pending), then green when the master arms it. PLAN column fills with ΔV TOT, BURN duration and T-IGN. Sidebar `A/P` key turns green | Two-step burn |
| ORAP-04 | Tap [EXEC] | Executor starts: banner shows the phase (ALIGN → BURN), throttle commanded, node ΔV REM counts down to zero, executor finishes and banner returns to idle after ~5 s | Executor |
| ORAP-05 | Tap the AP value box | Numeric keypad opens; enter a target apoapsis, ENT. Value shows cyan until echoed, then green. CANCEL closes without change | Keypad |
| ORAP-06 | Arm [AP] with the new target, EXEC | Burn raises apoapsis to the entered value (check on ORBIT after) | AP burn |
| ORAP-07 | Repeat with [PE] and [INC] | Same, periapsis and inclination change respectively | PE / INC burns |
| ORAP-08 | Set WARP AUTO, arm a node, EXEC, wait for alignment | Button relabels `WARP`; a second tap warps to ignition minus the lead. The executor never warps on its own | Auto-warp |
| ORAP-09 | Set STAGE AUTO and run a burn that exhausts a stage | Executor stages and continues | Auto-stage |
| ORAP-10 | Tap [A/P OFF] mid-burn | Throttle cut, mode cleared, banner reads `A/P OFF`, sidebar key returns to achromatic | Abort |
| ORAP-11 | With a target set within a few km, tap [APPR] and enter a rate, HOLD AT a distance | Approach-rate hold engages; RANGE row closes to the hold distance and stops | Approach hold |
| ORAP-12 | Attempt any of the above on **Info 1** | Not possible: Info 1 has no `A/P` key and cannot send commands | Unit roles |

---

## 10. Session TGT — rendezvous and target (T1 approaching T2)

| ID | Action | Expected | Source |
|---|---|---|---|
| TGT-01 | Set T2 as target from > 2 km | Info 2 remains on ORBIT (or MANEUVER if a node is due). TARGET reachable by hand | Rule 9 outer bound |
| TGT-02 | Press `TGT` manually from > 2 km | RPOD scope + rows Dist, V.Close, Brg / Elv, V.Brg / V.Elv, T+Int | TGT |
| TGT-03 | Close to inside 2000 m | Info 2 auto-routes to TARGET; stays until inside 200 m (DOCK) or back out past ~2400 m | Rule 9 + release |
| TGT-04 | Closing on the target | V.Close **negative**, T+Int shows Dist ÷ |V.Close|; opening → V.Close positive and T+Int dashed | Signed closure |
| TGT-05 | Point the nose at the target | TGT marker at centre; VEL marker at centre when relative velocity runs along the boresight | Nose-referenced |
| TGT-06 | Dist under 200 m | Dist white-on-green | TGT dist band |
| TGT-07 | Clear the target | `NO TARGET SET` fullscreen on TARGET and DOCKING; NAV still draws with dashed target rows | Splash |
| TGT-08 | **Info 1** PFD with target set | Target marker drawn on the ball; SAS TARGET mode shows on the SAS button | PFD markers |
| TGT-09 | C&W `IMPACT IMM` / `GROUND PROX` in orbit | Both dark (no surface closure) | Sanity |
| TGT-10 | Press `TGT` while > 2 km after auto-routing elsewhere | Lands on TARGET (not DOCK, since outside docking range) | First-press rule |

---

## 11. Session DOCK — docking (T1 to T2)

| ID | Action | Expected | Source |
|---|---|---|---|
| DOCK-01 | Close inside 200 m | Info 2 auto-routes to DOCKING (`AUTO`); Info 1 stays on SPACECRAFT (the two panels never show the same screen) | Rule 5 / 1.1.0 |
| DOCK-02 | Back out to 220 m, then 260 m | Stays on DOCKING at 220 m; releases only past 250 m | DOCK_CTX_RELEASE_M |
| DOCK-03 | Read DOCKING | Approach reticle; rows Dist, T+Dock, V.Close, V.Lat, Brg / Elv, V.Brg / V.Elv, Nos.Off, RCS / SAS | DOCK |
| DOCK-04 | Dist under 200 m / under 50 m | Dist yellow under 200 m, white-on-red under 50 m; V.Close alarm band tightens inside 100 m | DOCK_DIST_WARN/ALARM |
| DOCK-05 | Translate right with RCS at zero roll | Green VEL marker moves right; translate left to centre it (marker up-right → thrust left-down) | Relative velocity marker |
| DOCK-06 | Roll the craft 90° and repeat | Marker still moves in the craft's body axes (screen-up is the roof), not the horizon | −roll rotation |
| DOCK-07 | SAS modes | Button: TARGET = green, STAB = cyan, OFF = white-on-red, other modes red | DOCK SAS palette |
| DOCK-08 | Approach-path error past 90° | V.Brg / V.Elv read `---` | DOCK rows |
| DOCK-09 | Dock | `DOCKED` fullscreen (white-on-green) on DOCKING; C&W `DOCK` vertical indicator lit in the regime column; RES and C&W redraw for the new combined vessel | VESSEL_CHANGE |
| DOCK-10 | Undock | DOCKED splash clears; C&W `DOCK` clears; the deferred dock-check re-runs the ladder once target distance arrives | Deferred dock check |
| DOCK-11 | Switch vessel with `]` at close range | Info 2 re-routes to DOCKING for the new active vessel once TARGETINFO arrives | Deferred dock check |
| DOCK-12 | **RES** after docking | Layout for the new vessel name loads (default if unseen); tapes re-collapse for the resources aboard the combined craft | Per-vessel memory |

---

## 12. Session EVA — Kerbal on EVA (T6)

| ID | Action | Expected | Source |
|---|---|---|---|
| EVA-01 | EVA a Kerbal in orbit with no target | **Info 1** stays on SPACECRAFT: ball unchanged, rows 2–6 swap to Alt.Rdr, V.Srf, Dist, V.Close, EC; row 1 pinned to V.Orb. Dist / V.Close dashed | 1.7.0 |
| EVA-02 | Info 2 | Auto-routes to TARGET showing `NO TARGET SET` | Rule 10 |
| EVA-03 | Target the craft from the Kerbal | Dist / V.Close populate on the PFD; TARGET scope tracks the craft | EVA rows |
| EVA-04 | Suit EC | EC row yellow under 20%, white-on-red under 5% | ROVER EC thresholds |
| EVA-05 | **C&W** | `EVA ACTIVE` lit; vessel name shows the Kerbal | Bit 24 |
| EVA-06 | **RES** | Content swaps to the EVA layout: one large 270° ring for EVA Propellant, four small rings for EC, O2, Food, Water. Alert strip vessel name is the Kerbal's in grey. Sidebar still works | ScreenEVA |
| EVA-07 | Use the jetpack | EVA Propellant ring drains clockwise; trend arrow / TTE respond | EVA gauges |
| EVA-08 | Board the craft | RES restores the previous vessel layout including its bugs; Info 1 rows revert; C&W `EVA ACTIVE` clears; Info 2 returns to the ladder's choice | Snapshot / restore |
| EVA-09 | EVA a Kerbal standing on the Mun (later, DESC session) | Info 2 goes to VEHICLE INFO, not TARGET (surface rule outranks EVA) | Rule 6 vs 10 |

---

## 13. Session ENTR — re-entry (T1 returning to Kerbin)

| ID | Action | Expected | Source |
|---|---|---|---|
| ENTR-01 | Burn retrograde to put Pe at ~30 km, still above the atmosphere | Info 2 auto-routes to RE-ENTRY (`ENTR`) before entry interface | Rule 2 |
| ENTR-02 | Read RE-ENTRY | Corridor tape, atmosphere-density bar, G meter, heat-shield / retrograde alignment ball; rows T+Atm (above the atmosphere), Alt.SL, V.Srf, V.Vrt, PeA, Mach, Drogue, Main, Gear / SAS | ENTR layout |
| ENTR-03 | Cross 70 km | Row 0 toggles T+Atm → T+Grnd; row 1 toggles Alt.SL → Alt.Rdr when the atmosphere state changes; C&W `ATMO` and `DESCENT` light (caution chirp) | Row toggles |
| ENTR-04 | Turn SAS off above Mach 3 | SAS button white-on-red; below Mach 3 it drops to normal | SAS Mach gate |
| ENTR-05 | Peak heating | C&W `HIGH TEMP` red + MASTER ALARM above 90% of the part limit; TMAX / TSKIN cells yellow from 50%, red at 90% | tempAlarm |
| ENTR-06 | G meter through the pulse | Graphical G meter climbs; C&W `HIGH G` if above 9 g | G meter |
| ENTR-07 | Point the heat shield off retrograde | Alignment ball shows the error in body axes | Retro ball |
| ENTR-08 | C&W `CHUTE ENV` | Red while too fast for any chute, yellow when drogue-safe, green when mains are safe | Bit 22 |
| ENTR-09 | Fire CAG 1 (drogue) at a safe speed | Drogue row `STOWED` → `ARMED` → `OPEN`. CAG 2 cuts it | Chute state machine |
| ENTR-10 | Fire CAG 3 (main) | Main row follows the same ladder; CAG 4 cuts | Chute CAGs |
| ENTR-11 | Descending under chutes | `IMPACT IMM` lights under 60 s to impact (caution tone); `ALT` lights under 200 m; `GEAR UP` lights under 200 m if gear is up (chirp) | Bits 10, 11, 13 |
| ENTR-12 | **Info 1** during re-entry | SPACECRAFT PFD, row 1 back on `V.SRF` | Vel ref |
| ENTR-13 | GPWS lander profile with panel GREEN (audio) | Metre callouts 2500 / 1000 / 500 / 100 / 50 / 40 / 30 / 20 / 10; SINK RATE only if time-to-impact < 10 s **and** descending faster than 6 m/s | gpwsUpdateLander |
| ENTR-14 | Splashdown or touchdown | C&W `SPLASH` or `LANDED` and `CONTACT` tiles lit. Info 2 routes to VEHICLE INFO (no target) and shows `SPLASHED` / `LANDED` with the recoverable qualifier, not in place of the situation | Rule 6 / 1.10.17 |
| ENTR-15 | Recover the vessel | All panels go to standby | Scene change |

---

## 14. Session DESC — powered descent (T3 landing on the Mun, or T1 propulsive landing)

| ID | Action | Expected | Source |
|---|---|---|---|
| DESC-01 | De-orbit at the Mun; descending faster than 5 m/s under 10 km radar altitude | Info 2 auto-routes to POWERED DESCENT (`DESC`); on an airless body RE-ENTRY never appears | Rule 3 |
| DESC-02 | Climb back above 12 km (or arrest the descent) | Releases only past the widened band (12 km) | Release band 1.11.5 |
| DESC-03 | Read POWERED DESCENT | Altitude tape, X-pointer, ATT bullseye, V.Vrt bar, ground-track compass; rows V.Vrt, T+Grnd, Alt.Rdr, Stg.Brn, Fwd / Lat, ΔV.Stg, Throttle / RCS, Gear / SAS | DESC layout |
| DESC-04 | Descend with lateral drift | Fwd / Lat show roll-corrected drift in the craft heading frame; their thresholds tighten as T+Grnd shrinks | Fwd/Lat tiers |
| DESC-05 | Radar altitude under 200 m, then under 50 m | Alt.Rdr yellow at 200 m (this screen's tighter tier), white-on-red at 50 m; C&W `ALT` at 200 m | LNDG_ALT_RDR_WARN_M / ALT_RDR_ALARM_M / CW_ALT |
| DESC-06 | T+Grnd under 10 s with gear up | T+Grnd and V.Vrt white-on-red; C&W `GROUND PROX` red + MASTER ALARM. Lower the gear: GROUND PROX clears | LNDG_TGRND_ALARM_S / CW_GROUND_PROX |
| DESC-07 | Descending over 5 m/s under the landing tiers | V.Vrt bar caution below 5 m/s down, alarm below 8 | Landing tiers |
| DESC-08 | GPWS lander profile (audio, panel GREEN) | HORIZONTAL SPEED when lateral speed exceeds the altitude-ramped limit under 400 m; RETARD under 15 m with throttle still up; metre ladder callouts | Lander callouts |
| DESC-09 | GPWS bug: set a threshold on the GPWS input panel, cross it gear up | GROUND PROXIMITY (clip 34); gear down → the bug tone | Bug decision tree |
| DESC-10 | Touchdown | C&W `LANDED` + `CONTACT`; Info 2 to VEHICLE INFO; `DESCENT` clears | Rule 6 |
| DESC-11 | **RES** during the landing burn | LF / LOx stage column drains ahead of the total; alert strip shows any caution worst-first | Stage split |

---

## 15. Session LDAP — Landing Autopilot console (Info 2 only)

| ID | Action | Expected | Source |
|---|---|---|---|
| LDAP-01 | Suborbital or descending at the Mun, press `A/P` | Lands on LANDING AUTOPILOT, key caption `LDAP` | Situation pick |
| LDAP-02 | Read the grid | DESCENT [DESC] [HOVR] [BRAKE] [ENTRY] with rate / altitude / ign alt / AOA boxes; OPTIONS ATT REF, TWR, MARGIN, ROLL; DESCENT DATA RDR ALT, V/S, H SPD, ACCEL, IGN ALT, T-IMP, THRTL | Screen_LNDG_AP header |
| LDAP-03 | Tap [DESC], enter a rate | Descent-rate hold engages (green after echo); V/S holds at the setpoint; banner lists `DESC` and `ENGAGED` | DESC hold |
| LDAP-04 | Tap [HOVR], enter an altitude | Radar-altitude hold; RDR ALT settles at the setpoint | HOVR |
| LDAP-05 | Arm [BRAKE] high up | IGN ALT row computed; banner shows `IGN IN m:ss`, then `FIRING` as the suicide burn runs; IGN ALT turns orange when the margin is marginal | Armed suicide burn |
| LDAP-06 | Tap ATT REF | Toggles RETRO / RADIAL | Options |
| LDAP-07 | Set TWR override, MARGIN | Boxes accept keypad values, cyan until echoed | Options |
| LDAP-08 | On a Kerbin re-entry, tap [ENTRY] with an AOA and ROLL | Re-entry attitude hold engages | ENTRY |
| LDAP-09 | Tap [A/P OFF] | Every mode cleared, banner `A/P OFF`, sidebar key achromatic | Abort |
| LDAP-10 | Move the stick while a mode is engaged | Mode disconnects with an orange reason in the banner (e.g. `STICK`) for ~5 s | Disconnect reason |

---

## 16. Session ACFT — aircraft (T4, vessel type Plane)

| ID | Action | Expected | Source |
|---|---|---|---|
| ACFT-01 | T4 on the runway | Info 2 shows the LAUNCH pre-launch board (spaceplanes included); Info 1 on AIRCRAFT PFD | Rule 1 |
| ACFT-02 | Take off and climb | **Info 1** AIRCRAFT; **Info 2** NAVIGATION once flying with Ap below 70 km. Dismissing the pre-launch board by tap also works | Vehicle rule 2 / Mission rule 4 |
| ACFT-03 | Read AIRCRAFT | EADI ball, aircraft symbol, Hdg / Pitch / Roll; right column Alt.SL (Alt.Rdr under 750 m, `RDR`/`SL` chip), V.Srf, IAS with 1–3 chevron trend, V.Vrt, Ma / G split, AoA / Slip split, GEAR / AIRBRK / BRAKES buttons | ACFT layout |
| ACFT-04 | Tap the `RDR` / `SL` chip | Pins the other altitude reference; tap again for automatic | Ref chip |
| ACFT-05 | Toggle CAG 38 | AIRBRK reads `OUT` in cyan, `IN` when stowed | AIRBRAKE_CAG |
| ACFT-06 | Gear up / down, wheel brakes | GEAR and BRAKES buttons follow | Live |
| ACFT-07 | Cruise descent at −6 m/s with gear up above 750 m | V.Vrt and the VSI bar stay green (no landing tiers in cruise) | 1.13.1 |
| ACFT-08 | Same descent with gear down | V.Vrt caution below 5 m/s down, alarm below 8 | Approach-only tiers |
| ACFT-09 | Pull AoA past 10°, then 20° | AoA yellow, then white-on-red; GPWS STALL buzzer sounds unbroken at 20° while above the stall minimum speed | KCM_AOA_WARN/STALL |
| ACFT-10 | Read NAVIGATION | Compass card with own-ship symbol, green ground-track marker with line, violet target-bearing marker, legend bottom-left; heading box; left TRK, DRIFT, BRG; right DIST, V.CLOSE, T+INT | NAV |
| ACFT-11 | Fly a crab (rudder) | DRIFT shows the crab angle, yellow past 10° | NAV_DRIFT_WARN_DEG |
| ACFT-12 | Below 5 m/s (taxi) | TRK and DRIFT dashed | NAV_TRK_MIN_MS |
| ACFT-13 | Target a flag on the ground | BRG, DIST, V.CLOSE, T+INT populate; T+INT only while closing | NAV target rows |
| ACFT-14 | Fly toward the flag to 1.5 km, then 150 m | Info 2 **stays on NAVIGATION** (NAV outranks TARGET and DOCKING for an aircraft) | Rule 4 above 5/9 |
| ACFT-15 | Climb so Ap exceeds 70 km (spaceplane) | Info 2 leaves NAV for LAUNCH / ORBIT; Info 1 leaves AIRCRAFT for SPACECRAFT once out of the atmosphere | Ap gate |
| ACFT-16 | Land and roll out | Info 2 to VEHICLE INFO on the surface; Info 1 keeps AIRCRAFT | Rule 6 / vehicle rule |
| ACFT-17 | Set the control-mode switch to Aircraft | C&W control-mode tile `PLN` green with the plane icon; red if left on Spacecraft | Mode tile |
| ACFT-18 | **RES** press SEL, tap ACFT preset | Set becomes EC, LF, Intake Air, MP, O2, Food, Water (collapsed to those aboard) | ACFT preset |
| ACFT-19 | Type the same plane as Probe (control test) | Info 1 shows SPACECRAFT, Info 2 ORBIT — expected by design | Type rule |

### 16.1 GPWS aircraft profile (audio, C&W panel)

Set the GPWS input panel to GREEN unless stated.

| ID | Action | Expected | Source |
|---|---|---|---|
| GPWS-01 | Takeoff roll | `V1` at 55 m/s, `ROTATE` at 65 m/s (or the values set in TUNABLES), each once | Extras |
| GPWS-02 | Positive climb, gear still down | `GEAR UP` reminder ~6 s after liftoff, once | Advisory |
| GPWS-03 | Level off then sink during climb-out under ~450 m | `DON'T SINK` twice, then only as the loss deepens | Mode 3 |
| GPWS-04 | Descend steeply under 747 m | `SINK RATE`, then `WHOOP WHOOP PULL UP` as the rate crosses the inner envelope | Mode 1 |
| GPWS-05 | Fly level toward rising terrain | `TERRAIN, TERRAIN` then PULL UP; TERRAIN keeps repeating after PULL UP exits while clearance still falls | Mode 2 |
| GPWS-06 | Gear up under 500 ft, 15 s after liftoff | `TOO LOW, GEAR` | Mode 4A |
| GPWS-07 | Gear up, under 1000 ft, fast | `TOO LOW, TERRAIN` | Mode 4A speed-expanded |
| GPWS-08 | Bank steeply at low altitude | `BANK ANGLE, BANK ANGLE` | Mode 6 |
| GPWS-09 | Approach with gear down, descending through the feet ladder | 2500 … 10, 5 callouts once each; `RETARD` under 20 ft with throttle still up, stops at idle | Mode 6 / RETARD |
| GPWS-10 | Bug set at 150 m, gear down, aircraft | `APPROACHING MINIMUMS` ~30 m above, `MINIMUMS` at the bug, no tone | DH_SPLIT_M |
| GPWS-11 | Bug set at 500 m, gear down | Generic tone only | ≥ 300 m bug |
| GPWS-12 | Bug crossed gear up | `GROUND PROXIMITY` | Gear-up rule |
| GPWS-13 | Panel AMBER + proxAlarm, repeat GPWS-04 and GPWS-09 | No warnings; callouts, RETARD and the bug outcome still sound | Amber profile |
| GPWS-14 | Panel AMBER + rdvRadar with a target set, close through 500 … 5 m | Distance callouts in metres; bug tone on range | Distance ladder |
| GPWS-15 | Panel OFF | Silent | OFF |
| GPWS-16 | Master alarm active at the same time as a callout | Both sound: tone on the amp, voice on the DFPlayer | Independent paths |

---

## 17. Session ROVR — rover (T5, vessel type Rover)

| ID | Action | Expected | Source |
|---|---|---|---|
| ROVR-01 | Load T5 on Kerbin | **Info 1** ROVER (recoverable does not override it); **Info 2** VEHICLE INFO on the surface (TARGET if a target is set) | Vehicle rule 1 / 1.4.1 |
| ROVR-02 | Read ROVER | Compass card with heading box, cardinal ring, rover icon, target-bearing triangle when a target is set; DIST / T+TGT strip along the bottom; left V.Srf, ENDUR, BRAKES / GEAR / SAS; top corners FWD / REV; right ALT.TRN, Pitch, Roll tilt indicators | ROVR |
| ROVR-03 | Drive forward, then reverse, then coast | FWD lit, then REV lit, both muted at neutral | Wheel throttle |
| ROVR-04 | Drive on batteries with no generation | ENDUR shows time to empty; `CHG` while gaining; `---` when steady | 1.11.2 |
| ROVR-05 | Target the flag | Bearing triangle on the card; DIST and T+TGT in the strip (T+TGT only while closing) | Target strip |
| ROVR-06 | Climb / traverse a slope | Pitch and Roll tilt indicators follow; ALT.TRN equals Alt.SL − Alt.Rdr | Tilt |
| ROVR-07 | Brakes, SAS | Buttons follow controller state | Live |
| ROVR-08 | **Info 2** from VEHICLE INFO, set a target 1.5 km away | Stays on VEHICLE INFO / TARGET per rule 6 (surface rule outranks the approach window) | Rule 6 |
| ROVR-09 | C&W control-mode tile with the switch on Rover | `RVR` green with the rover icon | Mode tile |
| ROVR-10 | Crest a hill | Info 2 does **not** flip to LAUNCH while briefly climbing | Rover exclusion |
| ROVR-11 | **RES** press SEL, tap SRF preset | EC, Stored Charge, Ore, LF, LOx, MP, O2, Food, Water (collapsed to those aboard) | SRF preset |

### 17.1 Rover Autopilot console (Info 2)

| ID | Action | Expected | Source |
|---|---|---|---|
| RVAP-01 | With T5 active press `A/P` | Lands on ROVER AUTOPILOT, caption `RVAP`; repeated presses cycle RVAP alone for a landed rover | Console ring |
| RVAP-02 | Read the grid | DRIVE [CRUISE] [HDG] [TGT] [FOLLOW]; GUARD LIMITS SPEED, SLOPE, ROLL, STOP; DRIVE DATA SPEED, HDG, TGT BRG, DIST, PITCH, WHL THR, BRAKES | Screen_ROVR_AP header |
| RVAP-03 | Tap [CRUISE], enter 10 | Cruise engages (green after echo); WHL THR commanded; SPEED holds ~10 m/s. Enter −5: reverses | Signed cruise |
| RVAP-04 | Tap [HDG], enter a heading | Rover steers to it; engaging [TGT] drops HDG (exclusive) | HDG / TGT exclusive |
| RVAP-05 | [TGT] with the flag targeted, STOP set to 20 m | Drives to the flag, stops at 20 m with brakes on | TGT + STOP |
| RVAP-06 | [FOLLOW] with a moving target and a range | Holds range to the target | FOLLOW |
| RVAP-07 | Drive up a steep grade with SLOPE limit set low | Banner `SLOPE LIMIT`, SPEED and PITCH readouts orange, cruise setpoint scaled down | Slope guard |
| RVAP-08 | Exceed the ROLL limit | Autopilot disconnects and brakes; banner shows the reason in orange | Roll guard |
| RVAP-09 | Tap [A/P OFF] | All modes clear, key achromatic | Abort |

---

## 18. Session ASCAP — Ascent Autopilot console (T1 on the pad, Info 2)

| ID | Action | Expected | Source |
|---|---|---|---|
| ASCAP-01 | Press `A/P` on the pad | ASCENT AUTOPILOT console, caption `ASC`. Banner `DISARMED`, phase `IDLE` | Console |
| ASCAP-02 | Read the grid | MISSION: target apoapsis, inclination, launch N/S; VEH PROFILE: loft, roll hold, max-G, ARM / DISARM; GUIDANCE: commanded pitch / heading / throttle, G, Q, ApA, PeA | Three columns |
| ASCAP-03 | Tap the apoapsis box, enter 100000, ENT | Value cyan until the master echoes it back, then normal | Pending cue |
| ASCAP-04 | Tap launch N/S | Toggles; also cyan until echoed | Toggle field |
| ASCAP-05 | Tap ARM | Button shows `ARMING…` with a cyan border and the banner `...`; flips to ARMED only on the master's echo. Sidebar `ASC` key turns green at the same moment (not on the tap) | 1.3.0 / 1.1.3 |
| ASCAP-06 | Stage | Autopilot flies VERTICAL → GRAVITY TURN → COAST → CIRCULARIZE → COMPLETE; phase banner and ARM button colour follow each phase | Phases |
| ASCAP-07 | Edit max-G mid-ascent | Accepted and echoed; guidance respects it | Edit any time |
| ASCAP-08 | Tap DISARM mid-flight | `DISARMING…` then DISARMED on echo; key goes achromatic only then | Echo-confirmed |
| ASCAP-09 | While armed, press `PFD` and look at the sidebar | `ASC` key remains green while on other screens | Live-state key colour |
| ASCAP-10 | Unplug or reset the master mid-command (optional) | Pending cue never turns green on its own; queue does not falsely confirm | Round-trip rule |

### 18.1 Aircraft Autopilot console (T4 in flight, Info 2)

| ID | Action | Expected | Source |
|---|---|---|---|
| ACAP-01 | In the air with T4, press `A/P` | Lands on AIRCRAFT AUTOPILOT, caption `ACAP`; repeated presses cycle ACAP → ASC → ORAP → LDAP | Console ring |
| ACAP-02 | Read the grid | PITCH [ATT] [AOA] [V/S] [ALT] [GS]; LATERAL / THRUST [ROLL] [HDG] [NAV] [IAS] [MACH], [LVL] [A/P OFF]; FLIGHT DATA PITCH, ROLL, HDG, V/S, ALT, DIST, TGT EL | Screen_ACFT_AP header |
| ACAP-03 | Tap [ALT] | Captures the current altitude as the setpoint; button cyan then green; banner lists `ALT` | Capture on engage |
| ACAP-04 | Tap the ALT box, enter a new altitude | Aircraft climbs / descends to it | Setpoint edit |
| ACAP-05 | Tap [HDG], enter a heading; [IAS], enter a speed | Both hold; `LEVER OFF` in grey if the physical throttle is not following the autothrottle | Autothrottle |
| ACAP-06 | Tap [LVL] | Wings level, V/S 0 | LVL |
| ACAP-07 | Target a flag, tap [NAV] then [GS] | NAV banks to the bearing, GS holds the depression angle; DIST and TGT EL live | Approach modes |
| ACAP-08 | Move the stick | Modes disconnect; banner shows `STICK` in orange for ~5 s | Disconnect reason |
| ACAP-09 | Climb out of the atmosphere with IAS engaged | Disconnect reason `NO ATMO` | Reason |
| ACAP-10 | Tap [A/P OFF] | All modes clear | Abort |

---

## 19. Session RES — Resource Display (any crewed vessel, best done in orbit with T1)

### 19.1 Main screen and meters

| ID | Action | Expected | Source |
|---|---|---|---|
| RES-01 | Read the Main screen | One tape per resource aboard, grouped PWR / PROP / NUC / MISC / LS / AGR with bracketed labels and 1 px dividers; shared 0–100% axis inboard of the sidebar; alert strip across the top | Main |
| RES-02 | Each meter | Group label, tape in the resource colour, limit-band column (red / yellow at the bottom for consumables, top for waste), ticks every 5 / 10%, label, counter row, units counter (1.23 / 12.3 / 123 / 1234 / 12.3k) | Meter anatomy |
| RES-03 | LF, LOx, SF, Xenon, Ablator | Split tape: wide total column plus narrow half-bright stage column with a white stage line | Stage split |
| RES-04 | Burn the engine | Trend arrow falls on LF / LOx; arrow clears when steady | Trend |
| RES-05 | Press TTE | Counter row changes from percent to time-to-empty (`4:35`, `42m`, `5.5h`, `4d 3h`, `27d`, `---` when steady); key reverse-videos; press again to restore | TTE |
| RES-06 | TTE under warp | Estimate stays in game time (does not shrink with the warp rate) | Warp correction |
| RES-07 | Drain a consumable under 20% then 5% | Frame and counter yellow, then white-on-red; strip shows `XX CAUT` then `XX LOW`; new alarm tile flashes for ~3 s | RES_WARN / ALARM |
| RES-08 | Hover at the threshold | No flicker: 1% hysteresis to leave a state | ALERT_HYST_FRAC |
| RES-09 | Waste resource (TAC-LS CO2) filling | Bands at the top; `CO2 HIGH` at 95% | WASTE_* |
| RES-10 | Tap a meter | Detail opens on that resource | Touch |
| RES-11 | Tap a message in the alert strip | Detail opens on that resource | Strip tap |
| RES-12 | Balance cell in the strip with LF and LOx aboard | Shows the propellant balance | Strip |
| RES-13 | More than nine meters (ADV preset with CRP) | Meters switch to the compact class; pitch spreads across the full area | Spacing classes |
| RES-14 | Vessel switch | Meters zero, presence resets to `...` then `---` for anything not answering within 3 s | REFRESH_TIMEOUT_MS |

### 19.2 Reserve bugs

| ID | Action | Expected | Source |
|---|---|---|---|
| BUG-01 | Hold still on the LF tape for 1 s at ~40% | Cyan bug appears snapped to the 5% grid with its percent beside it and a mark on the band | BUG_HOLD_MS / SNAP |
| BUG-02 | Tap (not hold) the tape | Opens Detail, no bug set | Tap vs hold |
| BUG-03 | Hold on the bug and drag | Bug follows in 1% steps after 12 px of travel; release does nothing else | Drag |
| BUG-04 | Hold still on the bug for 1 s | Bug clears | Hold-to-clear |
| BUG-05 | Drain LF below the bug | Frame and counter turn cyan, `LF BUG` in the strip; never raises an alarm; limit bands outrank it | Bug crossing |
| BUG-06 | Press CLR BUG | Every bug on the vessel removed (orange guard treatment on the key) | CLR BUG |
| BUG-07 | Set a bug, wait 30 s, power-cycle the controller, reload the vessel | Bug is still there | PERSIST_SETTLE_MS |

### 19.3 Select screen

| ID | Action | Expected | Source |
|---|---|---|---|
| SEL-01 | Press SEL | Grid of resources, presets SPCT / XPD / SRF / ACFT / LSP / ADV, ORDER list, DFLT, CLEAR, BACK. EVA Propellant hidden when not on EVA | Select |
| SEL-02 | Tap a resource to add, tap again to remove | ORDER list updates in subsystem order (LF always left of LOx) | Group sort |
| SEL-03 | Add past 16 slots | Counter flashes yellow `MAX`; slot not added | MAX |
| SEL-04 | Tap SPCT | Set replaced by the nine-resource spacecraft set | Preset |
| SEL-05 | Tap DFLT | Current set with its bugs becomes the default for unseen vessels; strip name for this vessel turns cyan (from memory) | DFLT |
| SEL-06 | Load a never-seen vessel | Starts from the DFLT layout, name in grey | Default |
| SEL-07 | CLEAR, then DFLT | Stored default dropped, panel falls back to the SPCT preset | CLEAR + DFLT |
| SEL-08 | CLEAR, BACK, then switch vessel | That vessel is forgotten and starts from the default next time | Forget one |
| SEL-09 | Hold CLEAR 3 s | Countdown in the counter area; lift early to cancel; hold through: every vessel forgotten, default kept | MEM_CLEAR_HOLD_MS |
| SEL-10 | Build a layout, wait 30 s, switch away and back | Layout recalled, name cyan; cache holds 20 vessels in recency order | Per-vessel memory |
| SEL-11 | Two vessels with the same name | Share one record (by design) | Name key |

### 19.4 Detail screen

| ID | Action | Expected | Source |
|---|---|---|---|
| DET-01 | Press DATA or tap a meter | Left selector column, one key per active slot (dimmed if not aboard); right panel header, rows AVAIL, TOTAL, REM, RATE, TTE (TTF for waste) | Detail |
| DET-02 | Number formatting | Thousands separators; two decimals under 1,000, one under 10,000, none above; RATE flips to per-minute / per-hour when per-second would round to zero | Formatting |
| DET-03 | Level history trace | Ten-minute trace drawn with the bug in cyan, captioned with the game time it spans | DETAIL_HISTORY |
| DET-04 | Bug bar keys −10 / −1 / +1 / +10 / CLR | First step on a resource without a bug starts one at the caution fraction; keys move it in exact percent; CLR removes it; Main reflects it on return | Bug bar |
| DET-05 | Tap another selector key | Switches resource; BACK returns to Main | Selector |

---

## 20. Session CTX — context ladder cross-checks (run alongside the sessions above)

These confirm the ladder guards. Tick them as the situations occur.

| ID | Situation | Expected | Source |
|---|---|---|---|
| CTX-01 | Any automatic switch | At least 4 s between automatic switches; no visible flapping at any threshold | CONTEXT_DWELL_MS |
| CTX-02 | Every pairing observed during the run | Info 1 and Info 2 never show the same screen at the same time | 1.5.0 |
| CTX-03 | Vessel switch between two craft in different situations | Each panel re-routes for the new vessel; manual latches cleared | Vessel change |
| CTX-04 | Lander descending from Mun orbit (Pe below the surface, V.Vrt negative) | Info 2 does **not** read LAUNCH on the way down | Rule 7 V.Vrt test |
| CTX-05 | Top of a coast, V.Vrt passing through zero to −20 m/s | Info 2 stays on LAUNCH (release band) | LNCH_CTX_VVERT_RELEASE_MS |
| CTX-06 | Manual screen held, then the ladder's answer changes (e.g. node comes due) | `MAN` latch releases and the new automatic screen appears | Latch release on ladder change |
| CTX-07 | Circularisation node planned during the LAUNCH arc | Info 2 stays on LAUNCH / CIRC, not MANEUVER | Rule 7 above rule 8 |
| CTX-08 | Spaceplane on the runway | Pre-launch board appears (vessel type does not mask it) | Rule 1 |

---

## 21. Defect log template

| ID | Panel / screen | Firmware | Test point | Observed | Expected | Repro steps | Severity |
|---|---|---|---|---|---|---|---|
| D-01 | | | | | | | |

**Severity:** A = wrong or misleading flight data / lost command; B = wrong screen or state annunciation; C = cosmetic (overlap, trail, colour).

---

## Appendix A — threshold cheat sheet

| Quantity | Caution | Alarm | Where |
|---|---|---|---|
| Positive G | 4 g | 9 g | ASCENT G row, PFD, C&W HIGH G |
| Negative G | −2 g | −5 g | Same |
| Stage ΔV | — | 150 m/s | ΔV.Stg, C&W LOW ΔV |
| Stage burn time | — | 60 s | Stg.Brn, C&W LOW ΔV |
| Time to ground, gear up | — | 10 s | T+Grnd, C&W GROUND PROX, GPWS lander SINK RATE |
| Dynamic pressure (ascent) | 20 kPa | 40 kPa | ASCENT Q |
| Angle of attack | 10° | 20° | AIRCRAFT AoA, GPWS STALL |
| Docking distance | 200 m | 50 m | DOCK Dist |
| Docking / target context | enter 200 m, leave 250 m | — | Mission rule 5 |
| Target context window | 200–2000 m (release 150 / 2400) | — | Mission rule 9 |
| Maneuver context | T+Ign < 600 s (release 700) | — | Mission rule 8 |
| Powered-descent context | < 10 km radar, faster than −5 m/s (release 12 km) | — | Mission rule 3 |
| Re-entry context | Pe in atmosphere and above it or Mach > 3 | — | Mission rule 2 |
| Radar altitude | 500 m (200 m on POWERED DESCENT) | 50 m | Alt.Rdr on DESC / ACFT |
| Surface altitude | 200 m | — | C&W ALT / GEAR UP |
| Impact imminent | 60 s | — | C&W IMPACT IMM |
| Resource level (consumable) | 20% | 5% | RES bands, C&W PROP LOW / RCS LOW / BUS VOLTAGE |
| Waste resource | 80% | 95% | RES bands |
| EC time remaining | 15 min | 5 min | RES time tier |
| O2 time remaining | 30 min | 10 min | RES, C&W LIFE SUPP |
| Water time remaining | 12 h | 4 h | Same |
| Food time remaining | 72 h | 24 h | Same |
| Part temperature | 50% | 90% | C&W HIGH TEMP, TMAX / TSKIN |
| NAV drift | 10° | — | NAVIGATION DRIFT |
| Reticle angle (DOCK / MNVR) | 5° | 10° | Ring bands |
| Reticle angle (TGT) | 15° | 30° | Ring bands |

## Appendix B — suggested session order

1. BOOT, NAV, PAD, ASCAP (one pad load, then launch under the autopilot)
2. ASC, ORB, MNVR, ORAP, RES (one orbit session)
3. TGT, DOCK, EVA (rendezvous with T2)
4. ENTR (return T1)
5. DESC, LDAP (Mun landing with T3)
6. ACFT, GPWS, ACAP (T4)
7. ROVR, RVAP (T5)
8. CTX rows as they come up across all of the above
