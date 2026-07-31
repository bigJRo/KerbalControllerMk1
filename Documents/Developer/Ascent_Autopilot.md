# Kerbal Controller Mk1 — Ascent Autopilot

**Status:** Draft
**Project:** Kerbal Controller Mk1
**Organization:** Jeb's Controller Works
**Author:** J. Rostoker
**Runs on:** Master controller (Teensy 4.1), `Software/Controller_Main`

---

## 1. Overview

The ascent autopilot flies a launch-to-orbit profile through the master controller's KerbalSimpit
link: **vertical climb → gravity turn (targeting a commanded inclination) → coast to apoapsis →
circularization burn**. It closes the loop entirely on the master Teensy — telemetry in, steering and
throttle commands out — so no other board is involved.

It is **sphere-of-influence aware**: the profile adapts to whatever body the craft is in, atmospheric
or airless. Airless bodies (Mun, Minmus, Ike, …) automatically drop the aerodynamic guards (AoA limit,
max-Q) and pitch over aggressively; atmospheric bodies fly a gravity turn scaled to the atmosphere.

Files:

| File | Role |
|------|------|
| `ascent_autopilot.h`  | Public API + the `AscentConfig` tuning surface |
| `ascent_autopilot.ino` | Guidance state machine, PID steering, throttle manager, staging |
| `simpit_message_handler.ino` | Registers the extra channels and feeds `apIngest*()` |
| `Controller_Main.ino` | `apInit()` in `setup()`, `apUpdate()`/`apSerialConsole()` in `loop()` |

The module keeps its **own telemetry snapshot**, fed by `apIngest*()` from the Simpit message
handler; it does not depend on the master's other telemetry globals.

---

## 2. Telemetry used

Registered by `registerInputChannels()` and dispatched in `messageHandler()`:

| Channel | Feeds |
|---------|-------|
| `ALTITUDE_MESSAGE`        | surface altitude → pitch schedule / turn trigger |
| `VELOCITY_MESSAGE`        | surface speed → turn trigger, max-Q |
| `APSIDES_MESSAGE`         | apoapsis → cutoff; periapsis → circularization stop |
| `APSIDESTIME_MESSAGE`     | time-to-apoapsis → circularization burn timing |
| `ORBIT_MESSAGE`           | inclination (readback) |
| `ROTATION_DATA_MESSAGE`   | current attitude + surface/orbital prograde → steering & AoA |
| `ATMO_CONDITIONS_MESSAGE` | air density → max-Q; **has-atmosphere → airless/atmospheric branch** |
| `TEMP_LIMIT_MESSAGE`      | skin-temperature fraction → heat limiter |
| `DELTAV_MESSAGE`          | stage ΔV → auto-staging |
| `SOI_MESSAGE`             | current body name → body profile / SoI adaptation |

Outgoing: `ROTATION_MESSAGE` (pitch/yaw/roll), `THROTTLE_MESSAGE`, `STAGE_ACTION`, `SAS_ACTION`,
`setSASMode(AP_PROGRADE)`.

---

## 3. Flight phases

```
IDLE ──apArm()──▶ VERTICAL ──alt/vel trigger──▶ GRAVITY_TURN ──Ap≥target──▶ COAST ──t→Ap≤lead──▶ CIRCULARIZE ──Pe≥target──▶ COMPLETE
                                                     ▲                         │
                                                     └───── relight if Ap decays in atmo ─────┘
   any phase ── telemetry loss > timeout ──▶ ABORT (throttle cut, disarm)
```

- **VERTICAL** — full throttle, hold 90° at the launch azimuth until the turn trigger.
- **GRAVITY_TURN** — pitch program clamped by the AoA limit to surface prograde; managed throttle
  (max-Q / skin-temp / apoapsis taper); auto-staging.
- **COAST** — engines off; prograde held by stock SAS (or actively). Relights if drag pulls the
  apoapsis back down inside the atmosphere.
- **CIRCULARIZE** — prograde burn near apoapsis until periapsis reaches the target.
- **COMPLETE** — throttle 0; stock SAS enabled in **stability-hold** (attitude hold), control handed back to the pilot.
- **ABORT** — failsafe throttle cut on telemetry loss.

While actively steering, stock SAS is switched **off** (raw rotation commands fight SAS); during
coast/circularization the module hands attitude to stock SAS prograde hold.

