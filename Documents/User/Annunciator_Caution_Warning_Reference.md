# Annunciator — Caution & Warning Reference

**Document type:** User
**Location:** `Documents/User/Annunciator_Caution_Warning_Reference.md`
**Applies to:** KCMk1_Annunciator firmware 3.7.0

The Caution & Warning (C&W) panel is the 5 × 5 grid of indicator tiles on the Annunciator's
Main screen, to the right of the MASTER ALARM button. Every tile is evaluated continuously
from live KerbalSimpit telemetry; the table below is derived from that logic
(`CautionWarning.ino`, `Audio.ino`, `ScreenMain.ino`) and the thresholds in `AAA_Config.ino`
and `KCMk1_SystemConfig.h`.

![Every tile lit](assets/Annunciator_MainLampTest.png)

*Lamp test: every tile in its ON colour. The two-tier tiles (Pe LOW, PROP LOW, LIFE SUPPORT)
and CHUTE ENV show their most severe colour here.*

## How to read the tiles

| Appearance | Meaning |
|---|---|
| Dim grey label on near-black | Condition not present (tile OFF) |
| White on **red** | **Warning** — illuminates MASTER ALARM and sounds the master-alarm tone |
| Dark grey on **yellow** | **Caution** — advisory; some sound a short caution cue, none trip MASTER ALARM |
| White on **green** | Positive / safe state |
| White on **orange** | Active-state indicator (something is happening, no action implied) |
| White on **blue** | Information |

**MASTER ALARM.** The large button top-left lights red whenever any *warning* condition
(red tier) is present, and the master-alarm tone starts. Touching the button silences the
tone while leaving the button and the warning tiles lit. A *new* warning arriving while
silenced restarts the tone; when every warning clears, the tone stops and the silence
latch resets. Master-alarm audio is serviced on every screen, not just Main.

**Audio cues.** All audio is gated by the panel's `audioEnabled` setting (the AUDIO tile in
the bottom-right grid shows its state). Cues fire on the *transition* into a condition, not
continuously, except the master alarm which runs until silenced or cleared.

| Cue | Sound | Used by |
|---|---|---|
| Master alarm | 375 Hz / 1000 Hz alternating at 2.5 Hz (Shuttle-style), continuous | All red-tier warnings |
| Caution tone | 1000 Hz steady, 1.2 s | ALT, IMPACT IMM |
| Caution chirp | Two-note descending tritone, 1200 → 849 Hz, 120 ms each | DESCENT, ATMO, GEAR UP |
| Alert chirp | Two-note ascending, 880 → 1109 Hz, 120 ms each | Not a C&W tile — fires on entering ORBIT, on climbing through 3,500 m or 100 m/s surface speed, and on Ap rising through the body's minimum safe altitude |

## Indicator table

Grid position is row × column, reading left to right from the top-left tile.

