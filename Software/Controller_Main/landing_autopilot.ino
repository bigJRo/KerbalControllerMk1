/***************************************************************************************
   landing_autopilot.ino — DESC / HOVR / BRAKE / ENTRY.
   Contract in landing_autopilot.h; design in Mission_Autopilot.md §5, §7.2, §7.7.
****************************************************************************************/
#include "landing_autopilot.h"
#include "attitude_controller.h"
#include "control_links.h"
#include "hold_autopilot.h"

static const int32_t LP_AXIS_FULL = INT16_MAX;

struct LpTelemetry {
  uint8_t vesselType = 0, situation = 0;
  float altSea = 0, radarAlt = 0, velSurface = 0, velVertical = 0, mach = 0;
  float heading = 0, pitch = 0, roll = 0, orbVelHeading = 0, orbVelPitch = 0;
  float airDensity = 0; bool inAtmo = false;
  float gravity = 9.81f, flyHigh = 18000.0f; char bodyName[24] = {0};
  float throttle = 0;
  uint32_t lastMs = 0;
};
static LpTelemetry lp_t;
static LandingConfig lp_c;

static LpMode  lp_mode = LP_MODE_OFF;
static bool    lp_entry = false;
static bool    lp_brakeFiring = false;
static bool    lp_attRefRadial = false, lp_attRefRadialAuto = false;
static float   lp_descRate = -3.0f, lp_hovrAlt = 50.0f, lp_twr = 0.0f, lp_margin = 150.0f, lp_entryAoa = 8.0f, lp_entryRoll = 0.0f;
static float   lp_thrInt = 0.0f, lp_thrOut = 0.0f, lp_ignAlt = 0.0f;
static bool    lp_brakeMarginal = false;
static uint8_t lp_sasMode = 255;
static uint8_t lp_reason = HP_REASON_NONE; static uint32_t lp_reasonMs = 0;
static uint32_t lp_lastUpdateMs = 0;
static AttState lp_att;

static void lpSetReason(uint8_t r) { if (r == HP_REASON_PILOT) r = HP_REASON_NONE; lp_reason = r; lp_reasonMs = millis(); }
static inline bool lpOnGround() { return lp_t.situation == KSP_SIT_LANDED || lp_t.situation == KSP_SIT_SPLASHED; }
static inline float lpHorizontalSpeed() { float h2 = lp_t.velSurface * lp_t.velSurface - lp_t.velVertical * lp_t.velVertical; return h2 > 0.0f ? sqrtf(h2) : 0.0f; }

LandingConfig lpDefaultConfig() {
  LandingConfig c;
  c.entryGains = attRocketGains();
  c.descKp = 0.05f; c.descKi = 0.02f; c.descSlew = 0.5f;
  c.hovrKp = 0.3f; c.hovrVsCap = 10.0f;
  c.radialSwitchSpeed = 1.0f;
  c.brakeFactorEst = 0.25f; c.brakeFactorMeas = 0.10f; c.brakeFactorTwr = 0.05f;
  c.brakeLatencyS = 2.0f; c.brakeMinAccelG = 1.2f; c.brakeMarginalFrac = 0.9f;
  c.entryHandoffSpeed = 250.0f; c.entryPlaneMach = 2.5f; c.entryPlaneQ = 5000.0f;
  c.maxControlDeflection = 1.0f;
  c.telemetryTimeout = 2000;
  c.descMin = -50.0f; c.descMax = 20.0f; c.hovrMin = 2.0f; c.hovrMax = 5000.0f; c.twrMin = 0.0f; c.twrMax = 20.0f;
  c.marginMin = 0.0f; c.marginMax = 5000.0f; c.aoaMin = -30.0f; c.aoaMax = 40.0f; c.rollMin = -180.0f; c.rollMax = 180.0f;
  return c;
}

void lpInit() { lp_c = lpDefaultConfig(); lp_mode = LP_MODE_OFF; lp_entry = false; attReset(lp_att); lp_lastUpdateMs = millis(); }
LandingConfig &lpGetConfig() { return lp_c; }

/***************************************************************************************
   Outputs
****************************************************************************************/
static void lpThrottle(float t) {
  lp_thrOut = attClampf(t, 0.0f, 1.0f);
  thrAutoThrottle(THR_OWNER_LANDING, lp_thrOut);
  aeNoteThrottle(lp_thrOut);
}