---

## 4. Body / sphere-of-influence adaptation

With `autoBodyProfile` enabled (default), the guidance adapts to the current SoI:

- **Airless vs. atmospheric** is decided from the `hasAtmosphere` telemetry flag — robust even for
  bodies not in the table below. On airless bodies the AoA limit and max-Q limiter are skipped and the
  craft pitches over freely to build horizontal velocity.
- **Turn-end altitude** (where the pitch program reaches `finalPitch`) is computed body-relative:
  `turnEndAtmoFraction × lowSpace` (atmosphere top) on atmospheric bodies, or
  `turnEndAirlessFraction × targetApoapsis` on airless / unknown-atmosphere bodies.
- **Parking-orbit default** is adopted on each SoI change — unless the pilot has set an explicit target
  (via `apSetTargets()` or the console `ALT` command), which is preserved. Derived as `lowSpace + 10 km`
  (atmospheric) or `minSafe + max(8 km, minSafe/2)` (airless).
- **Minimum-safe-altitude** clamps the target up on arm for terrain clearance (`enforceMinSafeAltitude`):
  `lowSpace + 1 km` (atmospheric) or `minSafe + 5 km` (airless).

### Single source of truth

Body parameters come from the **shared celestial-body table** — `getBodyParams()` /
`BodyParams` in **`Software/Common/body_params.h`**, the same table the display firmware uses (it was
relocated there from the KerbalDisplayCommon library so there is one canonical copy). The autopilot reads
`lowSpace` (atmosphere top), `minSafe` (highest terrain), and `hasAtmo`, and derives its parking-orbit and
safe-altitude figures from them — it adds **no** body data of its own. Bodies not in the table (including
Jool and the Sun) fall back to telemetry-driven behaviour with whatever target you set.

---

## 5. Steering

Attitude error is computed in the navball frame (pitch above horizon, heading), then rotated into the
body frame by the current roll angle so pitch/yaw commands stay correct regardless of roll. Each axis
runs a PID (P + clamped I by default); roll is optionally held at `targetRoll`. The launch azimuth is

```
sin(azimuth) = cos(targetInclination) / cos(launchLatitude)      (clockwise from north)
```