| Tile | Definition | Colour / triggering condition | Audio cue |
|---|---|---|---|
| **LOW ΔV** (1×1) | Current stage is nearly spent. | **Red:** stage ΔV below 150 m/s *or* stage burn time below 60 s, while in flight with the throttle open. Suppressed on the pad, with throttle at zero, and for 1.5 s after throttle-up (burn-time estimate is unreliable at ignition). | Master alarm |
| **HIGH G** (1×2) | Acceleration outside the safe envelope. | **Red:** g-load above +9 g or below −5 g. | Master alarm |
| **HIGH TEMP** (1×3) | A part is approaching its thermal limit. | **Red:** hottest part temperature *or* skin temperature above 90 % of its limit. (The TMAX / TSKIN readouts turn yellow from 50 % and red at the same 90 % point.) | Master alarm |
| **BUS VOLTAGE** (1×4) | Electric charge critically low. | **Red:** electric charge below 5 % of total capacity. Requires the Alternate Resource Panel mod; without it the indicator never fires (and never false-triggers). | Master alarm |
| **ABORT** (1×5) | Abort action group has been activated. | **Red:** Abort action group active. | Master alarm |
| **GROUND PROX** (2×1) | Impact imminent. | **Red:** airborne and descending with less than 10 s to the surface at the current descent rate. Gear position is deliberately ignored — a powered landing at this rate still warrants the alarm. | Master alarm |
| **Pe LOW** (2×2) | Periapsis is inside the atmosphere / below terrain. | **Red:** Pe below the body's re-entry altitude (atmospheric bodies, e.g. 45 km at Kerbin) — committed re-entry — or below the body's minimum safe altitude (airless bodies). **Yellow:** atmospheric bodies only — Pe inside the atmosphere but above the re-entry altitude (aerobrake zone). | Master alarm (red tier only) |
| **PROP LOW** (2×3) | Stage propellant running low. | Monitors the lower of liquid-fuel and oxidizer fraction on the current stage. **Yellow:** below 20 %. **Red:** below 5 %. Suppressed when the stage carries no LF/OX tanks. | Master alarm (red tier only) |
| **LIFE SUPPORT** (2×4) | Crew consumables or waste capacity at risk (TAC Life Support). | Time remaining per resource at TAC-LS default rates × crew. **Yellow:** food < 72 h, water < 12 h, oxygen < 30 min, or any waste tank (CO₂, waste, waste water) > 80 % full. **Red:** food < 24 h, water < 4 h, oxygen < 10 min, or any waste tank > 95 % full. Worst resource wins. Crewed vessels only. | Master alarm (red tier only) |
| **O2 PRESENT** (2×5) | Breathable atmosphere. | **Blue:** inside an atmosphere that contains oxygen (jets work, helmets off). | None |
| **IMPACT IMM** (3×1) | Impact approaching. | **Yellow:** airborne and descending with less than 60 s to the surface at the current descent rate. Stays lit alongside GROUND PROX in the final 10 s. | Caution tone |
| **ALT** (3×2) | Low altitude. | **Yellow:** airborne with surface altitude below 200 m. | Caution tone |
| **DESCENT** (3×3) | Vessel is descending. | **Yellow:** airborne with negative vertical speed. | Caution chirp |
| **GEAR UP** (3×4) | Landing gear not deployed near the ground. | **Yellow:** airborne, descending, below 200 m surface altitude, gear retracted. | Caution chirp |
| **ATMO** (3×5) | In atmosphere. | **Yellow:** vessel is inside an atmosphere. | Caution chirp |
| **RCS LOW** (4×1) | Monopropellant running low. | **Yellow:** total vessel monopropellant below 20 %. Suppressed when the vessel has no monopropellant tanks. | None |
| **PROP RATIO** (4×2) | Fuel / oxidizer imbalance on the stage. | **Yellow:** stage LF : OX ratio deviates more than 10 % from the nominal 9 : 11. Only evaluated when the stage carries both. | None |
| **COMM LOST** (4×3) | No CommNet connection. | **Yellow:** CommNet signal strength 0 % while in flight (not on the pad). Also lights during a re-entry plasma blackout. | None |
| **Ap LOW** (4×4) | Apoapsis too low for a sustainable orbit. | **Yellow:** sub-orbital or orbital situation with apoapsis below the atmosphere top (70 km at Kerbin) or, on airless bodies, below the minimum safe altitude. Not evaluated in solar orbit. | None |
| **HIGH Q** (4×5) | Dynamic pressure high. | **Yellow:** dynamic pressure above the body's calibrated threshold. *The threshold is currently 0 (suppressed) for every body in the shared body table, so this tile does not light until thresholds are calibrated from flight test.* | None |
| **SRB ACTIVE** (5×1) | A solid rocket stage is burning. | **Orange:** stage solid fuel is decreasing and between 0.5 % and 99 % full. Holds 3 s after the last decrease; clears when exhausted, when a fresh full stage appears, or if the reading rises. | None |
| **ORBIT STABLE** (5×2) | Orbit clears the atmosphere and stays inside the SOI. | **Green:** situation is ORBIT, both Pe and Ap above the atmosphere top (or minimum safe altitude on airless bodies), and Ap inside the body's sphere of influence. | None (the alert chirp sounds on entering the ORBIT situation) |
| **ELEC GEN** (5×3) | Batteries are charging. | **Green:** electric charge rose on its last reading. Holds 5 s after the last rise, so a full battery does not read as charging. Clears on a falling reading. | None |
| **CHUTE ENV** (5×4) | Parachute deployment envelope (by dynamic pressure, so it is altitude- and body-correct). | Off outside the atmosphere. **Green:** safe for main chutes (below ≈ 250 m/s at Kerbin sea level). **Yellow:** drogue only (≈ 250–500 m/s at Kerbin sea level). **Red:** too fast for any chute. | None |
| **EVA ACTIVE** (5×5) | A Kerbal is on EVA. | **Orange:** the active vessel is a Kerbal on EVA. | None |

## Notes for the manual

- Nine tiles feed MASTER ALARM: LOW ΔV, HIGH G, HIGH TEMP, BUS VOLTAGE, ABORT, GROUND PROX,
  and the red tiers of Pe LOW, PROP LOW and LIFE SUPPORT. No yellow, green, orange or blue
  state ever trips it.
- GROUND PROX and IMPACT IMM cascade on purpose: IMPACT IMM comes up at 60 s to impact and
  stays up while GROUND PROX adds the master alarm inside 10 s.
- Pe LOW, PROP LOW and LIFE SUPPORT each occupy one tile with two severities. The yellow tier
  is advisory; only the red tier sounds or lights MASTER ALARM.
- Thresholds shared with the Info Display (ground-proximity time, g limits, low-ΔV and
  low-burn-time limits, chute limits) are defined once in `KCMk1_SystemConfig.h`, so the
  Annunciator tile and the Info Display readout always change colour at the same point.
- All thresholds quoted here are the firmware defaults and are tunable in `AAA_Config.ino`.
