/***************************************************************************************
   burn_autopilot.ino — burn executor, planners and approach-rate hold.
   Contract in burn_autopilot.h; design in Mission_Autopilot.md §4, §7.1, §7.3, §7.7.
****************************************************************************************/
#include "burn_autopilot.h"
#include "attitude_controller.h"
#include "control_links.h"
#include "hold_autopilot.h"     // HP_REASON_* shared reason codes

static const int32_t BP_AXIS_FULL = INT16_MAX;
static const float   BP_G0 = 9.81f;

struct BpTelemetry {
  bool  nodeAvailable = false; float nodeTimeTo = 0, nodeDv = 0, nodeDuration = 0, nodeHeading = 0, nodePitch = 0;
  float ecc = 0, sma = 0, inc = 0, lan = 0, argPe = 0, trueAnom = 0, period = 0;
  float apoapsis = 0, periapsis = 0, timeToAp = 0, timeToPe = 0, velOrbital = 0;
  float heading = 0, pitch = 0, roll = 0, orbVelHeading = 0, orbVelPitch = 0;
  bool  tgtAvailable = false; float tgtDist = 0, tgtVel = 0, tgtHeading = 0, tgtPitch = 0, tgtVelHeading = 0, tgtVelPitch = 0;
  float bodyRadius = 600000.0f, bodyGravity = 9.81f; char bodyName[24] = {0};
  uint32_t lastMs = 0;
};
static BpTelemetry bp_t;
static BurnConfig  bp_c;

static BpMode   bp_mode  = BP_MODE_NONE;
static BpPhase  bp_phase = BP_PHASE_IDLE;
static BurnPlan bp_plan;
static BurnPlan bp_planAtExec;
static bool     bp_autoWarp = true;
static bool     bp_appr = false;
static float    bp_targetAp = 100000.0f, bp_targetPe = 80000.0f, bp_targetInc = 0.0f;
static float    bp_apprRate = -2.0f, bp_apprDist = 50.0f;
static float    bp_thrOut = 0.0f, bp_dvRemaining = 0.0f;
static uint8_t  bp_reason = HP_REASON_NONE; static uint32_t bp_reasonMs = 0;
static uint32_t bp_lastUpdateMs = 0, bp_alignStartMs = 0, bp_settledSince = 0, bp_burnStartMs = 0, bp_doneMs = 0, bp_lastPlanMs = 0;
static float    bp_prevRemaining = 0.0f;
static AttState bp_att;
static bool     bp_incAtAN = true;    // plane change: which node the plan uses
static float    bp_incNodeDt = 0.0f;  // s to that node at plan time
static uint32_t bp_incPlanMs = 0;

static void bpSetReason(uint8_t r) { if (r == HP_REASON_PILOT) r = HP_REASON_NONE; bp_reason = r; bp_reasonMs = millis(); }
static inline float bpMu() { return bp_t.bodyGravity * bp_t.bodyRadius * bp_t.bodyRadius; }
static inline float d2r(float d) { return d * 0.0174532925199f; }

BurnConfig bpDefaultConfig() {
  BurnConfig c;
  c.alignTolDeg = 2.0f; c.alignSettleRate = 0.5f; c.alignSettleMs = 3000; c.alignTimeoutMs = 30000;
  c.alignLeadS = 20.0f; c.taperS = 3.0f; c.throttleFloor = 0.05f; c.cutDv = 0.2f; c.minBurnS = 1.0f;
  c.replanDvFrac = 0.05f; c.replanDvMin = 2.0f; c.replanTignS = 10.0f;
  c.apprKa = 0.5f; c.apprKl = 0.5f; c.apprDeadband = 0.05f; c.apprRateDivisor = 20.0f; c.apprAbortDivisor = 5.0f;
  c.trnSignX = 1.0f; c.trnSignY = 1.0f; c.trnSignZ = 1.0f;
  c.telemetryTimeout = 2000;
  c.apMin = 0.0f; c.apMax = 2.0e9f; c.peMin = 0.0f; c.peMax = 2.0e9f; c.incMin = 0.0f; c.incMax = 180.0f;
  c.apprRateMin = -20.0f; c.apprRateMax = 5.0f; c.apprDistMin = 5.0f; c.apprDistMax = 5000.0f;
  return c;
}

void bpInit() { bp_c = bpDefaultConfig(); bp_mode = BP_MODE_NONE; bp_phase = BP_PHASE_IDLE; bp_plan.valid = false; attReset(bp_att); bp_lastUpdateMs = millis(); }
BurnConfig &bpGetConfig() { return bp_c; }

