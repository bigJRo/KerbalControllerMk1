/***************************************************************************************
   AAA_Config.ino -- Tunable constants for Kerbal Controller Mk1 Information Display
   Sketch version is defined in KCMk1_InfoDisp.h
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


/***************************************************************************************
   OPERATING MODE
****************************************************************************************/
bool debugMode = false;
bool demoMode  = false;  // true = sine-wave demo values, no KSP required
bool fpsDiag   = false;  // true = print frame-rate / render-time diagnostics to Serial (~1 Hz)

// INFO_DISP_UNIT — which physical Info Display board this firmware image targets.
// The Info Display firmware is identical for both units; only the I2C slave address
// differs so the master can address each board independently on the shared bus.
//   1 = Info Display 1  -> I2C addr 0x12 (KCM_I2C_ADDR_INFODISP)
//   2 = Info Display 2  -> I2C addr 0x13 (KCM_I2C_ADDR_INFODISP_2)
// Set this before flashing each board. The sync/framing byte (0xAE) is shared by
// both units; the INT pin (pin 0) is per-board wiring and does not change here.
// (The System Info Display, addr 0x14, is separate hardware and is future work.)
#define INFO_DISP_UNIT 1

// STANDALONE_TEST: true = no I2C master connected — skip the boot PROCEED handshake
// and enter loop() immediately. Safe to leave true for bench/UI testing; set false
// for production (master will send PROCEED after reading the status packet).
const bool STANDALONE_TEST = false;


/***************************************************************************************
   DISPLAY ROTATION
   0 = normal (connector at bottom), 2 = 180deg (connector at top)
****************************************************************************************/
const uint8_t DISPLAY_ROTATION = 0;


/***************************************************************************************
   PARACHUTE ACTION GROUP ASSIGNMENTS  (LNDG re-entry screen)
   Set to match your vessel's CAG bindings. Set to 0 to disable.
****************************************************************************************/
const uint8_t DROGUE_DEPLOY_CAG = 1;
const uint8_t DROGUE_CUT_CAG    = 2;
const uint8_t MAIN_DEPLOY_CAG   = 3;
const uint8_t MAIN_CUT_CAG      = 4;

// Airbrake — base Custom Action Group 38 (Function Control B4); see
// Documents/Developer/Module_UI_Reference.md. Read for the AIRCRAFT screen's
// AIRBRK indicator. Set to 0 to disable (button always shows stowed).
const uint8_t AIRBRAKE_CAG      = 38;


/***************************************************************************************
   FLIGHT THRESHOLDS — AIRCRAFT (ACFT screen)
   Tune these per-aircraft during flight testing.
****************************************************************************************/

// IAS stall speed (m/s). Yellow below this, white-on-red below half.
// Set to 0.0 to disable stall warning entirely.
const float STALL_SPEED_MS = 0.0f;

// Maximum safe gear-down speed (m/s). Gear DOWN above this → yellow warning.
// Typical KSP aircraft: 150–200 m/s. 160 m/s ≈ 576 km/h.
const float GEAR_MAX_SPEED_MS = 160.0f;

// Roll angle thresholds for aircraft (degrees absolute)
const float ROLL_WARN_DEG  = 60.0f;   // yellow — steep bank
const float ROLL_ALARM_DEG = 90.0f;   // white-on-red — inverted / structural risk

// AoA thresholds (degrees absolute) — cross-panel aligned with the Annunciator GPWS
// STALL callout via KCMk1_SystemConfig.h (same "stall AoA" on both panels).
const float AOA_WARN_DEG  = KCM_AOA_WARN_DEG;   // yellow — approaching stall AoA
const float AOA_ALARM_DEG = KCM_AOA_STALL_DEG;  // white-on-red — beyond stall AoA (GPWS STALL point)

// Sideslip thresholds (degrees absolute)
const float SLIP_WARN_DEG  = 5.0f;   // yellow
const float SLIP_ALARM_DEG = 15.0f;  // white-on-red