static void lpSas(uint8_t mode) {
  if (lp_sasMode == mode) return;
  mySimpit.activateAction(SAS_ACTION);
  mySimpit.setSASMode(mode);
  lp_sasMode = mode;
}

static void lpReleaseThrottleOwner() { thrAutoRelease(THR_OWNER_LANDING); arbReleaseThrottle(AP_OWNER_LANDING); }
static void lpReleaseAttitudeOwner(bool sasStability) {
  rotClearAutoAxes();
  if (sasStability) { mySimpit.activateAction(SAS_ACTION); mySimpit.setSASMode(AP_STABILITYASSIST); }
  lp_sasMode = 255;
  arbReleaseAttitude(AP_OWNER_LANDING);
}

/***************************************************************************************
   Ignition altitude (Mission_Autopilot.md §7.2, review decision q.4)
****************************************************************************************/
static float lpAccel() { return (lp_twr > 0.0f) ? lp_twr * lp_t.gravity : aeAccel(); }
static uint8_t lpAccelSource() { return (lp_twr > 0.0f) ? AE_SRC_TWR : aeSource(); }

static float lpIgnitionAltitude() {
  float a = lpAccel(), g = lp_t.gravity;
  if (a <= g) return 0.0f;
  float k = lp_c.brakeFactorEst;
  switch (lpAccelSource()) { case AE_SRC_MEAS: k = lp_c.brakeFactorMeas; break; case AE_SRC_TWR: k = lp_c.brakeFactorTwr; break; default: break; }
  float v = lp_t.velSurface;
  float h = v * v / (2.0f * (a - g));
  return h * (1.0f + k) + fabsf(lp_t.velVertical) * lp_c.brakeLatencyS + lp_margin;
}

/***************************************************************************************
   Engage / disconnect
****************************************************************************************/
static void lpDropThrottleModes(uint8_t reason, bool keepThrottle) {
  if (lp_mode == LP_MODE_OFF) return;
  lp_mode = LP_MODE_OFF; lp_brakeFiring = false;
  if (!keepThrottle) lpThrottle(0.0f);
  lpReleaseThrottleOwner();
  if (!lp_entry) lpReleaseAttitudeOwner(true);
  lpSetReason(reason);
}

static void lpDropEntry(uint8_t reason) {
  if (!lp_entry) return;
  lp_entry = false;
  if (lp_mode == LP_MODE_OFF) lpReleaseAttitudeOwner(true);
  lpSetReason(reason);
}

void lpDisconnectAll(uint8_t reason) {
  bool keep = (reason == HP_REASON_TELEMETRY || reason == HP_REASON_STICK || reason == HP_REASON_LEVER || reason == HP_REASON_OTHER_AP);
  lpDropThrottleModes(reason, keep);
  lpDropEntry(reason);
}
void lpArbiterDropAttitude() { lpDropEntry(HP_REASON_OTHER_AP); lpDropThrottleModes(HP_REASON_OTHER_AP, true); }
void lpArbiterDropThrottle() { lpDropThrottleModes(HP_REASON_OTHER_AP, true); }

bool lpEngage(uint8_t mode, bool on) {
  if (!on) { if (lp_mode == mode) lpDropThrottleModes(HP_REASON_PILOT, false); return true; }
  if (mode < LP_MODE_DESC || mode > LP_MODE_BRAKE) return false;
  if ((millis() - lp_t.lastMs) > lp_c.telemetryTimeout) { lpSetReason(HP_REASON_REFUSED); return false; }
  if (lpOnGround()) { lpSetReason(HP_REASON_REFUSED); return false; }
  if (mode == LP_MODE_BRAKE && lpAccel() < lp_c.brakeMinAccelG * lp_t.gravity) { lpSetReason(HP_REASON_REFUSED); return false; }

  bool wasOff = (lp_mode == LP_MODE_OFF);
  arbTakeAttitude(AP_OWNER_LANDING);
  arbTakeThrottle(AP_OWNER_LANDING);
  if (wasOff) {
    float cur = thrLeverDriven() ? thrCurrentThrottle() : lp_t.throttle;
    lp_thrInt = lp_thrOut = attClampf(cur, 0.0f, 1.0f);
  }
  switch (mode) {
    case LP_MODE_HOVR:  lp_hovrAlt = attClampf(roundf(lp_t.radarAlt), lp_c.hovrMin, lp_c.hovrMax); break;
    case LP_MODE_BRAKE: lp_brakeFiring = false; break;
    default: break;
  }
  lp_mode = (LpMode)mode;
  lp_sasMode = 255;                     // force the SAS mode to be re-asserted
  return true;
}

