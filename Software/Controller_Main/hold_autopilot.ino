/***************************************************************************************
   hold_autopilot.ino — Hold-mode autopilot: mode manager, cascades, autothrottle,
   rover cruise / steering / guards, disconnect rules, status, bench console.
   Contract and design notes in hold_autopilot.h; design document in
   Documents/Developer/Hold_Mode_Autopilot.md.
****************************************************************************************/
#include "hold_autopilot.h"
#include "attitude_controller.h"
#include "control_links.h"
#include "ascent_autopilot.h"

static const int32_t HP_AXIS_FULL = INT16_MAX;   // Simpit axis full-scale (wheel steer/throttle)

// KSP enum values carried by FLIGHT_STATUS_MESSAGE (Vessel.Situations / VesselType)
static const uint8_t KSP_SIT_LANDED   = 1;
static const uint8_t KSP_SIT_SPLASHED = 2;

/***************************************************************************************
   Telemetry snapshot
****************************************************************************************/
struct HpTelemetry {
  uint8_t  vesselType    = 0;
  uint8_t  situation     = 0;
  bool     hasTarget     = false;
  float    altSea        = 0.0f;
  float    velSurface    = 0.0f;
  float    velVertical   = 0.0f;
  float    ias           = 0.0f;
  float    mach          = 0.0f;
  float    heading       = 0.0f;
  float    pitch         = 0.0f;
  float    roll          = 0.0f;
  float    srfVelHeading = 0.0f;
  float    srfVelPitch   = 0.0f;
  float    tgtBearing    = 0.0f;
  float    throttle      = 0.0f;   // game throttle echo 0..1
  bool     hasAtmo       = true;
  bool     inAtmo        = true;
  bool     brakes        = false;
  uint32_t lastMs        = 0;
};
static HpTelemetry hp_t;
static HoldConfig  hp_c;

/***************************************************************************************
   Mode + loop state
****************************************************************************************/
static HpPitchMode hp_pitchMode = HP_PITCH_OFF;
static HpLatMode   hp_latMode   = HP_LAT_OFF;
static HpThrMode   hp_thrMode   = HP_THR_OFF;
static bool        hp_cruise = false, hp_rhdg = false, hp_rtgt = false;

// Setpoints (initialised by hpDefaultConfig/hpInit; captured on engage)
static float hp_spAtt = 0.0f, hp_spAoa = 3.0f, hp_spVs = 0.0f, hp_spAlt = 1000.0f;
static float hp_spRoll = 0.0f, hp_spHdg = 90.0f, hp_spIas = 120.0f, hp_spMach = 0.5f;
static float hp_spCruise = 5.0f, hp_spRhdg = 0.0f;
static float hp_maxSpeed = 20.0f, hp_maxSlope = 20.0f, hp_maxRoll = 25.0f;

static AttState hp_att;
static float    hp_holdHeading = 0.0f;   // rocket-entry heading reference when only ROLL is held
static float    hp_vsInt = 0.0f;          // V/S loop integrator (deg of pitch)
static float    hp_thrInt = 0.0f, hp_thrOut = 0.0f;
static float    hp_wheelInt = 0.0f, hp_wheelOut = 0.0f, hp_steerOut = 0.0f;
static bool     hp_slopeGuard = false;
static bool     hp_sasIsOff = false;     // we switched stock SAS off for attitude holding
static float    hp_cmdPitch = 0.0f, hp_cmdBank = 0.0f;

static uint8_t  hp_reason = HP_REASON_NONE,      hp_roverReason = HP_REASON_NONE;
static uint32_t hp_reasonMs = 0,                 hp_roverReasonMs = 0;
static uint32_t hp_lastUpdateMs = 0;
static uint32_t hp_airborneSince = 0, hp_noAtmoSince = 0;

static inline bool hpAircraftEngaged() { return hp_pitchMode != HP_PITCH_OFF || hp_latMode != HP_LAT_OFF || hp_thrMode != HP_THR_OFF; }
static inline bool hpOnGround()        { return hp_t.situation == KSP_SIT_LANDED || hp_t.situation == KSP_SIT_SPLASHED; }

static void hpSetReason(uint8_t r)      { if (r == HP_REASON_PILOT) r = HP_REASON_NONE; hp_reason = r;      hp_reasonMs = millis(); }
static void hpSetRoverReason(uint8_t r) { if (r == HP_REASON_PILOT) r = HP_REASON_NONE; hp_roverReason = r; hp_roverReasonMs = millis(); }