/***************************************************************************************
   Outputs
****************************************************************************************/
static void bpThrottle(float t) {
  bp_thrOut = attClampf(t, 0.0f, 1.0f);
  thrAutoThrottle(THR_OWNER_BURN, bp_thrOut);
  aeNoteThrottle(bp_thrOut);
}

static void bpReleaseVehicle(bool sasStability) {
  bpThrottle(0.0f);
  thrAutoRelease(THR_OWNER_BURN);
  if (sasStability) { mySimpit.activateAction(SAS_ACTION); mySimpit.setSASMode(AP_STABILITYASSIST); }
  arbReleaseAttitude(AP_OWNER_BURN);
  arbReleaseThrottle(AP_OWNER_BURN);
}

/***************************************************************************************
   Orbital mechanics helpers
****************************************************************************************/
// Time from true anomaly nu0 to nu1 (degrees) on the current orbit, seconds, >= 0.
static float bpTimeBetween(float nu0, float nu1) {
  float e = bp_t.ecc; if (e >= 0.999f || bp_t.period <= 0.0f) return 0.0f;
  auto meanAnom = [&](float nuDeg) {
    float nu = d2r(nuDeg);
    float E = 2.0f * atanf(sqrtf((1.0f - e) / (1.0f + e)) * tanf(nu * 0.5f));
    return E - e * sinf(E);
  };
  float dM = meanAnom(nu1) - meanAnom(nu0);
  while (dM < 0.0f) dM += 2.0f * PI;
  return dM / (2.0f * PI / bp_t.period);
}
static float bpRadiusAt(float nuDeg) {
  float e = bp_t.ecc, a = bp_t.sma;
  return a * (1.0f - e * e) / (1.0f + e * cosf(d2r(nuDeg)));
}
static float bpSpeedAt(float r) { return sqrtf(bpMu() * (2.0f / r - 1.0f / bp_t.sma)); }

/***************************************************************************************
   Planners
****************************************************************************************/
static BurnPlan bpPlanNode() {
  BurnPlan p = {}; p.valid = false;
  if (!bp_t.nodeAvailable || bp_t.nodeDv <= 0.1f) return p;
  p.sasMode = AP_MANEUVER; p.dvTotal = bp_t.nodeDv;
  p.duration = aeBurnDuration(bp_t.nodeDv);
  if (p.duration <= 0.0f) p.duration = bp_t.nodeDuration;    // Simpit's own estimate as fallback
  p.tIgnition = bp_t.nodeTimeTo - p.duration * 0.5f;
  p.warpInstant = TIMEWARP_TO_NEXT_MANEUVER; p.warpDelay = -(p.duration * 0.5f + bp_c.alignLeadS);
  p.valid = true; return p;
}

// AP: burn at periapsis to put apoapsis at the target. PE: mirror at apoapsis.
static BurnPlan bpPlanApsis(bool changeAp) {
  BurnPlan p = {}; p.valid = false;
  if (bp_t.sma <= 0.0f) return p;
  float R = bp_t.bodyRadius, mu = bpMu();
  float rBurn = R + (changeAp ? bp_t.periapsis : bp_t.apoapsis);
  float rOther = R + (changeAp ? bp_targetAp : bp_targetPe);
  if (rBurn <= 0.0f || rOther <= 0.0f) return p;
  float v  = sqrtf(mu * (2.0f / rBurn - 1.0f / bp_t.sma));
  float a2 = (rBurn + rOther) * 0.5f;
  float v2 = sqrtf(mu * (2.0f / rBurn - 1.0f / a2));
  float dv = v2 - v;
  if (fabsf(dv) < 0.5f) return p;
  p.sasMode = (dv > 0.0f) ? AP_PROGRADE : AP_RETROGRADE;
  p.dvTotal = dv;
  p.duration = aeBurnDuration(dv);
  float tTo = changeAp ? bp_t.timeToPe : bp_t.timeToAp;
  p.tIgnition = tTo - p.duration * 0.5f;
  p.warpInstant = changeAp ? TIMEWARP_TO_PERIAPSIS : TIMEWARP_TO_APOAPSIS;
  p.warpDelay = -(p.duration * 0.5f + bp_c.alignLeadS);
  p.valid = true; return p;
}