bool lpEngageEntry(bool on) {
  if (!on) { lpDropEntry(HP_REASON_PILOT); return true; }
  if ((millis() - lp_t.lastMs) > lp_c.telemetryTimeout || lpOnGround()) { lpSetReason(HP_REASON_REFUSED); return false; }
  arbTakeAttitude(AP_OWNER_LANDING);
  mySimpit.deactivateAction(SAS_ACTION);   // raw rotation for ENTRY
  lp_sasMode = 255;
  attReset(lp_att);
  lp_entry = true;
  return true;
}

static bool lpInRange(float v, float lo, float hi) { return !(v < lo || v > hi); }
bool lpSetDescRate(float v)  { if (!lpInRange(v, lp_c.descMin, lp_c.descMax)) return false; lp_descRate = v; return true; }
bool lpSetHovrAlt(float v)   { if (!lpInRange(v, lp_c.hovrMin, lp_c.hovrMax)) return false; lp_hovrAlt = v; return true; }
bool lpSetTwr(float v)       { if (!lpInRange(v, lp_c.twrMin, lp_c.twrMax)) return false; lp_twr = v; aeSetTwrOverride(v, lp_t.gravity); return true; }
bool lpSetMargin(float v)    { if (!lpInRange(v, lp_c.marginMin, lp_c.marginMax)) return false; lp_margin = v; return true; }
bool lpSetEntryAoa(float v)  { if (!lpInRange(v, lp_c.aoaMin, lp_c.aoaMax)) return false; lp_entryAoa = v; return true; }
bool lpSetEntryRoll(float v) { if (!lpInRange(v, lp_c.rollMin, lp_c.rollMax)) return false; lp_entryRoll = v; return true; }
void lpSetAttRef(bool radial) { lp_attRefRadial = radial; }

bool lpAnyEngaged() { return lp_mode != LP_MODE_OFF || lp_entry; }

/***************************************************************************************
   Loops
****************************************************************************************/
static float lpDescLoop(float vsCmd, float dt) {
  float err = vsCmd - lp_t.velVertical;
  lp_thrInt = attClampf(lp_thrInt + lp_c.descKi * err * dt, 0.0f, 1.0f);
  float target = attClampf(lp_c.descKp * err + lp_thrInt, 0.0f, 1.0f);
  float step = lp_c.descSlew * dt;
  if      (target > lp_thrOut + step) return lp_thrOut + step;
  else if (target < lp_thrOut - step) return lp_thrOut - step;
  return target;
}

static void lpUpdateThrottleModes(uint32_t now, float dt) {
  (void)now;
  // Attitude reference: retrograde kills horizontal velocity; radial-out is pure vertical.
  // Retrograde swings wildly at low speed, so switch to radial-out automatically.
  bool radial = lp_attRefRadial || (lpHorizontalSpeed() < lp_c.radialSwitchSpeed);
  lp_attRefRadialAuto = radial && !lp_attRefRadial;
  if (!lp_entry) lpSas(radial ? AP_RADIALOUT : AP_RETROGRADE);

  lp_ignAlt = lpIgnitionAltitude();
  float a = lpAccel(), g = lp_t.gravity;
  float need = (a > g && lp_t.radarAlt > 0.0f) ? (lp_t.velSurface * lp_t.velSurface / (2.0f * fmaxf(lp_t.radarAlt, 1.0f)) + g) : 0.0f;
  lp_brakeMarginal = (need > lp_c.brakeMarginalFrac * a);

  switch (lp_mode) {
    case LP_MODE_DESC: lpThrottle(lpDescLoop(lp_descRate, dt)); break;
    case LP_MODE_HOVR: {
      float vsCmd = attClampf(lp_c.hovrKp * (lp_hovrAlt - lp_t.radarAlt), -lp_c.hovrVsCap, lp_c.hovrVsCap);
      lpThrottle(lpDescLoop(vsCmd, dt));
      break;
    }
    case LP_MODE_BRAKE: {
      if (!lp_brakeFiring) {
        lpThrottle(0.0f);
        if (lp_t.radarAlt <= lp_ignAlt && lp_t.velVertical < 0.0f) lp_brakeFiring = true;
      } else {
        lpThrottle(1.0f);
        if (lp_t.velVertical >= lp_descRate) {          // descent rate killed: hand off to DESC
          lp_mode = LP_MODE_DESC; lp_brakeFiring = false;
          lp_thrInt = lp_thrOut;
        }
      }
      break;
    }
    default: break;
  }
  asMaybeStage(asEnabled(), lp_thrOut, 50.0f);
}