/***************************************************************************************
   Configuration
****************************************************************************************/
HoldConfig hpDefaultConfig() {
  HoldConfig c;
  c.acftGains   = attAircraftGains();
  c.rocketGains = attRocketGains();
  c.steerLikeRocket      = false;
  c.coordinateTurn       = false;
  c.maxControlDeflection = 1.0f;

  c.vsMax    = 30.0f;
  c.pitchMax = 25.0f;
  c.bankMax  = 30.0f;
  c.altKp    = 0.20f;
  c.vsKp     = 0.50f;  c.vsKi = 0.15f;
  c.hdgKp    = 1.50f;

  c.iasKp  = 0.010f;  c.iasKi  = 0.004f;
  c.machKp = 3.00f;   c.machKi = 1.00f;
  c.throttleSlew = 0.25f;

  c.cruiseKp = 0.08f; c.cruiseKi = 0.04f;
  c.wheelSlew = 0.5f;
  c.steerKpLow = 0.05f; c.steerKpHigh = 0.02f; c.steerKpSpeed = 20.0f;
  c.steerSign = 1.0f;

  c.telemetryTimeout = 2000;
  c.airborneMs = 500;
  c.noAtmoMs   = 2000;

  c.attMin = -45.0f;  c.attMax = 45.0f;
  c.aoaMin = -10.0f;  c.aoaMax = 25.0f;
  c.vsMin  = -100.0f; c.vsMaxSp = 100.0f;
  c.altMin = 0.0f;    c.altMax = 70000.0f;
  c.rollMin = -60.0f; c.rollMax = 60.0f;
  c.iasMin = 20.0f;   c.iasMax = 1500.0f;
  c.machMin = 0.10f;  c.machMax = 6.0f;
  c.cruiseMin = -10.0f; c.cruiseMax = 60.0f;
  c.maxSpeedMin = 1.0f;  c.maxSpeedMax = 60.0f;
  c.maxSlopeMin = 5.0f;  c.maxSlopeMax = 45.0f;
  c.maxRollMin  = 5.0f;  c.maxRollMax  = 60.0f;
  return c;
}

void hpInit() {
  hp_c = hpDefaultConfig();
  attReset(hp_att);
  hp_pitchMode = HP_PITCH_OFF; hp_latMode = HP_LAT_OFF; hp_thrMode = HP_THR_OFF;
  hp_cruise = hp_rhdg = hp_rtgt = false;
  hp_lastUpdateMs = millis();
}

HoldConfig &hpGetConfig() { return hp_c; }

/***************************************************************************************
   Outputs
****************************************************************************************/
static void hpSendWheels(bool setThrottle, float thr, bool setSteer, float steer) {
  wheelMessage w;
  if (setThrottle) w.setThrottle((int16_t)(attClampf(thr,   -1.0f, 1.0f) * (float)HP_AXIS_FULL));
  if (setSteer)    w.setSteer   ((int16_t)(attClampf(steer, -1.0f, 1.0f) * (float)HP_AXIS_FULL));
  mySimpit.send(WHEEL_MESSAGE, w);
}

// Stock SAS fights raw rotation: off while we hold an attitude axis, back to stability
// assist when we let go so the pilot never inherits a plane with SAS off.
static void hpReconcileSAS() {
  bool holding = (hp_pitchMode != HP_PITCH_OFF || hp_latMode != HP_LAT_OFF);
  if (holding && !hp_sasIsOff) {
    mySimpit.deactivateAction(SAS_ACTION);
    hp_sasIsOff = true;
  } else if (!holding && hp_sasIsOff) {
    mySimpit.activateAction(SAS_ACTION);
    mySimpit.setSASMode(AP_STABILITYASSIST);
    hp_sasIsOff = false;
  }
}

/***************************************************************************************
   Rover speed sign: VELOCITY_MESSAGE.surface is unsigned; more than 90 deg between the
   nose and the surface-velocity vector means reverse (the ROVER screen's derivation).
****************************************************************************************/
static float hpSignedSpeed() {
  if (hp_t.velSurface < 0.3f) return hp_t.velSurface;
  float d = fabsf(attWrap180(hp_t.srfVelHeading - hp_t.heading));
  return (d > 90.0f) ? -hp_t.velSurface : hp_t.velSurface;
}

/***************************************************************************************
   Engage / disconnect
****************************************************************************************/
static void hpDropAircraftOutputs() {
  rotClearAutoAxes();
  if (hp_thrMode != HP_THR_OFF) thrAutoRelease(THR_OWNER_HOLD);
}

void hpDisconnectAircraft(uint8_t reason) {
  bool was = hpAircraftEngaged();
  hp_pitchMode = HP_PITCH_OFF; hp_latMode = HP_LAT_OFF;
  if (hp_thrMode != HP_THR_OFF) { hp_thrMode = HP_THR_OFF; thrAutoRelease(THR_OWNER_HOLD); }
  rotClearAutoAxes();
  hpReconcileSAS();
  if (was) hpSetReason(reason);
}