// G-force thresholds (shared with LNDG re-entry and DOCK)
const float G_WARN_POS  = KCM_HIGH_G_WARN_POS;    // #3D yellow — sustained high-G   (cross-panel aligned)
const float G_ALARM_POS = KCM_HIGH_G_ALARM_POS;   // #3D aligned with Annunciator CW_HIGH_G_ALARM
const float G_WARN_NEG  = KCM_HIGH_G_WARN_NEG;    // #3D yellow — negative G          (cross-panel aligned)
const float G_ALARM_NEG = KCM_HIGH_G_ALARM_NEG;   // #3D aligned with Annunciator CW_HIGH_G_WARN


/***************************************************************************************
   FLIGHT THRESHOLDS — LANDING (LNDG screen)
****************************************************************************************/

// T.Grnd / V.Vrt — gear UP (time-based, matches annunciator CW_GROUND_PROX)
const float LNDG_TGRND_ALARM_S  = KCM_GROUND_PROX_S;   // #3D aligned with Annunciator CW_GROUND_PROX_S
const float LNDG_TGRND_WARN_S   = 30.0f;   // yellow — T.Grnd below this with gear UP
const float LNDG_TGRND_HYST_S   = 3.0f;    // colour-band hysteresis: relax only after clearing a
                                           // threshold by this margin, so a jittery estimate sitting
                                           // near a boundary can't flip-flop green<->yellow each frame

// T.Grnd / V.Vrt — gear DOWN (speed-based, structural landing limits)
const float LNDG_VVRT_ALARM_MS  = -8.0f;   // red — crash landing speed (m/s, negative)
const float LNDG_VVRT_WARN_MS   = -5.0f;   // yellow — fast landing (m/s, negative)

// Alt.Rdr thresholds (m)
const float ALT_RDR_ALARM_M      = 50.0f;   // white-on-red — very low (LNDG + ACFT)
const float ALT_RDR_WARN_M       = 500.0f;  // yellow — low altitude (LNDG + ACFT)
const float LNDG_ALT_RDR_WARN_M  = 200.0f;  // yellow — powered-descent-specific tighter low-alt warning

// Horizontal velocity thresholds (Fwd/Lat) — tighten contextually with T.Grnd
// T.Grnd > 60s: loose  |  30-60s: mid  |  10-30s: tight  |  <10s: final
const float LNDG_HVEL_WARN_LOOSE_MS  = 20.0f;  const float LNDG_HVEL_ALARM_LOOSE_MS  = 999.0f;
const float LNDG_HVEL_WARN_MID_MS    =  5.0f;  const float LNDG_HVEL_ALARM_MID_MS    =  15.0f;
const float LNDG_HVEL_WARN_TIGHT_MS  =  2.0f;  const float LNDG_HVEL_ALARM_TIGHT_MS  =   8.0f;
const float LNDG_HVEL_WARN_FINAL_MS  =  1.0f;  const float LNDG_HVEL_ALARM_FINAL_MS  =   2.0f;

// Parachute deployment limits as dynamic pressure q = 0.5*airDensity*v^2 (Pa).
// KSP destroys a chute deployed above a structural q (force) limit, which is
// body-independent; expressing the limit as q makes the safe-deploy SPEED
// altitude-correct (higher up in thin air, lower near the ground). Cross-panel
// aligned with the Annunciator CW_CHUTE_ENV via KCMk1_SystemConfig.h.
const float LNDG_CHUTE_MAIN_MAX_Q   = KCM_CHUTE_MAIN_MAX_Q;    // main rips above this q (Pa)
const float LNDG_CHUTE_DROGUE_MAX_Q = KCM_CHUTE_DROGUE_MAX_Q;  // drogue rips above this q (Pa)