This is the **inertial** azimuth (it ignores the body's surface-rotation velocity). Trim with
`headingBias`, or use `launchSoutherly` for the descending-node solution. Retrograde targets are
approximate.

---

## 6. Configuration & tuning

Fetch a mutable `AscentConfig` with `apGetConfig()` (or the `apSetTargets()` helper) before `apArm()`.
`apDefaultConfig()` starts at 80 km equatorial; with `autoBodyProfile` on, the target adopts the current
body's default orbit on each SoI change unless you set an explicit target.

### Mission targets
| Field | Default | Notes |
|-------|---------|-------|
| `targetApoapsis` | 80000 m | Orbit altitude (also target Pe when circularizing). Auto-set per body unless locked |
| `targetInclination` | 0° | 0–180 |
| `launchLatitude` | 0° | Site latitude for the azimuth math (KSC ≈ 0) |
| `launchSoutherly` | false | Southerly / descending-node azimuth branch |
| `headingBias` | 0° | Manual azimuth trim |

### Body / SoI handling
| Field | Default | Notes |
|-------|---------|-------|
| `autoBodyProfile` | true | Adapt to the current SoI (atmospheric vs airless, per-body defaults) |
| `turnEndAtmoFraction` | 0.80 | Atmospheric: level off by this fraction of atmosphere top |
| `turnEndAirlessFraction` | 0.25 | Airless: pitch over within this fraction of target apoapsis |
| `enforceMinSafeAltitude` | true | Raise target to the body's minimum safe altitude on arm |

### Ascent shape
| Field | Default | Notes |
|-------|---------|-------|
| `turnStartAltitude` | 500 m | Begin pitch-over (surface/AGL) |
| `turnStartVelocity` | 60 m/s | OR speed trigger, whichever first (0 disables) |
| `turnEndAltitude` | 55000 m | Manual turn-end — used **only** when `autoBodyProfile` is off |
| **`loft`** | 1.0 | **Turn aggressiveness exponent.** <1 pitches over fast (high-TWR craft); >1 stays steep longer (low-TWR / draggy craft) |
| `initialPitchKick` | 3° | Immediate pitch-over at turn start to commit the turn |
| `finalPitch` | 0° | Pitch above horizon held to cutoff (0–15) |

### Throttle management
| Field | Default | Notes |
|-------|---------|-------|
| `launchThrottle` | 1.0 | Powered-ascent throttle |
| `autoLaunch` | false | Fire staging once on arm to ignite stage 1 |
| `maxQ` | 0 (off) | Dynamic-pressure limit in Pa; typical KSP ~18 000–25 000 |
| `maxQThrottleFloor` | 0.5 | Lowest throttle the max-Q limiter commands |
| `skinTempLimit` | 0 (off) | Ease off above this skin-temp fraction (e.g. 0.85) |
| `apoTaperStart` | 0.92 | Begin throttle taper at this fraction of target Ap |
| `apoTaperFloor` | 0.10 | Minimum throttle during the taper |

### Steering / authority
| Field | Default | Notes |
|-------|---------|-------|
| `aoaLimit` | 5° | Max angle from surface prograde (0 disables). The key aero-stability guard |
| `pitchK*/yawK*/rollK*` | see source | PID gains |
| `rollControlEnabled` | false | Actively hold `targetRoll` |
| `targetRoll` | 0° | Roll hold target |
| `maxControlDeflection` | 1.0 | Clamp on commanded axis magnitude (precision) |

### Staging / circularization / safety
| Field | Default | Notes |
|-------|---------|-------|
| `autoStage` | true | Fire staging when stage ΔV < `stageDVThreshold` |
| `stageDVThreshold` | 5 m/s | |
| `stageMinInterval` | 2000 ms | Lockout between auto-stagings |
| `circularize` | true | Coast + apoapsis burn to raise Pe to target |
| `circStartLeadTime` | 10 s | Start the burn when time-to-Ap falls below this |
| `circPeTolerance` | 1000 m | Stop when Pe is within this of target |
| `telemetryTimeout` | 2000 ms | Cut throttle + disarm on telemetry loss |
| `useStockSASForCoast` | true | Hold prograde with stock SAS during coast/circ instead of active steering |

---

## 7. Usage

```cpp
// In setup(): apInit() loads apDefaultConfig().

// Configure the mission (before arming):
apSetTargets(90000.0f, 45.0f, 0.9f);      // 90 km, 45° inclination, aggressive loft
AscentConfig &c = apGetConfig();
c.maxQ = 20000.0f;                        // enable max-Q throttling
c.rollControlEnabled = true;              // hold roll

// Engage — arm, then ignite (or set autoLaunch and let arm stage for you):
apArm();

// apUpdate() runs the loop every loop() pass and only commands KSP while armed.
// apDisarm() (or pressing Abort) returns control to the pilot at any time.
```

**Bench testing:** `apSerialConsole()` (already called in `loop()`) accepts line commands on the
primary Serial port: `ARM`, `DISARM`, `STATUS`, `ALT <m>`, `INC <deg>`, `LOFT <x>`.

**Wiring to a panel:** call `apArm()` / `apDisarm()` from a physical arm switch, and read
`apGetStatus()` for a phase/throttle/attitude readout to drive a display.

---

## 8. Assumptions & limitations

- **KSP1 + KerbalSimpit.** Control-axis full-scale is ±`INT16_MAX`; throttle is `0…INT16_MAX`
  (adjust `AP_AXIS_FULL` in `ascent_autopilot.ino` if a Simpit build differs).
- The launch azimuth is inertial (no rotation-velocity correction) — use `headingBias` to trim to an
  exact inclination.
- Circularization is a simple burn-to-target-periapsis, not an optimal minimum-ΔV node burn; it may
  raise apoapsis slightly above target.
- Body data comes from the shared `Software/Common/body_params.h` table (stock KSP1). `lowSpace`
  (atmosphere top) and `minSafe` (highest terrain) are canonical; the autopilot's derived parking-orbit
  and safe-altitude figures are engineering margins — verify before flying, especially on mountainous
  airless bodies. Unknown/modded bodies fall back to telemetry (atmosphere flag) with the target apoapsis
  you set. `launchLatitude` is not available from telemetry — set it for non-KSC sites.
- PID gains are conservative starting values and will want tuning per craft / control authority.
- The master `Controller_Main` sketch is still mid-integration; this module is self-contained and
  compiles independently, but a full sketch build depends on that ongoing work.