void hpDisconnectRover(uint8_t reason) {
  bool was = hpRoverEngaged();
  bool hadCruise = hp_cruise;
  hp_cruise = hp_rhdg = hp_rtgt = false;
  hp_slopeGuard = false;
  hp_wheelOut = hp_wheelInt = 0.0f; hp_steerOut = 0.0f;
  if (hadCruise) hpSendWheels(true, 0.0f, false, 0.0f);   // never leave the motors driving
  if (was) hpSetRoverReason(reason);
}

void hpDisconnectAll(uint8_t reason) {
  hpDisconnectAircraft(reason);
  hpDisconnectRover(reason);
}

void hpVesselChanged() {
  // Silent: a vessel or scene change is not a fault, and the new vessel starts clean.
  hp_pitchMode = HP_PITCH_OFF; hp_latMode = HP_LAT_OFF;
  if (hp_thrMode != HP_THR_OFF) { hp_thrMode = HP_THR_OFF; thrAutoRelease(THR_OWNER_HOLD); }
  hp_cruise = hp_rhdg = hp_rtgt = false;
  hp_slopeGuard = false;
  rotClearAutoAxes();
  hp_sasIsOff = false;           // do not touch SAS on a vessel we have not flown
  hp_reason = hp_roverReason = HP_REASON_NONE;
  hp_t.lastMs = millis();
}

static bool hpIsAircraftMode(HpMode m) { return m <= HP_MODE_MACH; }

bool hpEngage(HpMode mode, bool on) {
  if (mode >= HP_MODE_COUNT) return false;
  uint32_t now = millis();

  if (!on) {
    switch (mode) {
      case HP_MODE_ATT:  if (hp_pitchMode == HP_PITCH_ATT) hp_pitchMode = HP_PITCH_OFF; break;
      case HP_MODE_AOA:  if (hp_pitchMode == HP_PITCH_AOA) hp_pitchMode = HP_PITCH_OFF; break;
      case HP_MODE_VS:   if (hp_pitchMode == HP_PITCH_VS)  hp_pitchMode = HP_PITCH_OFF; break;
      case HP_MODE_ALT:  if (hp_pitchMode == HP_PITCH_ALT) hp_pitchMode = HP_PITCH_OFF; break;
      case HP_MODE_ROLL: if (hp_latMode == HP_LAT_ROLL)    hp_latMode   = HP_LAT_OFF;   break;
      case HP_MODE_HDG:  if (hp_latMode == HP_LAT_HDG)     hp_latMode   = HP_LAT_OFF;   break;
      case HP_MODE_IAS:  if (hp_thrMode == HP_THR_IAS)  { hp_thrMode = HP_THR_OFF; thrAutoRelease(THR_OWNER_HOLD); } break;
      case HP_MODE_MACH: if (hp_thrMode == HP_THR_MACH) { hp_thrMode = HP_THR_OFF; thrAutoRelease(THR_OWNER_HOLD); } break;
      case HP_MODE_CRUISE: if (hp_cruise) { hp_cruise = false; hp_slopeGuard = false; hp_wheelOut = 0.0f; hpSendWheels(true, 0.0f, false, 0.0f); } break;
      case HP_MODE_RHDG: hp_rhdg = false; break;
      case HP_MODE_RTGT: hp_rtgt = false; break;
      default: break;
    }
    if (!hpAircraftEngaged()) rotClearAutoAxes();
    else if (hp_pitchMode == HP_PITCH_OFF && hp_latMode == HP_LAT_OFF) rotClearAutoAxes();
    hpReconcileSAS();
    return true;
  }

  // ---- Refusals ----
  bool stale = (now - hp_t.lastMs) > hp_c.telemetryTimeout;
  if (hpIsAircraftMode(mode)) {
    if (stale || !hp_t.hasAtmo) { hpSetReason(HP_REASON_REFUSED); return false; }
    if ((mode == HP_MODE_IAS || mode == HP_MODE_MACH) && thrPrecision()) { hpSetReason(HP_REASON_REFUSED); return false; }
  } else {
    if (stale || !hpOnGround()) { hpSetRoverReason(HP_REASON_REFUSED); return false; }
    if (mode == HP_MODE_RTGT && !hp_t.hasTarget) { hpSetRoverReason(HP_REASON_REFUSED); return false; }
  }
  // Engaging a hold mode takes the vehicle from the ascent autopilot (design §1.1).
  if (apIsArmed()) apDisarm();

  bool wasHoldingAttitude = (hp_pitchMode != HP_PITCH_OFF || hp_latMode != HP_LAT_OFF);

  switch (mode) {
    // ---- pitch group: capture, then bumpless integrator init ----
    case HP_MODE_ATT: hp_spAtt = attClampf(hp_t.pitch, hp_c.attMin, hp_c.attMax); hp_pitchMode = HP_PITCH_ATT; break;
    case HP_MODE_AOA: hp_spAoa = attClampf(hp_t.pitch - hp_t.srfVelPitch, hp_c.aoaMin, hp_c.aoaMax); hp_pitchMode = HP_PITCH_AOA; break;
    case HP_MODE_VS:  hp_spVs  = attClampf(roundf(hp_t.velVertical), hp_c.vsMin, hp_c.vsMaxSp); hp_vsInt = hp_t.pitch; hp_pitchMode = HP_PITCH_VS; break;
    case HP_MODE_ALT: hp_spAlt = attClampf(roundf(hp_t.altSea / 10.0f) * 10.0f, hp_c.altMin, hp_c.altMax); hp_vsInt = hp_t.pitch; hp_pitchMode = HP_PITCH_ALT; break;
    // ---- lateral group ----
    case HP_MODE_ROLL: hp_spRoll = attClampf(hp_t.roll, hp_c.rollMin, hp_c.rollMax); hp_holdHeading = hp_t.heading; hp_latMode = HP_LAT_ROLL; break;
    case HP_MODE_HDG:  hp_spHdg  = attWrap360(roundf(hp_t.heading)); hp_latMode = HP_LAT_HDG; break;
    // ---- thrust group ----
    case HP_MODE_IAS:
    case HP_MODE_MACH: {
      if (mode == HP_MODE_IAS) { hp_spIas  = attClampf(hp_t.ias,  hp_c.iasMin,  hp_c.iasMax);  hp_thrMode = HP_THR_IAS; }
      else                     { hp_spMach = attClampf(hp_t.mach, hp_c.machMin, hp_c.machMax); hp_thrMode = HP_THR_MACH; }
      float cur = thrLeverDriven() ? thrCurrentThrottle() : hp_t.throttle;
      hp_thrInt = hp_thrOut = attClampf(cur, 0.0f, 1.0f);
      thrAutoThrottle(THR_OWNER_HOLD, hp_thrOut);
      break;
    }
    // ---- rover ----
    case HP_MODE_CRUISE: {
      hp_spCruise = attClampf(roundf(hpSignedSpeed() * 2.0f) * 0.5f, hp_c.cruiseMin, hp_c.cruiseMax);
      hp_wheelInt = hp_wheelOut = 0.0f;
      hp_cruise = true;
      break;
    }
    case HP_MODE_RHDG: hp_spRhdg = attWrap360(roundf(hp_t.heading)); hp_rhdg = true; hp_rtgt = false; break;
    case HP_MODE_RTGT: hp_rtgt = true; hp_rhdg = false; break;
    default: return false;
  }

  bool holdingAttitude = (hp_pitchMode != HP_PITCH_OFF || hp_latMode != HP_LAT_OFF);
  if (holdingAttitude && !wasHoldingAttitude) attReset(hp_att);
  hp_lastUpdateMs = now;
  hpReconcileSAS();
  return true;
}

