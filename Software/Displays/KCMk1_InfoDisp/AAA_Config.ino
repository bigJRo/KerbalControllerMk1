/***************************************************************************************
   AAA_Config.ino -- Tunable constants for Kerbal Controller Mk1 Information Display
   Sketch version is defined in KCMk1_InfoDisp.h
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


/***************************************************************************************
   OPERATING MODE
****************************************************************************************/
bool debugMode = true;
bool demoMode  = true;   // Phase 1: always demo; set false when Simpit is integrated


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
const float G_ALARM_POS =  9.0f;   // white-on-red — structural/physiological limit
const float G_WARN_NEG  = -2.0f;   // yellow — negative G
const float G_ALARM_NEG = -5.0f;   // white-on-red — negative G limit


/***************************************************************************************
   FLIGHT THRESHOLDS — LANDING (LNDG screen)
****************************************************************************************/

// T.Grnd / V.Vrt — gear UP (time-based, matches annunciator CW_GROUND_PROX)
const float LNDG_TGRND_ALARM_S  = 10.0f;   // red — T.Grnd below this with gear UP
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

// Parachute deployment dynamic pressure limits (Pa)
// Drogue: safe below 1500 Pa, unsafe above 5000 Pa (KSP defaults)
// Main:   safe below 500 Pa,  unsafe above 3000 Pa
const float LNDG_DROGUE_SAFE_Q   = 1500.0f;
const float LNDG_DROGUE_UNSAFE_Q = 5000.0f;
const float LNDG_MAIN_SAFE_Q     =  500.0f;
const float LNDG_MAIN_UNSAFE_Q   = 3000.0f;

// T.Grnd band boundaries for Fwd/Lat context-dependent thresholds (seconds)
const float LNDG_HVEL_T_LOOSE_S = 60.0f;   // above this: loose thresholds
const float LNDG_HVEL_T_MID_S   = 30.0f;   // above this: mid thresholds

// Stage ΔV thresholds (m/s) — shared with LNCH and VEH
const float DV_STG_ALARM_MS = 150.0f;  // white-on-red — nearly empty stage
const float DV_STG_WARN_MS  = 300.0f;  // yellow

// Total ΔV threshold (m/s) — VEH screen
const float DV_TOT_WARN_MS  = 500.0f;  // yellow — mission nearly out of propellant


/***************************************************************************************
   FLIGHT THRESHOLDS — RENDEZVOUS (RNDZ screen)
****************************************************************************************/

// Distance to target (m)
const float RNDZ_DIST_ALARM_M = 500.0f;    // white-on-red
const float RNDZ_DIST_WARN_M  = 5000.0f;   // yellow

// Closure rate (m/s magnitude)
const float RNDZ_VCLOSURE_WARN_MS  =  5.0f;   // yellow — closing faster than this
const float RNDZ_VCLOSURE_ALARM_MS = 10.0f;   // white-on-red — fast closure within 2km
const float RNDZ_VCLOSURE_ALARM_DIST_M = 2000.0f;  // alarm only within this distance

// Intercept time (seconds)
const float RNDZ_INT_WARN_S = 120.0f;   // yellow — intercept approaching

// Intercept distance quality (m)
const float RNDZ_INTDIST_GOOD_M  = 1000.0f;  // green — good intercept
const float RNDZ_INTDIST_WARN_M  = 5000.0f;  // yellow — workable intercept
// red if > RNDZ_INTDIST_WARN_M (poor intercept, correction needed)


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

// Eccentricity
const float ORB_ECC_WARN  = 0.9f;   // yellow — highly elliptical
const float ORB_ECC_ALARM = 1.0f;   // white-on-red — escape trajectory


/***************************************************************************************
   FLIGHT THRESHOLDS — APSIDES (APSI screen) & time thresholds
****************************************************************************************/

// Time to apoapsis/periapsis (seconds)
const float APSI_TIME_WARN_S = 60.0f;    // yellow — node approaching

// Time to ignition (MNVR screen, seconds)
const float MNVR_TIGN_WARN_S = 60.0f;   // yellow — get ready to light

// Total ΔV margin over maneuver ΔV — yellow if within this factor (e.g. 1.1 = within 10%)
const float MNVR_DV_MARGIN = 1.1f;

// CommNet signal (percent)
const float VEH_SIGNAL_WARN_PCT = 50.0f;   // yellow — weak link


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
const float LNCH_BURNTIME_ALARM_S = 60.0f;   // white-on-red — nearly out of fuel
const float LNCH_BURNTIME_WARN_S  = 120.0f;  // yellow


/***************************************************************************************
   PHASE 2 IMPLEMENTATION NOTES
   (unchanged — see original for Simpit channel mapping details)
****************************************************************************************/

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
