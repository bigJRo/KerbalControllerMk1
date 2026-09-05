/***************************************************************************************
   Demo.ino -- Demo mode for Kerbal Controller Mk1 Information Display
   Exercises all AppState fields and all conditional colour bands on every screen.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


static const uint32_t DEMO_UPDATE_MS = 100;
static uint32_t _demoLast  = 0;
static float    _demoPhase = 0.0f;

/***************************************************************************************
   DEMO-SIDE ASCENT AUTOPILOT
   In demo mode there is no Controller_Main to accept the console's commands and echo
   the accepted values back, so the demo plays that part. Console taps arrive here via
   apDemoApplyCommand(); stepDemoState() publishes these fields into state.ap* instead
   of the literals it used to hardcode, which closes the round trip the console expects
   — command out, accepted value back. Without it, an ARM/DISARM tap or a parameter
   edit would raise a pending cue that nothing could ever clear, because the demo would
   overwrite the field with its own literal on the very next frame.

   The ascent animation is gated on the armed state rather than driving it: the demo
   boots armed so the guidance outputs animate as they always have, DISARM parks it in
   IDLE, and ARM starts it running again from where it left off.
****************************************************************************************/
static bool     _demoApArmed   = true;      // boots armed so the animation runs as before
static float    _demoApT       = 0.0f;      // ascent timer, advances only while armed
static float    _demoApTgtAlt  = 80000.0f;
static float    _demoApIncl    = 6.0f;
static bool     _demoApSouth   = false;
static float    _demoApLoft    = 1.0f;
static bool     _demoApRollEn  = false;
static float    _demoApRollDeg = 0.0f;
static float    _demoApMaxG    = 4.0f;

// Applied from apEnqueueCmd() when demoMode is set. Returns true when the opcode was
// recognised, which is what raises the console's pending cue — cleared a frame later
// when stepDemoState() publishes the new value and apReconcilePending() sees it match.
bool apDemoApplyCommand(uint8_t op, float payload) {
  switch (op) {
    case AP_CMD_SET_TARGET_ALT:  _demoApTgtAlt = payload; return true;
    case AP_CMD_SET_INCLINATION: _demoApIncl   = payload; return true;
    case AP_CMD_SET_LAUNCH_DIR:  _demoApSouth  = (payload != 0.0f); return true;
    case AP_CMD_SET_LOFT:        _demoApLoft   = payload; return true;
    case AP_CMD_SET_MAXG:        _demoApMaxG   = payload; return true;
    case AP_CMD_SET_ROLL:
      // AP_ROLL_OFF is the disable sentinel, outside the +/-180 range.
      _demoApRollEn  = (fabsf(payload) <= 180.0f);
      if (_demoApRollEn) _demoApRollDeg = payload;
      return true;
    case AP_CMD_ARM:             _demoApArmed = true;  return true;
    case AP_CMD_DISARM:          _demoApArmed = false; return true;
    default: return false;
  }
}


/***************************************************************************************
   DEMO-SIDE HOLD-MODE AUTOPILOT
   Same idea for the AIRCRAFT AP / ROVER AP consoles: the demo plays Controller_Main,
   applying engage / setpoint commands to a small model and publishing it into
   state.hp* / state.rv* so the consoles' pending cues close. Engaging captures the
   demo's live value, as the master does.
****************************************************************************************/
static uint8_t _demoHpPitch = 0, _demoHpLat = 0, _demoHpThr = 0;
static float   _demoHpAtt = 5.0f, _demoHpAoa = 3.0f, _demoHpVs = 0.0f, _demoHpAlt = 6000.0f;
static float   _demoHpRoll = 0.0f, _demoHpHdg = 90.0f, _demoHpIas = 180.0f, _demoHpMach = 0.85f;
static uint8_t _demoHpReason = 0; static uint32_t _demoHpReasonMs = 0;
static bool    _demoRvCruise = false, _demoRvHdg = false, _demoRvTgt = false;
static float   _demoRvCruiseSp = 12.0f, _demoRvHdgSp = 45.0f;
static float   _demoRvMaxSpd = 20.0f, _demoRvMaxSlope = 20.0f, _demoRvMaxRoll = 25.0f;
static uint8_t _demoRvReason = 0; static uint32_t _demoRvReasonMs = 0;
static float   _demoHpGs = 3.0f, _demoRvFollow = 30.0f, _demoRvStop = 15.0f;
static bool    _demoRvFollowOn = false;
// Orbital / landing models (Mission_Autopilot.md)
static uint8_t _demoObMode = 0, _demoObPhase = 0; static uint32_t _demoObPhaseMs = 0;
static bool    _demoObAppr = false, _demoObWarp = true, _demoObStage = true;
static float   _demoObAp = 250000.0f, _demoObPe = 80000.0f, _demoObInc = 0.0f, _demoObRate = -2.0f, _demoObDist = 50.0f;
static uint8_t _demoObReason = 0; static uint32_t _demoObReasonMs = 0;
static uint8_t _demoLdMode = 0; static bool _demoLdEntry = false, _demoLdRadial = false;
static float   _demoLdRate = -4.0f, _demoLdAlt = 50.0f, _demoLdTwr = 0.0f, _demoLdMargin = 150.0f, _demoLdAoa = 8.0f, _demoLdRoll = 0.0f;
static uint8_t _demoLdReason = 0; static uint32_t _demoLdReasonMs = 0;