void hpLevel() {
  // Wings level + zero vertical speed. Goes through hpEngage for the refusals and SAS
  // handling, then overrides the captured setpoints.
  if (hpEngage(HP_MODE_ROLL, true)) hp_spRoll = 0.0f;
  if (hpEngage(HP_MODE_VS,   true)) hp_spVs   = 0.0f;
}

/***************************************************************************************
   Setpoints — range-checked, apply engaged or not
****************************************************************************************/
static bool hpInRange(float v, float lo, float hi) { return !(v < lo || v > hi); }
bool hpSetAtt(float v)      { if (!hpInRange(v, hp_c.attMin, hp_c.attMax)) return false; hp_spAtt = v; return true; }
bool hpSetAoa(float v)      { if (!hpInRange(v, hp_c.aoaMin, hp_c.aoaMax)) return false; hp_spAoa = v; return true; }
bool hpSetVs(float v)       { if (!hpInRange(v, hp_c.vsMin, hp_c.vsMaxSp)) return false; hp_spVs = v; return true; }
bool hpSetAlt(float v)      { if (!hpInRange(v, hp_c.altMin, hp_c.altMax)) return false; hp_spAlt = v; return true; }
bool hpSetRoll(float v)     { if (!hpInRange(v, hp_c.rollMin, hp_c.rollMax)) return false; hp_spRoll = v; return true; }
bool hpSetHdg(float v)      { hp_spHdg = attWrap360(v); return true; }
bool hpSetIas(float v)      { if (!hpInRange(v, hp_c.iasMin, hp_c.iasMax)) return false; hp_spIas = v; return true; }
bool hpSetMach(float v)     { if (!hpInRange(v, hp_c.machMin, hp_c.machMax)) return false; hp_spMach = v; return true; }
bool hpSetCruise(float v)   { if (!hpInRange(v, hp_c.cruiseMin, hp_c.cruiseMax)) return false; hp_spCruise = v; return true; }
bool hpSetRoverHdg(float v) { hp_spRhdg = attWrap360(v); return true; }
bool hpSetMaxSpeed(float v) { if (!hpInRange(v, hp_c.maxSpeedMin, hp_c.maxSpeedMax)) return false; hp_maxSpeed = v; return true; }
bool hpSetMaxSlope(float v) { if (!hpInRange(v, hp_c.maxSlopeMin, hp_c.maxSlopeMax)) return false; hp_maxSlope = v; return true; }
bool hpSetMaxRoll(float v)  { if (!hpInRange(v, hp_c.maxRollMin, hp_c.maxRollMax)) return false; hp_maxRoll = v; return true; }