// Parachute deployment state thresholds
// LNDG_CHUTE_SEMI_DENSITY: air density (kg/m³) above which the chute begins to
//   semi-deploy. Matches KSP's default minAirPressureToOpen = 0.04 atm.
//   On Kerbin: 0.04 atm × 1.225 kg/m³ (sea-level density) ≈ 0.049 kg/m³.
//   Chute shows ARMED (cyan) below this; OPEN yellow/green above it.
const float LNDG_CHUTE_SEMI_DENSITY = 0.049f;

// Full-deploy radar altitudes — below these the chute transitions from OPEN yellow
// (semi-deploying) to OPEN green (fully open). Drogues deploy higher than mains.
const float LNDG_DROGUE_FULL_ALT = 2500.0f;   // drogue fully open below 2500m AGL
const float LNDG_MAIN_FULL_ALT   = 1000.0f;   // main fully open below 1000m AGL

// T.Grnd band boundaries for Fwd/Lat context-dependent thresholds (seconds)
const float LNDG_HVEL_T_LOOSE_S = 60.0f;   // above this: loose thresholds
const float LNDG_HVEL_T_MID_S   = 30.0f;   // above this: mid thresholds

// Re-entry SAS indicator: below this Mach number aerodynamic forces are strong
// enough that SAS OFF is acceptable (capsule stabilises ballistically).
// Above this threshold SAS OFF is alarmed white-on-red.
// Tune during flight testing — Mach 3.0 is a reasonable starting point.
const float REENTRY_SAS_AERO_STABLE_MACH = 3.0f;
const float DV_STG_ALARM_MS = KCM_LOW_DV_MS;   // #3D aligned with Annunciator CW_LOW_DV_MS
const float DV_STG_WARN_MS  = 300.0f;  // yellow

// Total ΔV threshold (m/s) — VEH screen
const float DV_TOT_WARN_MS  = 500.0f;  // yellow — mission nearly out of propellant


/***************************************************************************************
   FLIGHT THRESHOLDS — TARGET (TGT screen)
****************************************************************************************/

// Distance to target (m) — yellow <5km, white-on-green <200m (ready for DOCK)
const float RNDZ_DIST_WARN_M  = 5000.0f;   // yellow — closing

// Closure velocity thresholds (m/s, absolute value)
const float TGT_VCLOSURE_WARN_MS  = 200.0f;  // yellow — fast approach
const float TGT_VCLOSURE_ALARM_MS = 500.0f;  // white-on-red — very fast

// Approach alignment error thresholds (degrees absolute)
// THE RETICLE RINGS ARE THE COLOUR BANDS. Every reticle draws its good zone at
// full-scale/4 and its middle ring at full-scale/2, so the thresholds are set to those
// radii and the picture can never disagree with the numbers: green inside the inner
// ring, yellow out to the middle ring, red beyond. TGT runs +/-60 deg full scale.
// These really are wider than DOCK now — the old 5/15 was TIGHTER than DOCK's 10/20,
// the opposite of what this comment claimed and of what the two phases need.
const float TGT_BRG_WARN_DEG  = 15.0f;   // yellow — inner ring  (60/4)
const float TGT_BRG_ALARM_DEG = 30.0f;   // white-on-red — middle ring  (60/2)


/***************************************************************************************
   FLIGHT THRESHOLDS — DOCKING (DOCK screen)
****************************************************************************************/

// Distance to target (m)
const float DOCK_DIST_ALARM_M = 50.0f;    // white-on-red
const float DOCK_DIST_WARN_M  = 200.0f;   // yellow

// Closure rate — alarm at >2 m/s within 100m
const float DOCK_VCLOSURE_ALARM_MS   = 2.0f;
const float DOCK_VCLOSURE_ALARM_DIST_M = 100.0f;

// Drift speed (m/s magnitude)
const float DOCK_DRIFT_WARN_MS  = 0.1f;   // yellow
const float DOCK_DRIFT_ALARM_MS = 0.5f;   // white-on-red