static BurnPlan bpPlanInc() {
  BurnPlan p = {}; p.valid = false;
  if (bp_t.sma <= 0.0f || bp_t.period <= 0.0f) return p;
  float di = bp_targetInc - bp_t.inc;
  if (fabsf(di) < 0.05f) return p;
  float nuAN = attWrap360(360.0f - bp_t.argPe), nuDN = attWrap360(180.0f - bp_t.argPe);
  float tAN = bpTimeBetween(bp_t.trueAnom, nuAN), tDN = bpTimeBetween(bp_t.trueAnom, nuDN);
  bp_incAtAN = (tAN <= tDN);
  float nuNode = bp_incAtAN ? nuAN : nuDN;
  float tNode  = bp_incAtAN ? tAN : tDN;
  float vNode  = bpSpeedAt(bpRadiusAt(nuNode));
  float dv = 2.0f * vNode * sinf(d2r(fabsf(di)) * 0.5f);
  // Raising inclination at the ascending node is a normal burn; at the descending node it is mirrored.
  bool normal = (di > 0.0f) == bp_incAtAN;
  p.sasMode = normal ? AP_NORMAL : AP_ANTINORMAL;
  p.dvTotal = dv;
  p.duration = aeBurnDuration(dv);
  p.tIgnition = tNode - p.duration * 0.5f;
  p.warpInstant = TIMEWARP_TO_NOW; p.warpDelay = p.tIgnition - bp_c.alignLeadS;
  bp_incNodeDt = tNode; bp_incPlanMs = millis();
  p.valid = true; return p;
}

static BurnPlan bpPlan(BpMode m) {
  switch (m) {
    case BP_MODE_NODE: return bpPlanNode();
    case BP_MODE_AP:   return bpPlanApsis(true);
    case BP_MODE_PE:   return bpPlanApsis(false);
    case BP_MODE_INC:  return bpPlanInc();
    default: { BurnPlan p = {}; p.valid = false; return p; }
  }
}

// Live ignition time and remaining delta-V for the armed plan.
static float bpTIgnitionLive() {
  switch (bp_mode) {
    case BP_MODE_NODE: return bp_t.nodeTimeTo - bp_plan.duration * 0.5f;
    case BP_MODE_AP:   return bp_t.timeToPe - bp_plan.duration * 0.5f;
    case BP_MODE_PE:   return bp_t.timeToAp - bp_plan.duration * 0.5f;
    case BP_MODE_INC:  return bp_incNodeDt - (millis() - bp_incPlanMs) * 0.001f - bp_plan.duration * 0.5f;
    default: return 0.0f;
  }
}

static float bpRemainingLive() {
  float mu = bpMu(), a = bp_t.sma;
  switch (bp_mode) {
    case BP_MODE_NODE: return bp_t.nodeDv;
    case BP_MODE_AP: {   // burning at Pe: d(Ap)/dv = 4 a^2 v / mu
      float v = bp_t.velOrbital > 1.0f ? bp_t.velOrbital : 1.0f;
      float dAp = bp_targetAp - bp_t.apoapsis;
      return (bp_plan.dvTotal >= 0.0f ? dAp : -dAp) * mu / (4.0f * a * a * v);
    }
    case BP_MODE_PE: {
      float v = bp_t.velOrbital > 1.0f ? bp_t.velOrbital : 1.0f;
      float dPe = bp_targetPe - bp_t.periapsis;
      return (bp_plan.dvTotal >= 0.0f ? dPe : -dPe) * mu / (4.0f * a * a * v);
    }
    case BP_MODE_INC: {
      float di = bp_targetInc - bp_t.inc;
      float sign = (bp_plan.dvTotal >= 0.0f && ((di > 0.0f) == bp_incAtAN) == (bp_plan.sasMode == AP_NORMAL)) ? 1.0f : 1.0f;
      (void)sign;
      return 2.0f * bp_t.velOrbital * sinf(d2r(fabsf(di)) * 0.5f) * ((fabsf(di) > 0.0f) ? 1.0f : 0.0f);
    }
    default: return 0.0f;
  }
}

/***************************************************************************************
   Pointing reference for alignment (navball frame heading / pitch)
****************************************************************************************/
static void bpPointingRef(float &hdg, float &pitch) {
  switch (bp_plan.sasMode) {
    case AP_MANEUVER:   hdg = bp_t.nodeHeading; pitch = bp_t.nodePitch; break;
    case AP_PROGRADE:   hdg = bp_t.orbVelHeading; pitch = bp_t.orbVelPitch; break;
    case AP_RETROGRADE: hdg = attWrap360(bp_t.orbVelHeading + 180.0f); pitch = -bp_t.orbVelPitch; break;
    // The orbit normal is perpendicular to the radius vector, so it is horizontal, at the
    // orbital prograde heading -90 deg (anti-normal +90). Review decision q.2.
    case AP_NORMAL:     hdg = attWrap360(bp_t.orbVelHeading - 90.0f); pitch = 0.0f; break;
    default:            hdg = attWrap360(bp_t.orbVelHeading + 90.0f); pitch = 0.0f; break;
  }
}