bool hpAnyEngaged()      { return hpAircraftEngaged() || hpRoverEngaged(); }
bool hpAttitudeEngaged() { return hp_pitchMode != HP_PITCH_OFF || hp_latMode != HP_LAT_OFF; }
bool hpThrustEngaged()   { return hp_thrMode != HP_THR_OFF; }
bool hpRoverEngaged()    { return hp_cruise || hp_rhdg || hp_rtgt; }

/***************************************************************************************
   Loops
****************************************************************************************/
static float hpVsLoop(float vsCmd, float dt) {
  float err = vsCmd - hp_t.velVertical;
  hp_vsInt = attClampf(hp_vsInt + hp_c.vsKi * err * dt, -hp_c.pitchMax, hp_c.pitchMax);
  return attClampf(hp_c.vsKp * err + hp_vsInt, -hp_c.pitchMax, hp_c.pitchMax);
}

static void hpUpdateAircraft(uint32_t now, float dt) {
  // ---- Disconnect rules ----
  if (!hp_t.inAtmo) {
    if (hp_noAtmoSince == 0) hp_noAtmoSince = now;
    else if (now - hp_noAtmoSince >= hp_c.noAtmoMs) { hpDisconnectAircraft(HP_REASON_NO_ATMO); return; }
  } else hp_noAtmoSince = 0;

  // (Pilot stick / lever input is handled globally in hpUpdate: it drops everything.)
  hpReconcileSAS();
  if (!hpAircraftEngaged()) { rotClearAutoAxes(); return; }

  // ---- Pitch group -> commanded pitch ----
  AttMeasure m; m.pitch = hp_t.pitch; m.heading = hp_t.heading; m.roll = hp_t.roll;
  attUpdateRates(hp_att, m, dt);

  switch (hp_pitchMode) {
    case HP_PITCH_ATT: hp_cmdPitch = hp_spAtt; break;
    case HP_PITCH_AOA: hp_cmdPitch = attClampf(hp_t.srfVelPitch + hp_spAoa, -89.0f, 89.0f); break;
    case HP_PITCH_VS:  hp_cmdPitch = hpVsLoop(hp_spVs, dt); break;
    case HP_PITCH_ALT: {
      float vsCmd = attClampf(hp_c.altKp * (hp_spAlt - hp_t.altSea), -hp_c.vsMax, hp_c.vsMax);
      hp_cmdPitch = hpVsLoop(vsCmd, dt);
      break;
    }
    default: hp_cmdPitch = hp_t.pitch; break;
  }

  // ---- Lateral group -> commanded bank (or heading, rocket entry) ----
  switch (hp_latMode) {
    case HP_LAT_ROLL: hp_cmdBank = hp_spRoll; break;
    case HP_LAT_HDG:  hp_cmdBank = attClampf(hp_c.hdgKp * attWrap180(hp_spHdg - hp_t.heading), -hp_c.bankMax, hp_c.bankMax); break;
    default:          hp_cmdBank = hp_t.roll; break;
  }

  uint8_t mask = 0;
  if (hp_pitchMode != HP_PITCH_OFF) mask |= ROT_AXIS_PITCH;
  if (hp_latMode   != HP_LAT_OFF)   mask |= ROT_AXIS_ROLL;

  if (mask) {
    AttCommand c;
    if (hp_c.steerLikeRocket) {
      float cmdHeading = (hp_latMode == HP_LAT_HDG) ? hp_spHdg : hp_holdHeading;
      c = attSteerRocket(hp_att, hp_c.rocketGains, m, hp_cmdPitch, cmdHeading,
                         hp_latMode == HP_LAT_ROLL, hp_spRoll, hp_c.maxControlDeflection, dt);
      if (hp_latMode != HP_LAT_OFF) mask |= ROT_AXIS_YAW;
    } else {
      c = attSteerAircraft(hp_att, hp_c.acftGains, m, hp_cmdPitch, hp_cmdBank,
                           hp_c.coordinateTurn, hp_c.maxControlDeflection, dt);
      if (hp_c.coordinateTurn && hp_latMode != HP_LAT_OFF) mask |= ROT_AXIS_YAW;
    }
    rotSetAutoAxes(c.pitch, c.yaw, c.roll, mask);
  } else {
    rotClearAutoAxes();
  }

  // ---- Thrust group -> throttle ----
  if (hp_thrMode != HP_THR_OFF) {
    float err, kp, ki;
    if (hp_thrMode == HP_THR_IAS) { err = hp_spIas - hp_t.ias;   kp = hp_c.iasKp;  ki = hp_c.iasKi;  }
    else                         { err = hp_spMach - hp_t.mach; kp = hp_c.machKp; ki = hp_c.machKi; }
    hp_thrInt = attClampf(hp_thrInt + ki * err * dt, 0.0f, 1.0f);
    float target = attClampf(kp * err + hp_thrInt, 0.0f, 1.0f);
    float step = hp_c.throttleSlew * dt;
    if      (target > hp_thrOut + step) hp_thrOut += step;
    else if (target < hp_thrOut - step) hp_thrOut -= step;
    else                               hp_thrOut  = target;
    thrAutoThrottle(THR_OWNER_HOLD, hp_thrOut);
  }
}