// Bearing/elevation angle (degrees absolute) — the reticle rings, as above.
// DOCK runs +/-20 deg full scale, so these are tighter than TGT: the precision phase
// gets the tighter tolerance, which is what the old 10/20-vs-TGT-5/15 had backwards.
// Applied to BOTH V.Brg/V.Elv (approach path vs port) and Nos.Off (nose off bore) —
// two different quantities sharing one band, which suits both at docking range.
const float DOCK_BRG_WARN_DEG  =  5.0f;  // yellow — inner ring  (20/4)
const float DOCK_BRG_ALARM_DEG = 10.0f;  // red — middle ring  (20/2)


/***************************************************************************************
   FLIGHT THRESHOLDS — ORBIT (ORB screen)
****************************************************************************************/


/***************************************************************************************
   FLIGHT THRESHOLDS — APSIDES (APSI screen) & time thresholds
****************************************************************************************/

// Time to ignition (MNVR screen, seconds)
const float MNVR_TIGN_WARN_S  = 60.0f;   // yellow — get ready to light
const float MNVR_TIGN_ALARM_S = 10.0f;   // white-on-red — light NOW

// Total ΔV margin over maneuver ΔV — yellow if within this factor (e.g. 1.1 = within 10%)
const float MNVR_DV_MARGIN = 1.1f;

// CommNet signal (percent)
const float VEH_SIGNAL_WARN_PCT = 50.0f;   // yellow — weak link

// ── Thermal & electrical (annunciator-aligned) ───────────────────────────────
// Thermal limits (percent of part limit; core or skin temperature). Alarm aligned
// to the Annunciator CW_HIGH_TEMP (KCM_TEMP_ALARM_PCT); the warn tier is a
// display-only yellow pre-alarm (the C&W panel has no temperature warning tier).
const uint8_t TEMP_WARN_PCT  = 75;                  // yellow — getting hot
const uint8_t TEMP_ALARM_PCT = KCM_TEMP_ALARM_PCT;  // white-on-red — critical (= 90)

// Electric charge (fraction 0..1). Alarm aligned to Annunciator CW_BUS_VOLTAGE
// (KCM_EC_LOW_ALARM_FRAC = 5%); warn aligned to the resource-low convention
// (KCM_RES_LOW_WARN_FRAC = 20%, same tier as CW_PROP_LOW / CW_RCS_LOW).
const float EC_LOW_WARN_FRAC  = KCM_RES_LOW_WARN_FRAC;   // yellow — 20%
const float EC_LOW_ALARM_FRAC = KCM_EC_LOW_ALARM_FRAC;   // white-on-red — 5%

// Pre-launch EC readiness check — deliberately stricter than the in-flight low-EC
// alarm above (you want the battery topped off before launch, not merely above the
// 5% bus-voltage floor). Display-only; no C&W analog.
const uint8_t EC_PRELAUNCH_READY_PCT = 90;   // green — good to go
const uint8_t EC_PRELAUNCH_LOW_PCT   = 75;   // yellow — below this: white-on-red


/***************************************************************************************
   FLIGHT THRESHOLDS — ATT screen (heading/pitch error)
****************************************************************************************/
// MNVR runs +/-20 deg full scale, so these match the reticle rings exactly as above.
// WARN also gates the neon-green alignment box drawn around the maneuver marker.
const float ATT_ERR_WARN_DEG  =  5.0f;   // yellow — inner ring  (20/4)
const float ATT_ERR_ALARM_DEG = 10.0f;   // white-on-red — middle ring  (20/2)


/***************************************************************************************
   FLIGHT THRESHOLDS — LAUNCH (LNCH screen)
****************************************************************************************/

// Time to apoapsis caution during gravity turn (seconds)
const float LNCH_TOAPO_WARN_S  = 30.0f;   // yellow — apoapsis close during burn