static float bpPointingError() {
  float h, p; bpPointingRef(h, p);
  float a1 = d2r(bp_t.pitch), a2 = d2r(p), dh = d2r(attWrap180(bp_t.heading - h));
  float c = sinf(a1) * sinf(a2) + cosf(a1) * cosf(a2) * cosf(dh);
  return acosf(attClampf(c, -1.0f, 1.0f)) * 57.2957795131f;
}

/***************************************************************************************
   Arm / execute / abort
****************************************************************************************/
static bool bpPlanChanged(const BurnPlan &a, const BurnPlan &b) {
  float dvTol = fmaxf(bp_c.replanDvMin, fabsf(a.dvTotal) * bp_c.replanDvFrac);
  return fabsf(a.dvTotal - b.dvTotal) > dvTol || fabsf(a.tIgnition - b.tIgnition) > bp_c.replanTignS || a.sasMode != b.sasMode;
}

bool bpArm(BpMode mode, bool on) {
  if (!on) {
    if (bp_mode == mode) { if (bp_phase >= BP_PHASE_ALIGN && bp_phase <= BP_PHASE_BURN) bpAbort(HP_REASON_PILOT); bp_mode = BP_MODE_NONE; bp_phase = BP_PHASE_IDLE; bp_plan.valid = false; }
    return true;
  }
  if (mode == BP_MODE_NONE || mode > BP_MODE_INC) return false;
  if ((millis() - bp_t.lastMs) > bp_c.telemetryTimeout) { bpSetReason(HP_REASON_REFUSED); return false; }
  if (bp_phase >= BP_PHASE_ALIGN && bp_phase <= BP_PHASE_BURN) bpAbort(HP_REASON_PILOT);
  BurnPlan p = bpPlan(mode);
  if (!p.valid) { bpSetReason(mode == BP_MODE_NODE ? HP_REASON_NO_NODE : HP_REASON_REFUSED); return false; }
  if (fabsf(p.dvTotal) > aeStageDv() + 1.0f && !asEnabled()) { bpSetReason(HP_REASON_FUEL); return false; }
  if (bp_appr) bpEngageApproach(false);          // exclusive with the burn modes
  bp_mode = mode; bp_plan = p; bp_phase = BP_PHASE_PLANNED; bp_lastPlanMs = millis();
  return true;
}

bool bpExecute() {
  uint32_t now = millis();
  if (bp_phase == BP_PHASE_PLANNED) {
    if (!bp_plan.valid) return false;
    arbTakeAttitude(AP_OWNER_BURN); arbTakeThrottle(AP_OWNER_BURN);
    mySimpit.activateAction(SAS_ACTION);
    mySimpit.setSASMode(bp_plan.sasMode);
    attReset(bp_att);
    bp_planAtExec = bp_plan;
    bp_phase = BP_PHASE_ALIGN; bp_alignStartMs = now; bp_settledSince = 0;
    return true;
  }
  if (bp_phase == BP_PHASE_WARP_READY && bp_autoWarp) {
    if (!arbCanWarp(AP_OWNER_BURN)) return false;
    timewarpToMessage w;
    w.instant = bp_plan.warpInstant;
    w.delay   = bp_plan.warpDelay;
    mySimpit.send(TIMEWARP_TO_MESSAGE, w);
    bp_phase = BP_PHASE_WARP;
    return true;
  }
  return false;
}

void bpAbort(uint8_t reason) {
  bool was = (bp_phase >= BP_PHASE_ALIGN && bp_phase <= BP_PHASE_BURN);
  if (was) { bpReleaseVehicle(true); bp_phase = BP_PHASE_ABORT; bp_doneMs = millis(); bpSetReason(reason); }
  else if (bp_phase != BP_PHASE_IDLE) { bp_phase = BP_PHASE_IDLE; }
  bp_mode = BP_MODE_NONE; bp_plan.valid = false;
  if (bp_appr) { bp_appr = false; bpReleaseVehicle(true); if (!was) bpSetReason(reason); }
}

void bpArbiterDrop() { bpAbort(HP_REASON_OTHER_AP); }

bool bpEngageApproach(bool on) {
  if (!on) { if (bp_appr) { bp_appr = false; bpReleaseVehicle(true); } return true; }
  if (!bp_t.tgtAvailable) { bpSetReason(HP_REASON_NO_TARGET); return false; }
  if (bp_phase != BP_PHASE_IDLE) { bp_mode = BP_MODE_NONE; bp_phase = BP_PHASE_IDLE; bp_plan.valid = false; }
  arbTakeAttitude(AP_OWNER_BURN);
  mySimpit.activateAction(SAS_ACTION);
  mySimpit.setSASMode(AP_TARGET);
  bp_appr = true;
  return true;
}