static void hpUpdateRover(uint32_t now, float dt) {
  // ---- Guards and disconnects ----
  if (!hpOnGround()) {
    if (hp_airborneSince == 0) hp_airborneSince = now;
    else if (now - hp_airborneSince >= hp_c.airborneMs) { hpDisconnectRover(HP_REASON_AIRBORNE); return; }
  } else hp_airborneSince = 0;

  if (fabsf(hp_t.roll) > hp_maxRoll) {
    hpDisconnectRover(HP_REASON_ROLL_LIMIT);
    mySimpit.activateAction(BRAKES_ACTION);      // stays applied until the pilot releases it
    return;
  }
  if (hp_cruise && hp_t.brakes) {
    hp_cruise = false; hp_slopeGuard = false; hp_wheelOut = 0.0f;
    hpSendWheels(true, 0.0f, false, 0.0f);
    hpSetRoverReason(HP_REASON_BRAKES);
  }
  if (hp_rtgt && !hp_t.hasTarget) { hp_rtgt = false; hpSetRoverReason(HP_REASON_REFUSED); }
  if (!hpRoverEngaged()) return;

  float speed = hpSignedSpeed();
  bool reversing = speed < -0.3f;

  // ---- Cruise ----
  if (hp_cruise) {
    float sp = attClampf(hp_spCruise, -hp_maxSpeed, hp_maxSpeed);
    float slope = fabsf(hp_t.pitch);
    float half = hp_maxSlope * 0.5f;
    float scale = 1.0f;
    if (slope >= hp_maxSlope)  scale = 0.0f;
    else if (slope > half)    scale = 1.0f - (slope - half) / (hp_maxSlope - half);
    hp_slopeGuard = (scale < 1.0f);
    sp *= scale;

    float err = sp - speed;
    hp_wheelInt = attClampf(hp_wheelInt + hp_c.cruiseKi * err * dt, -1.0f, 1.0f);
    float target = attClampf(hp_c.cruiseKp * err + hp_wheelInt, -1.0f, 1.0f);
    float step = hp_c.wheelSlew * dt;
    if      (target > hp_wheelOut + step) hp_wheelOut += step;
    else if (target < hp_wheelOut - step) hp_wheelOut -= step;
    else                                 hp_wheelOut  = target;
  }

  // ---- Steering ----
  if (hp_rhdg || hp_rtgt) {
    float tgt = hp_rtgt ? hp_t.tgtBearing : hp_spRhdg;
    float err = attWrap180(tgt - hp_t.heading);
    if (reversing) err = -err;
    float f = attClampf(fabsf(speed) / hp_c.steerKpSpeed, 0.0f, 1.0f);
    float kp = hp_c.steerKpLow + (hp_c.steerKpHigh - hp_c.steerKpLow) * f;
    hp_steerOut = attClampf(hp_c.steerSign * kp * err, -1.0f, 1.0f);
  }

  hpSendWheels(hp_cruise, hp_wheelOut, hp_rhdg || hp_rtgt, hp_steerOut);
}