// Stage burn time thresholds (seconds)
const float LNCH_BURNTIME_ALARM_S = KCM_LOW_BURN_S;   // #3D aligned with Annunciator CW_LOW_BURN_S
const float LNCH_BURNTIME_WARN_S  = 120.0f;  // yellow

// Fallback body radius when currentBody.radius is unavailable (Kerbin, metres).
const float DEFAULT_BODY_RADIUS_M = 600000.0f;

// Altitude (as a fraction of body radius) at which the ascent/circularization mode
// switch flips, with a small hysteresis band so it doesn't chatter near the boundary.
const float ORB_SWITCH_ALT_FRAC_ASC  = 0.06f;   // switch up to orbital view above this
const float ORB_SWITCH_ALT_FRAC_DESC = 0.055f;  // switch back down below this


/***************************************************************************************
   FLIGHT THRESHOLDS — ROVER (ROVR screen)
****************************************************************************************/

// Pitch (slope) thresholds (degrees). Tune per rover — heavier/wider rovers tolerate more.
const float ROVER_PITCH_WARN_DEG  = 20.0f;   // yellow — getting steep
const float ROVER_PITCH_ALARM_DEG = 30.0f;   // white-on-red — rollover risk

// Roll (lateral tilt) thresholds (degrees). Roll is typically the more critical axis.
const float ROVER_ROLL_WARN_DEG   = 15.0f;   // yellow — leaning significantly
const float ROVER_ROLL_ALARM_DEG  = 25.0f;   // white-on-red — rollover imminent

// Electric charge thresholds (%) — aligned to the shared low-EC thresholds so the
// rover matches every other screen and the Annunciator CW_BUS_VOLTAGE alarm.
const float ROVER_EC_WARN_PCT     = EC_LOW_WARN_FRAC  * 100.0f;   // yellow — 20%
const float ROVER_EC_ALARM_PCT    = EC_LOW_ALARM_FRAC * 100.0f;   // white-on-red — 5%

/***************************************************************************************
   PHASE 2 IMPLEMENTATION NOTES
   Items to complete when adding Simpit integration:

   VEH screen:
     state.commNetSignal comes from FLIGHT_STATUS_MESSAGE (channel 36).
     All other VEH fields (vesselName, vesselType, ctrlLevel, crewCount, crewCapacity,
       situation, isRecoverable) also come from FLIGHT_STATUS_MESSAGE and VESSEL_NAME_MESSAGE.

   APSI screen:
     Subscribe to APSIDESTIME_MESSAGE (channel 24).
     Struct: apsidesTimeMessage { int32_t periapsis; int32_t apoapsis; }

   MNVR screen:
     Subscribe to MANEUVER_MESSAGE (channel 34).
     Struct: maneuverMessage { float timeToNextManeuver; float deltaVNextManeuver;
       float durationNextManeuver; float deltaVTotal;
       float headingNextManeuver; float pitchNextManeuver; }

   RNDZ + DOCK screens:
     Subscribe to TARGETINFO_MESSAGE (channel 25).
     Use FLIGHT_HAS_TARGET flag from FLIGHT_STATUS_MESSAGE.

   RNDZ screen intercept data:
     Subscribe to INTERSECTS_MESSAGE (channel 33).

   ORB screen:
     Subscribe to ORBIT_MESSAGE (channel 36).

   ATT screen:
     Subscribe to ROTATION_DATA_MESSAGE (channel 45) and SAS_MODE_INFO_MESSAGE (channel 35).

   LNDG screen:
     state.gear_on   from ACTIONSTATUS_MESSAGE & GEAR_ACTION
     state.brakes_on from ACTIONSTATUS_MESSAGE & BRAKES_ACTION
     state.airDensity from ATMO_CONDITIONS_MESSAGE (channel 44).
     Drogue/Main deploy via CAGSTATUS_MESSAGE (channel 41).

   Parachute static bools (_drogueDeployed etc.) must be reset on vessel switch.
****************************************************************************************/