static bool bpInRange(float v, float lo, float hi) { return !(v < lo || v > hi); }
bool bpSetTargetAp(float v)  { if (!bpInRange(v, bp_c.apMin, bp_c.apMax)) return false; bp_targetAp = v; if (bp_mode == BP_MODE_AP && bp_phase == BP_PHASE_PLANNED) bp_plan = bpPlan(bp_mode); return true; }
bool bpSetTargetPe(float v)  { if (!bpInRange(v, bp_c.peMin, bp_c.peMax)) return false; bp_targetPe = v; if (bp_mode == BP_MODE_PE && bp_phase == BP_PHASE_PLANNED) bp_plan = bpPlan(bp_mode); return true; }
bool bpSetTargetInc(float v) { if (!bpInRange(v, bp_c.incMin, bp_c.incMax)) return false; bp_targetInc = v; if (bp_mode == BP_MODE_INC && bp_phase == BP_PHASE_PLANNED) bp_plan = bpPlan(bp_mode); return true; }
bool bpSetApprRate(float v)  { if (!bpInRange(v, bp_c.apprRateMin, bp_c.apprRateMax)) return false; bp_apprRate = v; return true; }
bool bpSetApprDist(float v)  { if (!bpInRange(v, bp_c.apprDistMin, bp_c.apprDistMax)) return false; bp_apprDist = v; return true; }
void bpSetAutoWarp(bool on)  { bp_autoWarp = on; }

bool bpArmed()      { return bp_phase >= BP_PHASE_PLANNED && bp_phase <= BP_PHASE_BURN; }
bool bpExecuting()  { return bp_phase >= BP_PHASE_ALIGN && bp_phase <= BP_PHASE_BURN; }
bool bpAnyEngaged() { return bpArmed() || bp_appr; }

/***************************************************************************************
   Approach-rate hold: SAS target mode keeps the nose on the target, so the body frame is
   roughly the line-of-sight frame. Relative velocity and line of sight are rotated into
   the body frame with the vessel's heading, pitch and roll; forward translation closes
   the rate error, lateral translation nulls the sideways components.
****************************************************************************************/
static void bpVec(float hdg, float pitch, float v[3]) {   // navball frame: N, E, Up
  float p = d2r(pitch), h = d2r(hdg);
  v[0] = cosf(p) * cosf(h); v[1] = cosf(p) * sinf(h); v[2] = sinf(p);
}
static float bpDot(const float a[3], const float b[3]) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
static void  bpCross(const float a[3], const float b[3], float o[3]) {
  o[0] = a[1]*b[2] - a[2]*b[1]; o[1] = a[2]*b[0] - a[0]*b[2]; o[2] = a[0]*b[1] - a[1]*b[0];
}

static void bpUpdateApproach(float dt) {
  (void)dt;
  if (!bp_t.tgtAvailable) { bpEngageApproach(false); bpSetReason(HP_REASON_NO_TARGET); return; }
  float range = bp_t.tgtDist;
  // Body axes from attitude: forward, right, up (roll rotates right/up about forward)
  float fwd[3], r0[3], up0[3], right[3], up[3];
  bpVec(bp_t.heading, bp_t.pitch, fwd);
  bpVec(bp_t.heading + 90.0f, 0.0f, r0);
  bpCross(r0, fwd, up0);
  float cr = cosf(d2r(bp_t.roll)), sr = sinf(d2r(bp_t.roll));
  for (int i = 0; i < 3; i++) { right[i] = r0[i] * cr + up0[i] * sr; up[i] = up0[i] * cr - r0[i] * sr; }
  float los[3], rel[3];
  bpVec(bp_t.tgtHeading, bp_t.tgtPitch, los);
  bpVec(bp_t.tgtVelHeading, bp_t.tgtVelPitch, rel);
  for (int i = 0; i < 3; i++) rel[i] *= bp_t.tgtVel;
  float vAlong = bpDot(rel, los);                    // + = opening
  float vLat[3]; for (int i = 0; i < 3; i++) vLat[i] = rel[i] - vAlong * los[i];
  if (vAlong < -range / bp_c.apprAbortDivisor && range > bp_apprDist) {
    bpEngageApproach(false); bpSetReason(HP_REASON_REFUSED); return;   // closing too fast for the range
  }
  float rateSp = bp_apprRate;                                          // negative = closing
  float maxClose = -range / bp_c.apprRateDivisor;
  if (rateSp < maxClose) rateSp = maxClose;
  if (range <= bp_apprDist) rateSp = 0.0f;
  float eAlong = rateSp - vAlong;
  float cmdFwd  = attClampf(bp_c.apprKa * eAlong, -1.0f, 1.0f);
  float cmdRight = attClampf(-bp_c.apprKl * bpDot(vLat, right), -1.0f, 1.0f);
  float cmdUp    = attClampf(-bp_c.apprKl * bpDot(vLat, up),    -1.0f, 1.0f);
  if (fabsf(eAlong) < bp_c.apprDeadband) cmdFwd = 0.0f;
  if (fabsf(bpDot(vLat, right)) < bp_c.apprDeadband) cmdRight = 0.0f;
  if (fabsf(bpDot(vLat, up)) < bp_c.apprDeadband) cmdUp = 0.0f;
  translationMessage t;
  t.setX((int16_t)(cmdRight * bp_c.trnSignX * (float)BP_AXIS_FULL));
  t.setY((int16_t)(cmdUp    * bp_c.trnSignY * (float)BP_AXIS_FULL));
  t.setZ((int16_t)(cmdFwd   * bp_c.trnSignZ * (float)BP_AXIS_FULL));
  mySimpit.send(TRANSLATION_MESSAGE, t);
}