void hpUpdate() {
  uint32_t now = millis();
  float dt = (now - hp_lastUpdateMs) * 0.001f;
  hp_lastUpdateMs = now;
  if (dt <= 0.0f || dt > 0.5f) dt = 0.02f;

  if (!hpAnyEngaged()) return;

  // Failsafe: telemetry loss. Aircraft: hand the airframe to stability assist at the
  // current throttle (cutting throttle is the rocket failsafe, not the aircraft one).
  // Rover: wheel throttle to zero, no brakes.
  if ((now - hp_t.lastMs) > hp_c.telemetryTimeout) {
    hpDisconnectAll(HP_REASON_TELEMETRY);
    return;
  }

  // The pilot has the vehicle: any input on the rotation stick, the translation stick or
  // the throttle lever disconnects every hold mode (rover modes included).
  uint8_t ovr;
  if (pilotOverrideDetected(ovr)) {
    hpDisconnectAll(ovr);
    return;
  }

  if (hpAircraftEngaged()) hpUpdateAircraft(now, dt);
  if (hpRoverEngaged())    hpUpdateRover(now, dt);
}

/***************************************************************************************
   Status
****************************************************************************************/
static uint8_t hpAgeSeconds(uint32_t sinceMs, uint8_t reason) {
  if (reason == HP_REASON_NONE) return 255;
  uint32_t a = (millis() - sinceMs) / 1000UL;
  return a > 255 ? 255 : (uint8_t)a;
}

HoldStatus hpGetStatus() {
  HoldStatus s;
  s.pitchMode = hp_pitchMode; s.latMode = hp_latMode; s.thrMode = hp_thrMode;
  s.reason = hp_reason; s.reasonAge = hpAgeSeconds(hp_reasonMs, hp_reason);
  s.anyEngaged = hpAnyEngaged();
  s.thrustEngaged = hpThrustEngaged();
  s.leverTouched = thrTouched();
  s.leverDriven  = thrLeverDriven();
  s.ascentArmed  = apIsArmed();
  s.att = hp_spAtt; s.aoa = hp_spAoa; s.vs = hp_spVs; s.alt = hp_spAlt;
  s.roll = hp_spRoll; s.hdg = hp_spHdg; s.ias = hp_spIas; s.mach = hp_spMach;
  s.cmdThrottle = hp_thrOut;
  s.cruise = hp_cruise; s.rhdg = hp_rhdg; s.rtgt = hp_rtgt;
  s.brakes = hp_t.brakes; s.slopeGuard = hp_slopeGuard; s.targetAvailable = hp_t.hasTarget;
  s.roverReason = hp_roverReason; s.roverReasonAge = hpAgeSeconds(hp_roverReasonMs, hp_roverReason);
  s.cruiseSp = hp_spCruise; s.rhdgSp = hp_spRhdg;
  s.maxSpeed = hp_maxSpeed; s.maxSlope = hp_maxSlope; s.maxRoll = hp_maxRoll;
  s.cmdWheel = hp_wheelOut;
  return s;
}

const char *hpModeName(HpMode m) {
  switch (m) {
    case HP_MODE_ATT: return "ATT";   case HP_MODE_AOA: return "AOA";  case HP_MODE_VS:   return "VS";
    case HP_MODE_ALT: return "ALT";   case HP_MODE_ROLL: return "ROLL"; case HP_MODE_HDG:  return "HDG";
    case HP_MODE_IAS: return "IAS";   case HP_MODE_MACH: return "MACH";
    case HP_MODE_CRUISE: return "CRUISE"; case HP_MODE_RHDG: return "RHDG"; case HP_MODE_RTGT: return "RTGT";
    default: return "?";
  }
}

const char *hpReasonName(uint8_t r) {
  switch (r) {
    case HP_REASON_STICK: return "STICK";     case HP_REASON_LEVER: return "LEVER";
    case HP_REASON_BRAKES: return "BRAKES";   case HP_REASON_AIRBORNE: return "AIRBORNE";
    case HP_REASON_ROLL_LIMIT: return "ROLL LIMIT"; case HP_REASON_NO_ATMO: return "NO ATMO";
    case HP_REASON_TELEMETRY: return "TELEMETRY"; case HP_REASON_ASCENT: return "ASCENT";
    case HP_REASON_REFUSED: return "REFUSED"; default: return "";
  }
}

/***************************************************************************************
   Bench console — lines arrive from apSerialConsole() with the "HP " prefix stripped:
     ENG <MODE> [0|1]   engage (default) / disengage a mode by name
     SET <MODE> <v>     setpoint (MODE also accepts MAXSPD, MAXSLOPE, MAXROLL)
     LVL | OFF | STATUS
****************************************************************************************/
static int8_t hpModeFromName(const char *n) {
  for (uint8_t i = 0; i < HP_MODE_COUNT; i++)
    if (strncasecmp(n, hpModeName((HpMode)i), strlen(hpModeName((HpMode)i))) == 0) return (int8_t)i;
  return -1;
}

