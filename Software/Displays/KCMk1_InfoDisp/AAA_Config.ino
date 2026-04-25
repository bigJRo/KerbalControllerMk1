/***************************************************************************************
   AAA_Config.ino -- Tunable constants for Kerbal Controller Mk1 Information Display
   Sketch version is defined in KCMk1_InfoDisp.h
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


/***************************************************************************************
   OPERATING MODE
****************************************************************************************/
bool debugMode = false;
bool demoMode  = true;   // true = sine-wave demo values, no KSP required

// STANDALONE_TEST: true = no I2C master connected — skip the boot PROCEED handshake
// and enter loop() immediately. Safe to leave true for bench/UI testing; set false
// for production (master will send PROCEED after reading the status packet).
const bool STANDALONE_TEST = true;


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

// AoA thresholds (degrees absolute)
const float AOA_WARN_DEG  = 10.0f;   // yellow — approaching stall AoA
const float AOA_ALARM_DEG = 20.0f;   // white-on-red — beyond stall AoA

// Sideslip thresholds (degrees absolute)
const float SLIP_WARN_DEG  = 5.0f;   // yellow
const float SLIP_ALARM_DEG = 15.0f;  // white-on-red

// G-force thresholds (shared with LNDG re-entry and DOCK)
const float G_WARN_POS  =  4.0f;   // yellow — sustained high-G
const float G_ALARM_POS = KCM_HIGH_G_ALARM_POS;   // #3D aligned with Annunciator CW_HIGH_G_ALARM
const float G_WARN_NEG  = -2.0f;   // yellow — negative G
const float G_ALARM_NEG = KCM_HIGH_G_ALARM_NEG;   // #3D aligned with Annunciator CW_HIGH_G_WARN


/***************************************************************************************
   FLIGHT THRESHOLDS — LANDING (LNDG screen)
****************************************************************************************/

// T.Grnd / V.Vrt — gear UP (time-based, matches annunciator CW_GROUND_PROX)
const float LNDG_TGRND_ALARM_S  = KCM_GROUND_PROX_S;   // #3D aligned with Annunciator CW_GROUND_PROX_S
const float LNDG_TGRND_WARN_S   = 30.0f;   // yellow — T.Grnd below this with gear UP

// T.Grnd / V.Vrt — gear DOWN (speed-based, structural landing limits)
const float LNDG_VVRT_ALARM_MS  = -8.0f;   // red — crash landing speed (m/s, negative)
const float LNDG_VVRT_WARN_MS   = -5.0f;   // yellow — fast landing (m/s, negative)

// Alt.Rdr thresholds (m)
const float ALT_RDR_ALARM_M      = 50.0f;   // white-on-red — very low (LNDG + ACFT)
const float ALT_RDR_WARN_M       = 500.0f;  // yellow — low altitude (LNDG + ACFT)

// Horizontal velocity thresholds (Fwd/Lat) — tighten contextually with T.Grnd
// T.Grnd > 60s: loose  |  30-60s: mid  |  10-30s: tight  |  <10s: final
const float LNDG_HVEL_WARN_LOOSE_MS  = 20.0f;  const float LNDG_HVEL_ALARM_LOOSE_MS  = 999.0f;
const float LNDG_HVEL_WARN_MID_MS    =  5.0f;  const float LNDG_HVEL_ALARM_MID_MS    =  15.0f;
const float LNDG_HVEL_WARN_TIGHT_MS  =  2.0f;  const float LNDG_HVEL_ALARM_TIGHT_MS  =   8.0f;
const float LNDG_HVEL_WARN_FINAL_MS  =  1.0f;  const float LNDG_HVEL_ALARM_FINAL_MS  =   2.0f;

// Re-entry horizontal velocity thresholds (m/s) — V.Hrz on re-entry mode
const float LNDG_REENTRY_VHRZ_ALARM_MS = 50.0f;   // white-on-red
const float LNDG_REENTRY_VHRZ_WARN_MS  = 10.0f;   // yellow


// Parachute deployment speed limits (m/s surface velocity)
// KSP's safe/risky/unsafe indicator is speed-based, not dynamic-pressure-based.
// Stock values from testing and community data:
//   Drogue: safe below ~500 m/s, risky 500-600 m/s, unsafe above ~600 m/s
//   Main:   safe below ~250 m/s, risky 250-300 m/s, unsafe above ~300 m/s
// Tune LNDG_*_RISKY_MS if the display doesn't match your game's yellow threshold.
const float LNDG_DROGUE_SAFE_MS  = 750.0f;   // green below this
const float LNDG_DROGUE_RISKY_MS = 850.0f;   // yellow above safe, red above risky
const float LNDG_MAIN_SAFE_MS    = 475.0f;   // green below this
const float LNDG_MAIN_RISKY_MS   = 550.0f;   // yellow above safe, red above risky

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
// Wider than DOCK — long-range ops tolerate larger angles
const float TGT_BRG_WARN_DEG  =  5.0f;   // yellow — off-axis approach
const float TGT_BRG_ALARM_DEG = 15.0f;   // white-on-red — significantly misaligned


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