/***************************************************************************************
   Executor
****************************************************************************************/
void bpUpdate() {
  uint32_t now = millis();
  float dt = (now - bp_lastUpdateMs) * 0.001f;
  bp_lastUpdateMs = now;
  if (dt <= 0.0f || dt > 0.5f) dt = 0.02f;

  if ((bp_phase == BP_PHASE_DONE || bp_phase == BP_PHASE_ABORT) && now - bp_doneMs > 5000) bp_phase = BP_PHASE_IDLE;
  if (!bpAnyEngaged()) return;

  if ((now - bp_t.lastMs) > bp_c.telemetryTimeout) { bpAbort(HP_REASON_TELEMETRY); return; }
  uint8_t ovr;
  if (pilotOverrideDetected(ovr)) { bpAbort(ovr); return; }

  if (bp_appr) { bpUpdateApproach(dt); if (!bpArmed()) return; }

  // Node vanished / stage spent
  if (bp_mode == BP_MODE_NODE && !bp_t.nodeAvailable && bp_phase != BP_PHASE_BURN) { bpAbort(HP_REASON_NO_NODE); return; }

  // Re-plan while the pilot can still see it: a material change drops back to PLANNED.
  if (bp_phase == BP_PHASE_PLANNED || bp_phase == BP_PHASE_ALIGN || bp_phase == BP_PHASE_WARP_READY) {
    if (now - bp_lastPlanMs >= 1000) {
      bp_lastPlanMs = now;
      BurnPlan p = bpPlan(bp_mode);
      if (p.valid) {
        if (bp_phase != BP_PHASE_PLANNED && bpPlanChanged(p, bp_planAtExec)) {
          bpReleaseVehicle(true); bp_phase = BP_PHASE_PLANNED; bpSetReason(HP_REASON_REPLAN);
        }
        bp_plan = p;
      }
    }
  }

  AttMeasure m; m.pitch = bp_t.pitch; m.heading = bp_t.heading; m.roll = bp_t.roll;
  attUpdateRates(bp_att, m, dt);

  switch (bp_phase) {
    case BP_PHASE_ALIGN: {
      float err = bpPointingError();
      float rate = fmaxf(fabsf(bp_att.ratePitch), fmaxf(fabsf(bp_att.rateHeading), fabsf(bp_att.rateRoll)));
      if (rate < bp_c.alignSettleRate) { if (bp_settledSince == 0) bp_settledSince = now; } else bp_settledSince = 0;
      bool settled = bp_settledSince && (now - bp_settledSince >= bp_c.alignSettleMs);
      if (err < bp_c.alignTolDeg && settled) bp_phase = BP_PHASE_WARP_READY;
      else if (now - bp_alignStartMs > bp_c.alignTimeoutMs) { bpAbort(HP_REASON_ALIGN); return; }
      break;
    }
    case BP_PHASE_WARP_READY:
    case BP_PHASE_WARP: {
      // Alignment is re-checked continuously (a pilot warp can disturb it); ignition when due.
      if (bpPointingError() > bp_c.alignTolDeg * 2.0f && bp_phase == BP_PHASE_WARP_READY) { bp_phase = BP_PHASE_ALIGN; bp_alignStartMs = now; bp_settledSince = 0; break; }
      if (bpTIgnitionLive() <= 0.0f) {
        bp_phase = BP_PHASE_BURN; bp_burnStartMs = now; bp_prevRemaining = bpRemainingLive();
      }
      break;
    }
    case BP_PHASE_BURN: {
      float rem = bpRemainingLive();
      bp_dvRemaining = rem;
      float a = aeAccel();
      float taperBand = (a > 0.0f) ? a * bp_c.taperS : 5.0f;
      float thr = 1.0f;
      if (rem < taperBand) thr = fmaxf(bp_c.throttleFloor, rem / taperBand);
      bool overshoot = (now - bp_burnStartMs > (uint32_t)(bp_c.minBurnS * 1000)) && (rem > bp_prevRemaining + 0.05f);
      if (rem <= bp_c.cutDv || overshoot) {
        bpReleaseVehicle(true);
        bp_phase = BP_PHASE_DONE; bp_doneMs = now; bp_mode = BP_MODE_NONE; bp_plan.valid = false;
        break;
      }
      bp_prevRemaining = rem;
      if (aeStageDv() < 5.0f && !asEnabled() && rem > 5.0f) { bpAbort(HP_REASON_FUEL); return; }
      bpThrottle(thr);
      asMaybeStage(asEnabled(), thr, rem);
      break;
    }
    default: break;
  }
}

