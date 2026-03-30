/***************************************************************************************
   Demo.ino -- Demo mode for Kerbal Controller Mk1 Information Display
   Exercises all AppState fields and all conditional colour bands on every screen.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


static const uint32_t DEMO_UPDATE_MS = 100;
static uint32_t _demoLast  = 0;
static float    _demoPhase = 0.0f;


void initDemoMode() {
  state.vesselName      = "Jeb's Rocket";
  state.vesselType      = type_Ship;
  state.ctrlLevel       = 3;
  state.situation       = sit_Flying;
  state.isRecoverable   = false;
  state.gameSOI         = "Kerbin";
  state.crewCount       = 3;
  state.crewCapacity    = 4;
  state.commNetSignal   = 85;
  state.targetAvailable = true;
  state.rcs_on          = false;
  state.gear_on         = false;
  state.brakes_on       = false;
  state.drogueDeploy    = false;
  state.drogueCut       = false;
  state.mainDeploy      = false;
  state.mainCut         = false;
  state.heading         = 90.0f;
  state.pitch           = 15.0f;
  state.roll            = 0.0f;
  state.sasMode         = 0;     // STABILITY
  state.inAtmo          = false;
  state.airDensity      = 0.0f;
  currentBody           = getBodyParams(state.gameSOI);
}


void stepDemoState() {
  uint32_t now = millis();
  if (now - _demoLast < DEMO_UPDATE_MS) return;
  _demoLast = now;

  _demoPhase += 0.015f;

  // -----------------------------------------------------------------------
  // LNCH / APSI — altitude sweeps -75km..+75km, exercises:
  //   Alt: red (< 0), yellow (< 500m), green
  //   Vel switch: orbital above ~36km, surface below ~33km
  // -----------------------------------------------------------------------
  state.altitude    = 75000.0f * sinf(_demoPhase * 0.4f);

  // Orbital velocity — goes slightly negative to exercise red band
  state.orbitalVel  = 2000.0f * sinf(_demoPhase * 0.35f);   // -2000..+2000 m/s
  state.surfaceVel  = 1800.0f * sinf(_demoPhase * 0.38f);
  // verticalVel: large enough to drive ORB/SRF hysteresis switch, but also
  // sweeps through small negative values for realistic T_GRND calculation
  state.verticalVel = 15.0f * sinf(_demoPhase * 0.8f);      // -15..+15 m/s

  // Apoapsis — goes negative (suborbital) when altitude is low
  state.apoapsis    = 80000.0f * sinf(_demoPhase * 0.4f + 0.3f);

  // Time to Ap — goes negative (past apoapsis)
  state.timeToAp    = 600.0f * sinf(_demoPhase * 0.4f + 0.5f);

  // Periapsis — sweeps negative (below ground) to exercise red on APSI
  state.periapsis   = 60000.0f * sinf(_demoPhase * 0.42f + 1.0f);

  // Time to Pe — goes negative (past periapsis)
  state.timeToPe    = 2700.0f * sinf(_demoPhase * 0.38f + 1.5f);

  // Orbital elements
  state.orbitalPeriod = 3600.0f + 300.0f * sinf(_demoPhase + 0.8f);
  state.inclination   = 6.0f   + 4.0f   * sinf(_demoPhase + 1.2f);
  state.eccentricity  = 0.05f  + 0.04f  * sinf(_demoPhase + 0.3f);
  state.semiMajorAxis = 700000.0f + 80000.0f * sinf(_demoPhase * 0.1f);  // ~700km orbit
  state.LAN           = 180.0f * (0.5f  + 0.5f * sinf(_demoPhase * 0.1f));
  state.argOfPe       = 180.0f * (0.5f  + 0.5f * sinf(_demoPhase * 0.12f));
  state.trueAnomaly   = 180.0f * (0.5f  + 0.5f * sinf(_demoPhase * 0.08f));
  state.meanAnomaly   = 180.0f * (0.5f  + 0.5f * sinf(_demoPhase * 0.09f));

  // Attitude — heading sweeps 0-360, pitch -30..+30, roll -45..+45
  state.heading       = 180.0f + 179.0f * sinf(_demoPhase * 0.3f);
  state.pitch         = 30.0f  * sinf(_demoPhase * 0.5f);
  state.roll          = 45.0f  * sinf(_demoPhase * 0.4f);
  state.orbVelHeading = 180.0f + 170.0f * sinf(_demoPhase * 0.28f);
  state.orbVelPitch   = 10.0f  * sinf(_demoPhase * 0.45f);
  state.srfVelHeading = 180.0f + 175.0f * sinf(_demoPhase * 0.32f);
  state.srfVelPitch   = 8.0f   * sinf(_demoPhase * 0.42f);

  // SAS mode cycles 0-9 then briefly shows 255 (SAS OFF) — one state per 5s, full cycle ~55s
  {
    uint8_t sasTick = (uint8_t)((millis() / 5000) % 11);  // 0-10
    state.sasMode = (sasTick == 10) ? 255 : sasTick;       // 10 → 255 (OFF)
  }
  // Atmosphere toggles every 20s (simulates ascent through atmosphere)
  state.inAtmo  = ((millis() / 20000) % 2 == 0);

  // RCS toggles every 12 seconds
  state.rcs_on = ((millis() / 12000) % 2 == 0);

  // Gear and Brakes — toggle every ~10 seconds
  state.gear_on   = ((millis() / 10000) % 2 == 0);
  state.brakes_on = ((millis() / 15000) % 2 == 0);

  // Intercept data — sweeps through valid and invalid states
  state.intercept1Time = (int32_t)(300.0f * sinf(_demoPhase * 0.18f));  // -300..+300s
  state.intercept1Dist = 800.0f + 600.0f * sinf(_demoPhase * 0.22f);   // 200..1400m
  state.intercept2Time = (int32_t)(900.0f * sinf(_demoPhase * 0.12f));  // -900..+900s
  state.intercept2Dist = 1500.0f + 1200.0f * sinf(_demoPhase * 0.15f); // 300..2700m

  // Parachute demo: cycles through stowed->deployed->cut over 30s
  uint32_t chutePhase = (millis() / 10000) % 3;
  state.drogueDeploy = (chutePhase >= 1);
  state.drogueCut    = (chutePhase >= 2);
  state.mainDeploy   = (chutePhase >= 1) && ((millis() / 5000) % 2 == 0);
  state.mainCut      = false;  // main rarely cut in demo
  // Air density cycles 0..1.2 kg/m3 (Kerbin sea level = 1.2)
  state.airDensity   = 0.6f + 0.6f * sinf(_demoPhase * 0.15f);

  // -----------------------------------------------------------------------
  // LNCH — stage burn time: sweeps 0..150s through red(<60), yellow(<120), green
  // -----------------------------------------------------------------------
  state.stageBurnTime = 75.0f + 75.0f * sinf(_demoPhase * 0.3f);  // 0..150s

  // -----------------------------------------------------------------------
  // LNCH — stage ΔV: sweeps 0..450 through red(<150), yellow(<300), green
  // -----------------------------------------------------------------------
  state.stageDeltaV = 225.0f + 225.0f * sinf(_demoPhase * 0.25f);  // 0..450 m/s

  // MNVR — mnvrTime goes negative (past node), totalDeltaV crosses below mnvrDeltaV
  state.mnvrTime     = 400.0f  * sinf(_demoPhase * 0.7f);           // -400..+400s
  state.mnvrDeltaV   = 300.0f  + 250.0f * sinf(_demoPhase * 0.5f);  // 50..550 m/s
  state.mnvrDuration = 45.0f   + 40.0f  * sinf(_demoPhase * 0.6f);
  state.totalDeltaV  = 300.0f  + 250.0f * sinf(_demoPhase * 0.4f);  // different phase crosses mnvrDeltaV
  state.mnvrHeading  = 180.0f  + 175.0f * sinf(_demoPhase * 0.22f); // 0-360 burn heading
  state.mnvrPitch    = 20.0f   * sinf(_demoPhase * 0.38f);           // -20..+20 burn pitch

  // LNDG — radarAlt sweeps low, verticalVel goes negative to exercise T_GRND thresholds
  // T_GRND = radarAlt / |verticalVel|
  //   radarAlt ~10m,  vel -20 m/s → ~0.5s  (white-on-red,  < 10s)
  //   radarAlt ~100m, vel -5 m/s  → ~20s   (yellow,        < 30s)
  //   radarAlt ~490m, vel -5 m/s  → ~98s   (green)
  state.radarAlt   = 250.0f + 240.0f * sinf(_demoPhase * 0.4f);  // 10..490m

  // -----------------------------------------------------------------------
  // ACFT
  // -----------------------------------------------------------------------
  state.throttle      = 0.5f + 0.5f * sinf(_demoPhase * 0.5f);  // 0..1 (0-100%)
  state.machNumber = 1.5f + 1.2f * sinf(_demoPhase * 0.5f);
  state.IAS        = 180.0f + 150.0f * sinf(_demoPhase * 0.45f);
  state.gForce     = 7.0f   * sinf(_demoPhase * 0.6f);
  // Air density sweeps 0..1.5 kg/m3 to exercise parachute safety thresholds
  // At v=100 m/s: q = 0.5 * 1.2 * 10000 = 6000 Pa (unsafe for main)
  state.airDensity = (state.inAtmo) ? (0.6f + 0.6f * sinf(_demoPhase * 0.25f)) : 0.0f;

  // -----------------------------------------------------------------------
  // TGT — toggles targetAvailable periodically
  // -----------------------------------------------------------------------
  state.tgtDistance   = 5000.0f + 4000.0f * sinf(_demoPhase * 0.3f);
  state.tgtVelocity   = 25.0f   * sinf(_demoPhase * 0.35f);   // -25..+25 m/s (neg=closing)
  state.tgtHeading    = 90.0f   + 85.0f   * sinf(_demoPhase * 0.25f);
  state.tgtPitch      = 5.0f    * sinf(_demoPhase * 0.4f);
  state.tgtVelHeading = 180.0f  + 170.0f  * sinf(_demoPhase * 0.27f);
  state.tgtVelPitch   = 8.0f    * sinf(_demoPhase * 0.38f);

  // -----------------------------------------------------------------------
  // VEH — cycle all vessel types (all 17), ctrl levels (0-3),
  //        situations, and isRecoverable.
  // Use millis() directly for time-based cycling independent of phase speed.
  // -----------------------------------------------------------------------
  uint32_t tick2s = millis() / 2000;   // advances every 2 seconds

  // Ctrl level: 0-3, changes every 2s (~8s full cycle)
  state.ctrlLevel     = tick2s % 4;
  // CommNet signal cycles through strong, weak, and lost
  state.commNetSignal = (uint8_t)constrain((int)(70.0f + 70.0f * sinf(_demoPhase * 0.2f)), 0, 100);

  // All 17 vessel types, one per 2s (~34s full cycle)
  static const VesselType typeCycle[] = {
    type_Ship, type_Probe, type_Relay, type_Rover, type_Lander,
    type_Plane, type_Station, type_Base, type_EVA, type_Flag,
    type_Debris, type_Object, type_Unknown, type_SciCtrlr, type_SciPart,
    type_Part, type_GndPart
  };
  state.vesselType = typeCycle[(tick2s / 4) % 17];

  // Situations + isRecoverable, one per 2s (~18s full cycle)
  uint8_t sitTick = (tick2s / 2) % 9;
  state.isRecoverable = (sitTick == 8);
  static const VesselSituation sitCycle[] = {
    sit_PreLaunch, sit_Flying, sit_SubOrb, sit_Orbit,
    sit_Escaping, sit_Landed, sit_Splashed, sit_Docked, sit_Flying
  };
  state.situation = sitCycle[sitTick % 9];

  // targetAvailable: 30s with target, 3s NO TARGET
  // Use modulo 33000ms; NO TARGET only during the last 3000ms of each cycle
  state.targetAvailable = ((millis() % 33000) < 30000);
}