// Bearing/elevation angle (degrees absolute)
const float DOCK_BRG_WARN_DEG  = 10.0f;  // yellow — target off bore
const float DOCK_BRG_ALARM_DEG = 20.0f;  // red — large angle


/***************************************************************************************
   FLIGHT THRESHOLDS — ORBIT (ORB screen)
****************************************************************************************/

// Orbit screen: T+Ap/T+Pe near-circular suppression guard.
// If ApA and PeA are within ORB_CIRCULAR_PCT percent of each other,
// the orbit is effectively circular and the T+ row shows --- to avoid a
// rapidly-jumping meaningless value. 1.0 = 1%.
const float ORB_CIRCULAR_PCT = 1.0f;

// Eccentricity thresholds
const float ORB_ECC_WARN  = 0.9f;   // yellow — highly elliptical
const float ORB_ECC_ALARM = 1.0f;   // white-on-red — escape trajectory


/***************************************************************************************
   FLIGHT THRESHOLDS — APSIDES (APSI screen) & time thresholds
****************************************************************************************/

// Time to apoapsis/periapsis (seconds)
const float APSI_TIME_WARN_S = 60.0f;    // yellow — node approaching

// Time to ignition (MNVR screen, seconds)
const float MNVR_TIGN_WARN_S  = 60.0f;   // yellow — get ready to light
const float MNVR_TIGN_ALARM_S = 10.0f;   // white-on-red — light NOW

// Total ΔV margin over maneuver ΔV — yellow if within this factor (e.g. 1.1 = within 10%)
const float MNVR_DV_MARGIN = 1.1f;

// CommNet signal (percent)
const float VEH_SIGNAL_WARN_PCT = 50.0f;   // yellow — weak link

// Thermal limits (percent of part limit, skin temperature)
const uint8_t VEH_TEMP_SUPPRESS_PCT = 40;   // below this: bar suppressed (nominal)
const uint8_t VEH_TEMP_WARN_PCT     = 70;   // yellow — getting warm
const uint8_t VEH_TEMP_ALARM_PCT    = 85;   // white-on-red — critical


/***************************************************************************************
   FLIGHT THRESHOLDS — ATT screen (heading/pitch error)
****************************************************************************************/
const float ATT_ERR_WARN_DEG  =  5.0f;   // yellow
const float ATT_ERR_ALARM_DEG = 15.0f;   // white-on-red


/***************************************************************************************
   FLIGHT THRESHOLDS — LAUNCH (LNCH screen)
****************************************************************************************/

// Time to apoapsis caution during gravity turn (seconds)
const float LNCH_TOAPO_WARN_S  = 30.0f;   // yellow — apoapsis close during burn

// Stage burn time thresholds (seconds)
const float LNCH_BURNTIME_ALARM_S = KCM_LOW_BURN_S;   // #3D aligned with Annunciator CW_LOW_BURN_S
const float LNCH_BURNTIME_WARN_S  = 120.0f;  // yellow


/***************************************************************************************
   FLIGHT THRESHOLDS — ROVER (ROVR screen)
****************************************************************************************/

// Radar altitude thresholds (m) — inverted logic vs aircraft/lander.
// On a rover, being close to the ground is GOOD (wheels on surface).
// Green < GOOD, yellow < WARN, red >= WARN (significantly airborne = out of control).
const float ROVER_ALT_RDR_GOOD_M = 5.0f;    // green — wheels on/near ground
const float ROVER_ALT_RDR_WARN_M = 10.0f;   // yellow — lightly airborne

// Pitch (slope) thresholds (degrees). Tune per rover — heavier/wider rovers tolerate more.
const float ROVER_PITCH_WARN_DEG  = 20.0f;   // yellow — getting steep
const float ROVER_PITCH_ALARM_DEG = 30.0f;   // white-on-red — rollover risk

// Roll (lateral tilt) thresholds (degrees). Roll is typically the more critical axis.
const float ROVER_ROLL_WARN_DEG   = 15.0f;   // yellow — leaning significantly
const float ROVER_ROLL_ALARM_DEG  = 25.0f;   // white-on-red — rollover imminent

// Target bearing error thresholds (degrees).
const float ROVER_BRG_WARN_DEG    = 10.0f;   // yellow — off course
const float ROVER_BRG_ALARM_DEG   = 30.0f;   // white-on-red — significantly off course

// Electric charge thresholds (%).
const float ROVER_EC_WARN_PCT     = 50.0f;   // yellow — running low
const float ROVER_EC_ALARM_PCT    = 25.0f;   // white-on-red — critical

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