/***************************************************************************************
   Status / console
****************************************************************************************/
static uint8_t bpAge(uint8_t r, uint32_t ms) { if (r == HP_REASON_NONE) return 255; uint32_t a = (millis() - ms) / 1000UL; return a > 255 ? 255 : (uint8_t)a; }

BurnStatus bpGetStatus() {
  BurnStatus s;
  s.mode = bp_mode; s.phase = bp_phase; s.reason = bp_reason; s.reasonAge = bpAge(bp_reason, bp_reasonMs);
  s.armed = bpArmed(); s.executing = bpExecuting(); s.autoWarp = bp_autoWarp; s.autoStage = asEnabled();
  s.targetAvailable = bp_t.tgtAvailable; s.nodeAvailable = bp_t.nodeAvailable; s.apprEngaged = bp_appr;
  s.targetAp = bp_targetAp; s.targetPe = bp_targetPe; s.targetInc = bp_targetInc; s.apprRate = bp_apprRate; s.apprDist = bp_apprDist;
  s.dvTotal = bp_plan.valid ? fabsf(bp_plan.dvTotal) : 0.0f;
  s.dvRemaining = (bp_phase == BP_PHASE_BURN) ? bp_dvRemaining : s.dvTotal;
  s.tIgnition = bp_plan.valid ? bpTIgnitionLive() : 0.0f;
  s.burnDuration = bp_plan.valid ? bp_plan.duration : 0.0f;
  s.accelEst = aeAccel(); s.cmdThrottle = bp_thrOut;
  return s;
}

const char *bpPhaseName(uint8_t p) {
  switch (p) {
    case BP_PHASE_PLANNED: return "PLANNED"; case BP_PHASE_ALIGN: return "ALIGNING"; case BP_PHASE_WARP_READY: return "WARP READY";
    case BP_PHASE_WARP: return "WARPING"; case BP_PHASE_BURN: return "BURNING"; case BP_PHASE_DONE: return "DONE"; case BP_PHASE_ABORT: return "ABORT";
    default: return "IDLE";
  }
}