static void lpUpdateEntry(uint32_t now, float dt) {
  (void)now;
  // Hand-off by vessel type (review decision q.5)
  if (lp_t.vesselType == KSP_TYPE_PLANE) {
    float q = 0.5f * lp_t.airDensity * lp_t.velSurface * lp_t.velSurface;
    if (lp_t.mach < lp_c.entryPlaneMach && q > lp_c.entryPlaneQ && lp_t.altSea < lp_t.flyHigh) {
      lp_entry = false;
      lpReleaseAttitudeOwner(false);
      lpSetReason(HP_REASON_HANDOFF);
      hpEngage(HP_MODE_ATT, true);     // captures the current pitch and bank on the aircraft console
      hpEngage(HP_MODE_ROLL, true);
      return;
    }
  } else if (lp_t.velSurface < lp_c.entryHandoffSpeed) {
    lpDropEntry(HP_REASON_HANDOFF);
    return;
  }
  AttMeasure m; m.pitch = lp_t.pitch; m.heading = lp_t.heading; m.roll = lp_t.roll;
  attUpdateRates(lp_att, m, dt);
  float refHdg = attWrap360(lp_t.orbVelHeading + 180.0f);
  float refPitch = -lp_t.orbVelPitch;
  AttCommand c = attSteerRocket(lp_att, lp_c.entryGains, m, attClampf(refPitch + lp_entryAoa, -89.0f, 89.0f), refHdg,
                                true, lp_entryRoll, lp_c.maxControlDeflection, dt);
  rotSetAutoAxes(c.pitch, c.yaw, c.roll, ROT_AXIS_PITCH | ROT_AXIS_YAW | ROT_AXIS_ROLL);
}

void lpUpdate() {
  uint32_t now = millis();
  float dt = (now - lp_lastUpdateMs) * 0.001f;
  lp_lastUpdateMs = now;
  if (dt <= 0.0f || dt > 0.5f) dt = 0.02f;
  if (!lpAnyEngaged()) return;

  // Failsafe: telemetry loss keeps the throttle where it is and hands over.
  if ((now - lp_t.lastMs) > lp_c.telemetryTimeout) { lpDisconnectAll(HP_REASON_TELEMETRY); return; }
  uint8_t ovr;
  if (pilotOverrideDetected(ovr)) { lpDisconnectAll(ovr); return; }
  if (lpOnGround()) { lpThrottle(0.0f); lpDisconnectAll(HP_REASON_LANDED); lpThrottle(0.0f); return; }

  if (lp_mode != LP_MODE_OFF) lpUpdateThrottleModes(now, dt);
  if (lp_entry) lpUpdateEntry(now, dt);
}

/***************************************************************************************
   Status / console
****************************************************************************************/
LandingStatus lpGetStatus() {
  LandingStatus s;
  s.mode = lp_mode; s.entry = lp_entry; s.engaged = lpAnyEngaged();
  s.brakeArmed = (lp_mode == LP_MODE_BRAKE && !lp_brakeFiring); s.brakeFiring = lp_brakeFiring;
  s.attRefRadial = lp_attRefRadial || lp_attRefRadialAuto; s.autoStage = asEnabled(); s.landed = lpOnGround();
  s.brakeMarginal = lp_brakeMarginal;
  s.reason = lp_reason; uint32_t a = (millis() - lp_reasonMs) / 1000UL; s.reasonAge = (lp_reason == HP_REASON_NONE) ? 255 : (a > 255 ? 255 : (uint8_t)a);
  s.accelSource = lpAccelSource();
  s.descRate = lp_descRate; s.hovrAlt = lp_hovrAlt; s.twrOverride = lp_twr; s.margin = lp_margin;
  s.entryAoa = lp_entryAoa; s.entryRoll = lp_entryRoll;
  s.ignitionAlt = lpIgnitionAltitude(); s.accelEst = lpAccel(); s.cmdThrottle = lp_thrOut;
  return s;
}