bool hpDemoApplyCommand(uint8_t op, float payload) {
  bool on = (payload != 0.0f);
  switch (op) {
    // A/P OFF is everything, as Controller_Main's arbAllOff() is: the ascent autopilot too.
    // The demo boots armed, so without this the key stayed green after A/P OFF on any console.
    case HP_CMD_AP_OFF: _demoApArmed = false; _demoHpPitch = _demoHpLat = _demoHpThr = 0; _demoRvCruise = _demoRvHdg = _demoRvTgt = false;
                        _demoRvFollowOn = false; _demoObMode = 0; _demoObPhase = 0; _demoObAppr = false; _demoLdMode = 0; _demoLdEntry = false; return true;
    case HP_CMD_LVL:    _demoHpLat = 1; _demoHpRoll = 0.0f; _demoHpPitch = 3; _demoHpVs = 0.0f; return true;
    case HP_CMD_ENGAGE_ATT:  if (on) { _demoHpPitch = 1; _demoHpAtt = state.pitch; } else if (_demoHpPitch == 1) _demoHpPitch = 0; return true;
    case HP_CMD_ENGAGE_AOA:  if (on) { _demoHpPitch = 2; _demoHpAoa = state.pitch - state.srfVelPitch; } else if (_demoHpPitch == 2) _demoHpPitch = 0; return true;
    case HP_CMD_ENGAGE_VS:   if (on) { _demoHpPitch = 3; _demoHpVs = roundf(state.verticalVel); } else if (_demoHpPitch == 3) _demoHpPitch = 0; return true;
    case HP_CMD_ENGAGE_ALT:  if (on) { _demoHpPitch = 4; _demoHpAlt = roundf(state.altitude / 10.0f) * 10.0f; } else if (_demoHpPitch == 4) _demoHpPitch = 0; return true;
    case HP_CMD_ENGAGE_ROLL: if (on) { _demoHpLat = 1; _demoHpRoll = state.roll; } else if (_demoHpLat == 1) _demoHpLat = 0; return true;
    case HP_CMD_ENGAGE_HDG:  if (on) { _demoHpLat = 2; _demoHpHdg = roundf(state.heading); } else if (_demoHpLat == 2) _demoHpLat = 0; return true;
    case HP_CMD_ENGAGE_IAS:  if (on) { _demoHpThr = 1; _demoHpIas = state.IAS; } else if (_demoHpThr == 1) _demoHpThr = 0; return true;
    case HP_CMD_ENGAGE_MACH: if (on) { _demoHpThr = 2; _demoHpMach = state.machNumber; } else if (_demoHpThr == 2) _demoHpThr = 0; return true;
    case HP_CMD_SET_ATT:  _demoHpAtt = payload;  return true;
    case HP_CMD_SET_AOA:  _demoHpAoa = payload;  return true;
    case HP_CMD_SET_VS:   _demoHpVs = payload;   return true;
    case HP_CMD_SET_ALT:  _demoHpAlt = payload;  return true;
    case HP_CMD_SET_ROLL: _demoHpRoll = payload; return true;
    case HP_CMD_SET_HDG:  _demoHpHdg = payload;  return true;
    case HP_CMD_SET_IAS:  _demoHpIas = payload;  return true;
    case HP_CMD_SET_MACH: _demoHpMach = payload; return true;
    case HP_CMD_ENGAGE_CRUISE: _demoRvCruise = on; if (on) _demoRvCruiseSp = roundf(state.surfaceVel * 2.0f) * 0.5f; return true;
    case HP_CMD_ENGAGE_RHDG:   _demoRvHdg = on; if (on) { _demoRvTgt = false; _demoRvHdgSp = roundf(state.heading); } return true;
    case HP_CMD_ENGAGE_RTGT:
      if (on && !state.targetAvailable) { _demoRvReason = HP_REASON_REFUSED; _demoRvReasonMs = millis(); return true; }
      _demoRvTgt = on; if (on) _demoRvHdg = false; return true;
    case HP_CMD_SET_CRUISE:   _demoRvCruiseSp = payload; return true;
    case HP_CMD_SET_RHDG:     _demoRvHdgSp = payload;    return true;
    case HP_CMD_SET_MAXSPD:   _demoRvMaxSpd = payload;   return true;
    case HP_CMD_SET_MAXSLOPE: _demoRvMaxSlope = payload; return true;
    case HP_CMD_SET_MAXROLL:  _demoRvMaxRoll = payload;  return true;
    // ---- mission autopilot ----
    case HP_CMD_ENGAGE_NAV:  if (on && !state.targetAvailable) { _demoHpReason = HP_REASON_NO_TARGET; _demoHpReasonMs = millis(); return true; }
                             if (on) _demoHpLat = 3; else if (_demoHpLat == 3) _demoHpLat = 0; return true;
    case HP_CMD_ENGAGE_GS:   if (on && !state.targetAvailable) { _demoHpReason = HP_REASON_NO_TARGET; _demoHpReasonMs = millis(); return true; }
                             if (on) _demoHpPitch = 5; else if (_demoHpPitch == 5) _demoHpPitch = 0; return true;
    case HP_CMD_SET_GS:      _demoHpGs = payload; return true;
    case HP_CMD_ENGAGE_FOLLOW: _demoRvFollowOn = on; if (on) { _demoRvCruise = false; _demoRvHdg = _demoRvTgt = false; } return true;
    case HP_CMD_SET_FOLLOW_RANGE: _demoRvFollow = payload; return true;
    case HP_CMD_SET_STOP_DIST:    _demoRvStop = payload; return true;
    case OB_CMD_ARM_NODE: case OB_CMD_ARM_AP: case OB_CMD_ARM_PE: case OB_CMD_ARM_INC: {
      uint8_t m = (uint8_t)(op - OB_CMD_ARM_NODE + 1);
      if (on) { if (m == 1 && state.mnvrDeltaV <= 0.01f) { _demoObReason = HP_REASON_NO_NODE; _demoObReasonMs = millis(); return true; }
                _demoObMode = m; _demoObPhase = 1; _demoObAppr = false; }
      else if (_demoObMode == m) { _demoObMode = 0; _demoObPhase = 0; }
      return true;
    }
    case OB_CMD_ENGAGE_APPR: _demoObAppr = on; if (on) { _demoObMode = 0; _demoObPhase = 0; } return true;
    case OB_CMD_SET_AP: _demoObAp = payload; return true;
    case OB_CMD_SET_PE: _demoObPe = payload; return true;
    case OB_CMD_SET_INC: _demoObInc = payload; return true;
    case OB_CMD_SET_APPR_RATE: _demoObRate = payload; return true;
    case OB_CMD_SET_APPR_DIST: _demoObDist = payload; return true;
    case OB_CMD_SET_WARP: _demoObWarp = on; return true;
    case OB_CMD_SET_AUTOSTAGE: _demoObStage = on; return true;
    case OB_CMD_EXEC:
      if (_demoObPhase == 1) { _demoObPhase = 2; _demoObPhaseMs = millis(); }
      else if (_demoObPhase == 3 && _demoObWarp) { _demoObPhase = 4; _demoObPhaseMs = millis(); }
      return true;
    case LD_CMD_ENGAGE_DESC: case LD_CMD_ENGAGE_HOVR: case LD_CMD_ENGAGE_BRAKE: {
      uint8_t m = (uint8_t)(op - LD_CMD_ENGAGE_DESC + 1);
      if (on) _demoLdMode = m; else if (_demoLdMode == m) _demoLdMode = 0;
      return true;
    }
    case LD_CMD_ENGAGE_ENTRY: _demoLdEntry = on; return true;
    case LD_CMD_SET_DESC_RATE: _demoLdRate = payload; return true;
    case LD_CMD_SET_HOVR_ALT: _demoLdAlt = payload; return true;
    case LD_CMD_SET_TWR: _demoLdTwr = payload; return true;
    case LD_CMD_SET_MARGIN: _demoLdMargin = payload; return true;
    case LD_CMD_SET_ENTRY_AOA: _demoLdAoa = payload; return true;
    case LD_CMD_SET_ENTRY_ROLL: _demoLdRoll = payload; return true;
    case LD_CMD_SET_ATT_REF: _demoLdRadial = on; return true;
    default: return false;
  }
}


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
  state.wheelThrottle = sinf(_demoPhase * 0.2f);   // -1..1: drives ROVER's FWD / REV blocks
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
  state.gear_on     = ((millis() / 10000) % 2 == 0);
  state.brakes_on   = ((millis() / 15000) % 2 == 0);
  state.airbrake_on = ((millis() / 12000) % 2 == 0);
  state.trimEnabled = ((millis() / 8000)  % 2 == 0);

  // (AppState.intercept1/2 Dist/Time are unread future-KSP2 stubs — not driven here.)

  // Parachute demo: walks stowed -> deployed -> cut over 30 s. The RE-ENTRY screen's
  // chute states latch the way KSP's do (a cut chute stays cut until repacked, which
  // only a vessel switch resets), so the demo shows the sequence once, not on a loop.
  uint32_t chutePhase = (millis() / 10000) % 3;
  state.drogueDeploy = (chutePhase >= 1);
  state.drogueCut    = (chutePhase >= 2);
  state.mainDeploy   = (chutePhase >= 1) && ((millis() / 5000) % 2 == 0);
  state.mainCut      = false;  // main rarely cut in demo

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
  state.mnvrTotalDeltaV = state.mnvrDeltaV + 200.0f + 150.0f * sinf(_demoPhase * 0.3f);  // plan total >= next node
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
  // Air density sweeps 0..1.2 kg/m3 in atmosphere (0 outside) to exercise the
  // parachute safety thresholds: at v=100 m/s the peak is q = 0.5 * 1.2 * 10000 =
  // 6000 Pa, above the main-chute limit.
  state.airDensity = (state.inAtmo) ? (0.6f + 0.6f * sinf(_demoPhase * 0.25f)) : 0.0f;

  // -----------------------------------------------------------------------
  // ASCENT AUTOPILOT — cycles the phase through a launch so the panel animates
  // -----------------------------------------------------------------------
  {
    // The ascent runs only while armed; DISARM parks it in IDLE and holds the timer, so
    // ARM resumes where it left off. Phase never returns to IDLE while armed — IDLE is
    // what disarmed looks like — so the bands below start at VERTICAL.
    if (_demoApArmed) _demoApT = fmodf(_demoApT + 0.015f, 8.0f);
    float t = _demoApT;
    uint8_t ph = !_demoApArmed ? 0
               : (t < 0.8f) ? 1 : (t < 3.4f) ? 2 : (t < 4.9f) ? 3 : (t < 6.4f) ? 4 : 5;
    state.apPhase       = ph;
    state.apArmed       = _demoApArmed;
    state.apTargetAlt   = _demoApTgtAlt;
    state.apInclination = _demoApIncl;
    state.apSoutherly   = _demoApSouth;
    state.apLoft        = _demoApLoft;
    state.apRollEnable  = _demoApRollEn;
    state.apRollDeg     = _demoApRollDeg;
    state.apMaxG        = _demoApMaxG;
    float prog = (t - 1.0f) / 3.0f;                 // 0..1 across the gravity turn
    if (prog < 0.0f) prog = 0.0f;
    if (prog > 1.0f) prog = 1.0f;
    state.apCmdPitch    = 90.0f - 65.0f * prog;
    state.apCmdHeading  = 90.0f;
    state.apCmdThrottle = (ph == 3) ? 0.0f : (ph == 4) ? 0.6f : 1.0f;
    state.apDynPressure = 18000.0f * sinf(t * 0.5f);
    if (state.apDynPressure < 0.0f) state.apDynPressure = 0.0f;
  }

  // -----------------------------------------------------------------------
  // HOLD-MODE AUTOPILOT — publishes the demo's aircraft / rover models
  // -----------------------------------------------------------------------
  {
    bool any = _demoHpPitch || _demoHpLat || _demoHpThr;
    bool leverDriven = ((millis() / 30000) % 2 == 0);     // exercises the LEVER OFF note
    state.hpPitchMode = _demoHpPitch; state.hpLatMode = _demoHpLat; state.hpThrMode = _demoHpThr;
    state.hpFlags = (any ? 0x01 : 0) | (_demoHpThr ? 0x02 : 0) | (leverDriven ? 0x08 : 0) | (_demoApArmed ? 0x10 : 0);
    state.hpReason = _demoHpReason;
    state.hpReasonAge = _demoHpReason ? (uint8_t)min((millis() - _demoHpReasonMs) / 1000UL, 255UL) : 255;
    state.hpAtt = _demoHpAtt; state.hpAoa = _demoHpAoa; state.hpVs = _demoHpVs; state.hpAlt = _demoHpAlt;
    state.hpRoll = _demoHpRoll; state.hpHdg = _demoHpHdg; state.hpIas = _demoHpIas; state.hpMach = _demoHpMach;
    state.hpGs = _demoHpGs;
    state.hpCmdThrottle = state.throttle;

    bool slopeGuard = _demoRvCruise && fabsf(state.pitch) > _demoRvMaxSlope * 0.5f;
    state.rvFlags = (_demoRvCruise ? 0x01 : 0) | (_demoRvHdg ? 0x02 : 0) | (_demoRvTgt ? 0x04 : 0) |
                    (state.brakes_on ? 0x08 : 0) | (slopeGuard ? 0x10 : 0) | (state.targetAvailable ? 0x20 : 0) |
                    (_demoRvFollowOn ? 0x40 : 0);
    state.rvFollowRange = _demoRvFollow; state.rvStopDist = _demoRvStop;
    state.rvReason = _demoRvReason;
    state.rvReasonAge = _demoRvReason ? (uint8_t)min((millis() - _demoRvReasonMs) / 1000UL, 255UL) : 255;
    state.rvCruise = _demoRvCruiseSp; state.rvHdg = _demoRvHdgSp;
    state.rvMaxSpeed = _demoRvMaxSpd; state.rvMaxSlope = _demoRvMaxSlope; state.rvMaxRoll = _demoRvMaxRoll;
    state.rvCmdWheel = (_demoRvCruise || _demoRvFollowOn) ? 0.4f * sinf(_demoPhase * 0.3f) : 0.0f;
  }

  // -----------------------------------------------------------------------
  // ORBITAL / LANDING AUTOPILOT — walk a burn through its phases; publish both models
  // -----------------------------------------------------------------------
  {
    uint32_t now = millis();
    // Burn phases advance on a timer: ALIGN 4 s -> WARP READY (waits for EXEC if WARP on,
    // else 4 s) -> WARP 4 s -> BURN 8 s -> DONE 5 s -> IDLE.
    switch (_demoObPhase) {
      case 2: if (now - _demoObPhaseMs > 4000) { _demoObPhase = 3; _demoObPhaseMs = now; } break;
      case 3: if (!_demoObWarp && now - _demoObPhaseMs > 4000) { _demoObPhase = 5; _demoObPhaseMs = now; } break;
      case 4: if (now - _demoObPhaseMs > 4000) { _demoObPhase = 5; _demoObPhaseMs = now; } break;
      case 5: if (now - _demoObPhaseMs > 8000) { _demoObPhase = 6; _demoObPhaseMs = now; } break;
      case 6: if (now - _demoObPhaseMs > 5000) { _demoObPhase = 0; _demoObMode = 0; } break;
      default: break;
    }
    bool armed = _demoObMode != 0 && _demoObPhase >= 1 && _demoObPhase <= 5;
    bool exec  = _demoObPhase >= 2 && _demoObPhase <= 5;
    state.obFlags = (armed ? 0x01 : 0) | (exec ? 0x02 : 0) | (_demoObWarp ? 0x04 : 0) | (_demoObStage ? 0x08 : 0) |
                    (state.targetAvailable ? 0x10 : 0) | (state.mnvrDeltaV > 0.01f ? 0x20 : 0) | (_demoObAppr ? 0x40 : 0);
    state.obMode = _demoObMode; state.obPhase = _demoObPhase;
    state.obReason = _demoObReason;
    state.obReasonAge = _demoObReason ? (uint8_t)min((now - _demoObReasonMs) / 1000UL, 255UL) : 255;
    state.obTargetAp = _demoObAp; state.obTargetPe = _demoObPe; state.obTargetInc = _demoObInc;
    state.obApprRate = _demoObRate; state.obApprDist = _demoObDist;
    float dv = (_demoObMode == 1) ? state.mnvrDeltaV : 842.0f;
    state.obDvTotal = armed ? dv : 0.0f;
    float burnFrac = (_demoObPhase == 5) ? (now - _demoObPhaseMs) / 8000.0f : 0.0f;
    state.obDvRemaining = armed ? dv * (1.0f - burnFrac) : 0.0f;
    state.obTIgnition = armed ? ((_demoObPhase >= 5) ? 0.0f : 252.0f - (now - _demoObPhaseMs) * 0.001f) : 0.0f;
    state.obBurnDuration = armed ? 98.0f : 0.0f;
    state.obAccelEst = 8.6f;
    state.obCmdThrottle = (_demoObPhase == 5) ? 1.0f : 0.0f;

    bool ldEngaged = _demoLdMode != 0 || _demoLdEntry;
    float ignAlt = 2140.0f + 800.0f * sinf(_demoPhase * 0.2f);
    bool brakeFiring = (_demoLdMode == 3) && state.radarAlt < ignAlt;
    state.ldFlags = (ldEngaged ? 0x01 : 0) | ((_demoLdMode == 3 && !brakeFiring) ? 0x02 : 0) | (brakeFiring ? 0x04 : 0) |
                    (_demoLdRadial ? 0x08 : 0) | (_demoObStage ? 0x10 : 0) | ((millis() / 40000) % 2 ? 0x40 : 0);
    state.ldMode = _demoLdMode; state.ldEntry = _demoLdEntry ? 1 : 0;
    state.ldReason = _demoLdReason;
    state.ldReasonAge = _demoLdReason ? (uint8_t)min((now - _demoLdReasonMs) / 1000UL, 255UL) : 255;
    state.ldAccelSource = (_demoLdTwr > 0.0f) ? 2 : ((millis() / 20000) % 2);
    state.ldDescRate = _demoLdRate; state.ldHovrAlt = _demoLdAlt; state.ldTwr = _demoLdTwr; state.ldMargin = _demoLdMargin;
    state.ldEntryAoa = _demoLdAoa; state.ldEntryRoll = _demoLdRoll;
    state.ldIgnAlt = ignAlt; state.ldAccelEst = 14.2f;
    state.ldCmdThrottle = brakeFiring ? 1.0f : (_demoLdMode ? 0.5f + 0.3f * sinf(_demoPhase) : 0.0f);
  }

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
  // Electric charge % — sweeps through the pre-launch board's red/yellow/green bands
  state.electricChargePercent = constrain(70.0f + 45.0f * sinf(_demoPhase * 0.22f), 0.0f, 100.0f);
  state.coreTempPct = (uint8_t)constrain(55.0f + 45.0f * sinf(_demoPhase * 0.18f),        0.0f, 100.0f);
  state.skinTempPct = (uint8_t)constrain(60.0f + 40.0f * sinf(_demoPhase * 0.18f + 0.6f), 0.0f, 100.0f);

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
      vOrb = 175.0f + vSrf * 0.95f; // ≈ vSrf + small offset
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
