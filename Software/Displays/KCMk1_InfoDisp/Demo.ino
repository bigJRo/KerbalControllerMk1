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
  if (!demoMode) return;   // guard: no-op when called outside demo mode
  uint32_t now = millis();
  if (now - _demoLast < DEMO_UPDATE_MS) return;
  _demoLast = now;

  _demoPhase += 0.015f;

  // -----------------------------------------------------------------------
  // LNCH / APSI — altitude sweeps -75km..+75km, exercises:
  //   Alt: red (< 0), yellow (< 500m), green
  //   Vel switch: orbital above ~36km, surface below ~33km
  // -----------------------------------------------------------------------
  // Altitude: sweep independently so non-ORB screens (LNCH etc) exercise red/yellow bands.
  // When the ORB screen is active, override with the orbitally-consistent value below.
  state.altitude = 75000.0f * sinf(_demoPhase * 0.4f);

  // Orbital velocity — goes slightly negative to exercise red band
  state.orbitalVel  = 2000.0f * sinf(_demoPhase * 0.35f);   // -2000..+2000 m/s
  state.surfaceVel  = 1800.0f * sinf(_demoPhase * 0.38f);
  // verticalVel: large enough to drive ORB/SRF hysteresis switch, but also
  // sweeps through small negative values for realistic T_GRND calculation
  state.verticalVel = 15.0f * sinf(_demoPhase * 0.8f);      // -15..+15 m/s

  // Apoapsis/periapsis derived from orbital elements below — not set independently
  state.timeToAp    = 600.0f  * sinf(_demoPhase * 0.4f + 0.5f);
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

  // Apoapsis and periapsis derived from sma + ecc for orbital consistency
  state.apoapsis  = state.semiMajorAxis * (1.0f + state.eccentricity) - currentBody.radius;
  state.periapsis = state.semiMajorAxis * (1.0f - state.eccentricity) - currentBody.radius;

  // When viewing the ORB screen, override altitude with the orbitally-consistent value
  // so the position marker label matches the displayed orbit geometry.
  if (activeScreen == screen_ORB) {
    float ta_rad = state.trueAnomaly * DEG_TO_RAD;
    float e = state.eccentricity, a = state.semiMajorAxis;
    float r = a * (1.0f - e*e) / (1.0f + e * cosf(ta_rad));
    state.altitude = fmaxf(0.0f, r - currentBody.radius);
  }


  // Attitude — heading sweeps 0-360, pitch -30..+30, roll -45..+45
  state.heading       = 180.0f + 179.0f * sinf(_demoPhase * 0.3f);

  state.pitch         = 30.0f  * sinf(_demoPhase * 0.5f);
  state.roll          = 45.0f  * sinf(_demoPhase * 0.4f);
  
  /*
  ATT_staticTest_update();
  */

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

  // ─────────────────────────────────────────────────────────────────────────
  // LNCH ascent profile override — runs LAST so it supersedes the general
  // sinusoidal sweeps for fields that would otherwise conflict. When viewing
  // the LNCH screen (ascent mode), the altitude ladder, V.Vrt bar, and right-
  // panel numbers all move together in a way that mirrors a real Kerbin launch.
  // 180-second cycle then loops.
  //
  // Profile: pad → gravity turn → main ascent → MECO at ~90 km ApA →
  //          coast to apoapsis → circularization burn.
  // ─────────────────────────────────────────────────────────────────────────
  if (activeScreen == screen_LNCH) {
    float t = (float)(millis() % 180000UL) / 1000.0f;  // 0..180 s cycle

    float alt, vSrf, vVrt, apa, tToAp, thr, stgBrn, stgDV;

    // Altitude — piecewise curve through key waypoints
    if (t < 10.0f) {
      alt = 50.0f * t * t;                           // 0..5 km (rapid initial climb)
    } else if (t < 40.0f) {
      float p = (t - 10.0f) / 30.0f;
      alt = 500.0f + 9500.0f * powf(p, 1.3f);        // 500 m..10 km
    } else if (t < 100.0f) {
      alt = 10000.0f + 40000.0f * (t - 40.0f) / 60.0f;   // 10..50 km
    } else if (t < 130.0f) {
      alt = 50000.0f + 35000.0f * (t - 100.0f) / 30.0f;  // 50..85 km
    } else if (t < 160.0f) {
      alt = 85000.0f + 5000.0f * (t - 130.0f) / 30.0f;   // 85..90 km (coasting)
    } else {
      alt = 90000.0f + 1000.0f * sinf((t - 160.0f) / 20.0f * PI);  // near apoapsis
    }

    // V.Vrt — rises fast, peaks ~30 s, decays through coast, small again during circ
    if (t < 5.0f) {
      vVrt = 20.0f * t;                              // 0..100
    } else if (t < 40.0f) {
      vVrt = 100.0f + 250.0f * sinf((t - 5.0f) / 35.0f * (PI / 2.0f));
    } else if (t < 120.0f) {
      vVrt = 350.0f * cosf((t - 40.0f) / 80.0f * (PI / 2.0f));   // 350..0 decay
    } else if (t < 160.0f) {
      vVrt = 50.0f - 50.0f * (t - 120.0f) / 40.0f;   // 50..0
    } else {
      vVrt = 5.0f * sinf((t - 160.0f) / 10.0f * PI); // small oscillation at Ap
    }

    // V.Srf — total surface speed, builds throughout ascent.
    // Note: V.Srf includes horizontal + vertical components, so initially
    // (vertical phase) V.Srf ≈ V.Vrt. As the vessel pitches over for the
    // gravity turn, horizontal component grows and V.Srf grows faster.
    if (t < 0.5f) {
      // Truly stationary for first half second — FPA dial shows default "up"
      vSrf = 0.0f;
    } else if (t < 5.0f) {
      // Pure vertical phase: V.Srf ≈ V.Vrt
      vSrf = fmaxf(vVrt, 5.0f);
    } else if (t < 40.0f) {
      // Pitch-over phase: V.Srf grows faster than V.Vrt (horizontal builds)
      float p = (t - 5.0f) / 35.0f;
      vSrf = fmaxf(vVrt, 100.0f + 827.0f * powf(p, 1.2f));   // reaches ~927 at t=40
    } else if (t < 100.0f) {
      vSrf = 927.0f + 1123.0f * (t - 40.0f) / 60.0f;     // 927..2050
    } else if (t < 160.0f) {
      vSrf = 2050.0f + 200.0f * (t - 100.0f) / 60.0f;    // 2050..2250
    } else {
      vSrf = 2250.0f + 150.0f * (t - 160.0f) / 20.0f;    // circ adds ~150
    }

    // Apoapsis — rises to 90 km target by t=120, then small adjustments
    if (t < 5.0f) {
      apa = 100.0f * t;
    } else if (t < 120.0f) {
      apa = 500.0f + 89500.0f * (t - 5.0f) / 115.0f;
    } else if (t < 160.0f) {
      apa = 90000.0f;
    } else {
      apa = 90000.0f + 5000.0f * (t - 160.0f) / 20.0f;
    }

    // Throttle — burning during ascent, MECO 115-160s, circ burn after
    if (t < 115.0f)      thr = 1.0f;
    else if (t < 160.0f) thr = 0.0f;
    else                 thr = 1.0f;

    // Time to apoapsis — decreases as we approach Ap during coast
    if (t < 115.0f)      tToAp = 180.0f - 50.0f * (t / 115.0f);
    else if (t < 160.0f) tToAp = 130.0f * (1.0f - (t - 115.0f) / 45.0f);
    else                 tToAp = 1800.0f;   // post-circ orbital period is long

    // Stage burn time & ΔV — deplete during burns, reset on staging
    if (t < 115.0f) {
      stgBrn = 180.0f - 1.5f * t;
      stgDV  = 3500.0f - 30.0f * t;
    } else if (t < 160.0f) {
      stgBrn = 200.0f;                     // staged, engine off
      stgDV  = 1500.0f;
    } else {
      stgBrn = 200.0f - 3.0f * (t - 160.0f);
      stgDV  = 1500.0f - 7.0f * (t - 160.0f);
    }

    state.altitude      = alt;
    state.verticalVel   = vVrt;
    state.surfaceVel    = vSrf;
    state.apoapsis      = apa;
    state.timeToAp      = tToAp;
    state.throttle      = thr;
    state.stageBurnTime = stgBrn;
    state.stageDeltaV   = stgDV;

    // Orbital velocity for the V.Orb bar. On a surface-locked vessel on Kerbin's
    // equator, orbital velocity starts at ~175 m/s (rotation contribution) and
    // grows with V.Srf as the vessel accelerates east. Circular-orbit velocity
    // at 90 km on Kerbin is ~2270 m/s, so we target ~2270 by t~170.
    float vOrb;
    if (t < 1.0f) {
      vOrb = 175.0f;                                    // pad rotation
    } else if (t < 115.0f) {
      // Grows from ~175 toward ~2170 during ascent burn, tracking V.Srf
      vOrb = 175.0f + (vSrf + 175.0f - 175.0f) * 0.95f; // ≈ vSrf + small offset
      // Clamp to realistic progression
      if (vOrb < 175.0f) vOrb = 175.0f;
    } else if (t < 160.0f) {
      // Coast — orbital velocity decays slightly as altitude rises (energy traded)
      vOrb = 2170.0f - 50.0f * (t - 115.0f) / 45.0f;    // ~2170 → 2120
    } else {
      // Circ burn — push to ~2270 (target)
      vOrb = 2120.0f + 150.0f * (t - 160.0f) / 20.0f;   // 2120 → 2270
    }
    state.orbitalVel = vOrb;

    // Air density for the atmosphere gauge. Kerbin atmosphere follows a rough
    // exponential decay with altitude, sea-level density = 1.225 kg/m³, scale
    // height ≈ 5.6 km, atmosphere top = 70 km. Simple approximation:
    //   ρ(h) = ρ₀ × exp(-h / H_scale)   for h < atmosphere_top
    //   ρ = 0                           for h >= atmosphere_top
    {
      const float rho0 = 1.225f;           // Kerbin sea-level density
      const float scaleH = 5600.0f;        // Kerbin scale height (approximate)
      const float atmTop = 70000.0f;       // Kerbin atmosphere top
      if (alt >= atmTop) {
        state.airDensity = 0.0f;
        state.inAtmo = false;
      } else {
        state.airDensity = rho0 * expf(-alt / scaleH);
        state.inAtmo = true;
      }
    }

    // Orbital elements for the orbital-phase left-panel graphic. These only
    // matter once we cross into orbital mode (alt > ~36 km on Kerbin with the
    // auto-switch hysteresis), but we set them continuously for simplicity.
    // Pe stays just below the atmosphere (~65 km) so the orbit visibly needs
    // circularization — Ap growing during burn, Pe rising toward Ap.
    {
      // Pe: starts at 0 (pad) and rises as we ascend. Clamped at 65 km.
      float peaDemo = (alt < 10000.0f) ? 0.0f : fminf(65000.0f, alt - 10000.0f);
      state.periapsis = peaDemo;

      // argOfPe and trueAnomaly: slowly rotate for visual interest in the demo.
      state.argOfPe    = fmodf(t * 2.0f, 360.0f);        // 1 full rotation per 180s
      state.trueAnomaly = fmodf(t * 4.5f, 360.0f);       // vessel orbits faster

      // Eccentricity + sma for completeness. Derived from apa/pea.
      float rAp = apa + currentBody.radius;
      float rPe = peaDemo + currentBody.radius;
      float sma = (rAp + rPe) * 0.5f;
      float ecc = (rAp > rPe + 1.0f) ? ((rAp - rPe) / (rAp + rPe)) : 0.0f;
      state.semiMajorAxis = sma;
      state.eccentricity  = ecc;
    }

    // Ensure situation isn't PreLaunch/Landed during ascent so V.Vrt isn't
    // suppressed by the "dead-band" logic in the draw functions.
    if (t > 1.0f) {
      state.situation = sit_Flying;
    } else {
      state.situation = sit_PreLaunch;
    }
  }
}