bool lpConsoleLine(const char *line) {
  if (strncasecmp(line, "ENG ", 4) == 0) {
    const char *n = line + 4; const char *sp = strchr(n, ' '); bool on = sp ? (atoi(sp + 1) != 0) : true;
    if (strncasecmp(n, "DESC", 4) == 0)  return lpEngage(LP_MODE_DESC, on);
    if (strncasecmp(n, "HOVR", 4) == 0)  return lpEngage(LP_MODE_HOVR, on);
    if (strncasecmp(n, "BRAKE", 5) == 0) return lpEngage(LP_MODE_BRAKE, on);
    if (strncasecmp(n, "ENTRY", 5) == 0) return lpEngageEntry(on);
    return false;
  }
  if (strncasecmp(line, "SET ", 4) == 0) {
    const char *a = line + 4; const char *sp = strchr(a, ' '); if (!sp) return false; float v = atof(sp + 1);
    if (strncasecmp(a, "RATE", 4) == 0)   return lpSetDescRate(v);
    if (strncasecmp(a, "ALT", 3) == 0)    return lpSetHovrAlt(v);
    if (strncasecmp(a, "TWR", 3) == 0)    return lpSetTwr(v);
    if (strncasecmp(a, "MARGIN", 6) == 0) return lpSetMargin(v);
    if (strncasecmp(a, "AOA", 3) == 0)    return lpSetEntryAoa(v);
    if (strncasecmp(a, "ROLL", 4) == 0)   return lpSetEntryRoll(v);
    if (strncasecmp(a, "RADIAL", 6) == 0) { lpSetAttRef(v != 0.0f); return true; }
    return false;
  }
  if (strncasecmp(line, "OFF", 3) == 0) { lpDisconnectAll(HP_REASON_PILOT); return true; }
  if (strncasecmp(line, "STATUS", 6) == 0) {
    LandingStatus s = lpGetStatus();
    Serial.print(F("LP mode=")); Serial.print(s.mode); Serial.print(F(" entry=")); Serial.print(s.entry);
    Serial.print(F(" ign=")); Serial.print(s.ignitionAlt, 0); Serial.print(F(" a=")); Serial.print(s.accelEst, 2);
    Serial.print(F(" src=")); Serial.print(s.accelSource); Serial.print(F(" thr=")); Serial.print(s.cmdThrottle, 2);
    Serial.print(F(" reason=")); Serial.println(hpReasonName(s.reason));
    return true;
  }
  return false;
}

/***************************************************************************************
   Telemetry ingest
****************************************************************************************/
static inline void lpStamp() { lp_t.lastMs = millis(); }
void lpIngestFlightStatus(uint8_t vesselType, uint8_t situation) { lp_t.vesselType = vesselType; lp_t.situation = situation; lpStamp(); }
void lpIngestAltitude(float sealevel, float surface) { lp_t.altSea = sealevel; lp_t.radarAlt = surface; lpStamp(); }
void lpIngestVelocity(float surface, float vertical) { lp_t.velSurface = surface; lp_t.velVertical = vertical; lpStamp(); }
void lpIngestAirspeed(float mach)                    { lp_t.mach = mach; lpStamp(); }
void lpIngestAttitude(float heading, float pitch, float roll, float orbVelHeading, float orbVelPitch) {
  lp_t.heading = heading; lp_t.pitch = pitch; lp_t.roll = roll; lp_t.orbVelHeading = orbVelHeading; lp_t.orbVelPitch = orbVelPitch; lpStamp();
}
void lpIngestAtmo(float airDensity, bool inAtmosphere) { lp_t.airDensity = airDensity; lp_t.inAtmo = inAtmosphere; lpStamp(); }
void lpIngestBody(float gravity, float flyHigh, const char *name) {
  bool changed = name && strncmp(name, lp_t.bodyName, sizeof(lp_t.bodyName)) != 0;
  if (gravity > 0.0f) lp_t.gravity = gravity;
  lp_t.flyHigh = (flyHigh > 0.0f) ? flyHigh : 18000.0f;
  if (name) { strncpy(lp_t.bodyName, name, sizeof(lp_t.bodyName) - 1); lp_t.bodyName[sizeof(lp_t.bodyName) - 1] = '\0'; }
  if (changed && lpAnyEngaged()) lpDisconnectAll(HP_REASON_SOI);
}
void lpIngestThrottle(float t01) { lp_t.throttle = attClampf(t01, 0.0f, 1.0f); }
void lpVesselChanged() {
  lp_mode = LP_MODE_OFF; lp_entry = false; lp_brakeFiring = false; lp_sasMode = 255;
  rotClearAutoAxes(); arbReleaseAttitude(AP_OWNER_LANDING); arbReleaseThrottle(AP_OWNER_LANDING);
  lp_reason = HP_REASON_NONE; lp_t.lastMs = millis();
}