bool hpConsoleLine(const char *line) {
  if (strncasecmp(line, "ENG ", 4) == 0) {
    int8_t m = hpModeFromName(line + 4);
    if (m < 0) return false;
    const char *sp = strchr(line + 4, ' ');
    bool on = sp ? (atoi(sp + 1) != 0) : true;
    return hpEngage((HpMode)m, on);
  }
  if (strncasecmp(line, "SET ", 4) == 0) {
    const char *arg = line + 4;
    const char *sp = strchr(arg, ' ');
    if (!sp) return false;
    float v = atof(sp + 1);
    if      (strncasecmp(arg, "MAXSPD", 6) == 0)   return hpSetMaxSpeed(v);
    else if (strncasecmp(arg, "MAXSLOPE", 8) == 0) return hpSetMaxSlope(v);
    else if (strncasecmp(arg, "MAXROLL", 7) == 0)  return hpSetMaxRoll(v);
    int8_t m = hpModeFromName(arg);
    switch (m) {
      case HP_MODE_ATT: return hpSetAtt(v);   case HP_MODE_AOA: return hpSetAoa(v);
      case HP_MODE_VS:  return hpSetVs(v);    case HP_MODE_ALT: return hpSetAlt(v);
      case HP_MODE_ROLL: return hpSetRoll(v); case HP_MODE_HDG: return hpSetHdg(v);
      case HP_MODE_IAS: return hpSetIas(v);   case HP_MODE_MACH: return hpSetMach(v);
      case HP_MODE_CRUISE: return hpSetCruise(v); case HP_MODE_RHDG: return hpSetRoverHdg(v);
      default: return false;
    }
  }
  if (strncasecmp(line, "LVL", 3) == 0)  { hpLevel(); return true; }
  if (strncasecmp(line, "OFF", 3) == 0)  { hpDisconnectAll(HP_REASON_PILOT); return true; }
  if (strncasecmp(line, "STATUS", 6) == 0) {
    HoldStatus s = hpGetStatus();
    Serial.print(F("HP pitch=")); Serial.print(s.pitchMode);
    Serial.print(F(" lat="));     Serial.print(s.latMode);
    Serial.print(F(" thr="));     Serial.print(s.thrMode);
    Serial.print(F(" reason="));  Serial.print(hpReasonName(s.reason));
    Serial.print(F(" att="));     Serial.print(s.att, 1);
    Serial.print(F(" vs="));      Serial.print(s.vs, 1);
    Serial.print(F(" alt="));     Serial.print(s.alt, 0);
    Serial.print(F(" hdg="));     Serial.print(s.hdg, 0);
    Serial.print(F(" ias="));     Serial.print(s.ias, 0);
    Serial.print(F(" thrOut="));  Serial.print(s.cmdThrottle, 2);
    Serial.print(F(" | cruise=")); Serial.print(s.cruise);
    Serial.print(F(" sp="));      Serial.print(s.cruiseSp, 1);
    Serial.print(F(" wheel="));   Serial.print(s.cmdWheel, 2);
    Serial.print(F(" rreason=")); Serial.println(hpReasonName(s.roverReason));
    return true;
  }
  return false;
}

/***************************************************************************************
   Telemetry ingest
****************************************************************************************/
static inline void hpStamp() { hp_t.lastMs = millis(); }

void hpIngestFlightStatus(uint8_t vesselType, uint8_t situation, bool hasTarget) {
  hp_t.vesselType = vesselType; hp_t.situation = situation; hp_t.hasTarget = hasTarget; hpStamp();
}
void hpIngestAltitude(float sealevel)                   { hp_t.altSea = sealevel; hpStamp(); }
void hpIngestVelocity(float surface, float vertical)    { hp_t.velSurface = surface; hp_t.velVertical = vertical; hpStamp(); }
void hpIngestAirspeed(float ias, float mach)            { hp_t.ias = ias; hp_t.mach = mach; hpStamp(); }
void hpIngestAttitude(float heading, float pitch, float roll, float srfVelHeading, float srfVelPitch) {
  hp_t.heading = heading; hp_t.pitch = pitch; hp_t.roll = roll;
  hp_t.srfVelHeading = srfVelHeading; hp_t.srfVelPitch = srfVelPitch; hpStamp();
}
void hpIngestAtmo(bool hasAtmosphere, bool inAtmosphere) { hp_t.hasAtmo = hasAtmosphere; hp_t.inAtmo = inAtmosphere; hpStamp(); }
void hpIngestBrakes(bool on)                             { hp_t.brakes = on; }
void hpIngestTarget(float bearingDeg)                    { hp_t.tgtBearing = attWrap360(bearingDeg); }
void hpIngestThrottle(float t01)                         { hp_t.throttle = attClampf(t01, 0.0f, 1.0f); }