bool bpConsoleLine(const char *line) {
  if (strncasecmp(line, "ARM ", 4) == 0) {
    const char *n = line + 4;
    BpMode m = strncasecmp(n, "NODE", 4) == 0 ? BP_MODE_NODE : strncasecmp(n, "AP", 2) == 0 ? BP_MODE_AP :
               strncasecmp(n, "PE", 2) == 0 ? BP_MODE_PE : strncasecmp(n, "INC", 3) == 0 ? BP_MODE_INC : BP_MODE_NONE;
    return bpArm(m, true);
  }
  if (strncasecmp(line, "EXEC", 4) == 0)  return bpExecute();
  if (strncasecmp(line, "APPR ", 5) == 0) return bpEngageApproach(atoi(line + 5) != 0);
  if (strncasecmp(line, "WARP ", 5) == 0) { bpSetAutoWarp(atoi(line + 5) != 0); return true; }
  if (strncasecmp(line, "STAGE ", 6) == 0) { asSetEnabled(atoi(line + 6) != 0); return true; }
  if (strncasecmp(line, "SET ", 4) == 0) {
    const char *a = line + 4; const char *sp = strchr(a, ' '); if (!sp) return false; float v = atof(sp + 1);
    if (strncasecmp(a, "AP", 2) == 0 && a[2] == ' ')  return bpSetTargetAp(v);
    if (strncasecmp(a, "PE", 2) == 0 && a[2] == ' ')  return bpSetTargetPe(v);
    if (strncasecmp(a, "INC", 3) == 0) return bpSetTargetInc(v);
    if (strncasecmp(a, "RATE", 4) == 0) return bpSetApprRate(v);
    if (strncasecmp(a, "DIST", 4) == 0) return bpSetApprDist(v);
    return false;
  }
  if (strncasecmp(line, "OFF", 3) == 0) { bpAbort(HP_REASON_PILOT); return true; }
  if (strncasecmp(line, "STATUS", 6) == 0) {
    BurnStatus s = bpGetStatus();
    Serial.print(F("BP mode=")); Serial.print(s.mode); Serial.print(F(" phase=")); Serial.print(bpPhaseName(s.phase));
    Serial.print(F(" dv=")); Serial.print(s.dvTotal, 1); Serial.print(F(" rem=")); Serial.print(s.dvRemaining, 1);
    Serial.print(F(" tign=")); Serial.print(s.tIgnition, 0); Serial.print(F(" dur=")); Serial.print(s.burnDuration, 0);
    Serial.print(F(" a=")); Serial.print(s.accelEst, 2); Serial.print(F(" reason=")); Serial.println(hpReasonName(s.reason));
    return true;
  }
  return false;
}

/***************************************************************************************
   Telemetry ingest
****************************************************************************************/
static inline void bpStamp() { bp_t.lastMs = millis(); }
void bpIngestNode(float timeTo, float dv, float duration, float heading, float pitch) {
  bp_t.nodeAvailable = (dv > 0.01f); bp_t.nodeTimeTo = timeTo; bp_t.nodeDv = dv; bp_t.nodeDuration = duration;
  bp_t.nodeHeading = heading; bp_t.nodePitch = pitch; bpStamp();
}
void bpIngestOrbit(float ecc, float sma, float inc, float lan, float argPe, float trueAnom, float period) {
  bp_t.ecc = ecc; bp_t.sma = sma; bp_t.inc = inc; bp_t.lan = lan; bp_t.argPe = argPe; bp_t.trueAnom = trueAnom; bp_t.period = period; bpStamp();
}
void bpIngestApsides(float apoapsis, float periapsis) { bp_t.apoapsis = apoapsis; bp_t.periapsis = periapsis; bpStamp(); }
void bpIngestApsidesTime(float toAp, float toPe)      { bp_t.timeToAp = toAp; bp_t.timeToPe = toPe; bpStamp(); }
void bpIngestVelocity(float orbital)                  { bp_t.velOrbital = orbital; bpStamp(); }
void bpIngestAttitude(float heading, float pitch, float roll, float orbVelHeading, float orbVelPitch) {
  bp_t.heading = heading; bp_t.pitch = pitch; bp_t.roll = roll; bp_t.orbVelHeading = orbVelHeading; bp_t.orbVelPitch = orbVelPitch; bpStamp();
}
void bpIngestTarget(bool available, float distance, float velocity, float heading, float pitch, float velHeading, float velPitch) {
  bp_t.tgtAvailable = available; bp_t.tgtDist = distance; bp_t.tgtVel = velocity; bp_t.tgtHeading = heading; bp_t.tgtPitch = pitch;
  bp_t.tgtVelHeading = velHeading; bp_t.tgtVelPitch = velPitch;
}
void bpIngestBody(float radius, float gravity, const char *name) {
  bool changed = name && strncmp(name, bp_t.bodyName, sizeof(bp_t.bodyName)) != 0;
  if (radius > 0.0f) bp_t.bodyRadius = radius;
  if (gravity > 0.0f) bp_t.bodyGravity = gravity;
  if (name) { strncpy(bp_t.bodyName, name, sizeof(bp_t.bodyName) - 1); bp_t.bodyName[sizeof(bp_t.bodyName) - 1] = '\0'; }
  if (changed && bpAnyEngaged()) bpAbort(HP_REASON_SOI);
}
void bpVesselChanged() {
  bp_mode = BP_MODE_NONE; bp_phase = BP_PHASE_IDLE; bp_plan.valid = false; bp_appr = false;
  arbReleaseAttitude(AP_OWNER_BURN); arbReleaseThrottle(AP_OWNER_BURN);
  bp_reason = HP_REASON_NONE; bp_t.lastMs = millis();
}
