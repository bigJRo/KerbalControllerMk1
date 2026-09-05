/********************************************************************************************************************************
  Ascent Autopilot — implementation (Master Teensy 4.1)

  Runs a launch-to-orbit guidance loop over the master controller's KerbalSimpit link. See ascent_autopilot.h for the public
  API and the tunable AscentConfig surface.

  Because all sketch .ino files compile as a single translation unit, this file references the global `mySimpit` object and the
  KerbalSimpit message types / action constants declared in Controller_Main.ino directly.

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Written for Jeb's Controller Works.
********************************************************************************************************************************/
#include "ascent_autopilot.h"
#include "attitude_controller.h"
#include "hold_autopilot.h"
#include "burn_autopilot.h"
#include "landing_autopilot.h"
#include "control_links.h"

/***************************************************************************************
   KerbalSimpit control-axis scaling.
   Rotation axes are int16 with full deflection at +/-INT16_MAX; throttle is 0..INT16_MAX.
   If a particular Simpit build uses a different full-scale, adjust AP_AXIS_FULL only.
****************************************************************************************/
static const int32_t AP_AXIS_FULL = INT16_MAX;

/***************************************************************************************
   Module state
****************************************************************************************/
static AscentConfig g_cfg;                 // Active configuration
static AscentPhase  g_phase   = AP_PHASE_IDLE;
static bool         g_armed   = false;

// Latest commanded outputs (mirrored into AscentStatus)
static float g_cmdPitch    = 90.0f;
static float g_cmdHeading  = 90.0f;
static float g_cmdThrottle = 0.0f;
static float g_dynPressure = 0.0f;

// Phase / event bookkeeping
static uint32_t g_lastStageMs   = 0;
static bool     g_sasIsOff       = false;  // we have commanded stock SAS off (active raw steering)
static bool     g_sasProgradeSet = false;  // we have commanded stock SAS on + prograde hold (coast)

// Attitude loop state (shared attitude controller — attitude_controller.h)
static AttState g_att;
static uint32_t g_lastUpdateMs  = 0;       // wall clock of last apUpdate (for dt)

/***************************************************************************************
   Telemetry snapshot, fed by apIngest*()
****************************************************************************************/
struct ApTelemetry {
  bool     inFlight      = false;
  float    altSurface    = 0.0f;   // m
  float    altSea        = 0.0f;   // m
  float    velSurface    = 0.0f;   // m/s
  float    velVertical   = 0.0f;   // m/s
  float    velOrbital    = 0.0f;   // m/s
  float    apoapsis      = 0.0f;   // m
  float    periapsis     = 0.0f;   // m
  float    timeToAp      = 0.0f;   // s
  float    inclination   = 0.0f;   // deg
  float    heading       = 0.0f;   // deg (current attitude)
  float    pitch         = 0.0f;   // deg (current attitude, above horizon)
  float    roll          = 0.0f;   // deg (current attitude)
  float    srfVelHeading = 0.0f;   // deg (surface prograde heading — used for AoA / gravity turn)
  float    srfVelPitch   = 0.0f;   // deg (surface prograde pitch)
  float    orbVelHeading = 0.0f;   // deg (orbital prograde heading — used for coast / circularization)
  float    orbVelPitch   = 0.0f;   // deg (orbital prograde pitch)
  float    airDensity    = 0.0f;   // kg/m^3
  float    gForce        = 0.0f;   // felt acceleration in g (from AIRSPEED_MESSAGE)
  bool     hasAtmo       = true;   // current body has an atmosphere (from ATMO_CONDITIONS)
  bool     inAtmo        = true;   // vessel is currently within the atmosphere
  float    skinTempFrac  = 0.0f;   // 0..1
  float    stageDV       = 1.0e6f; // m/s (start high so we do not stage before data arrives)
  char     bodyName[24]  = {0};    // current sphere-of-influence body (from SOI_MESSAGE)
  uint32_t lastTelemMs   = 0;      // freshness stamp
};
static ApTelemetry g_tel;

/***************************************************************************************
   Current-body parameters come from the shared celestial-body table (single source of
   truth in Software/Common/body_params.h, included by Controller_Main.ino). getBodyParams()
   is keyed on the SOI_MESSAGE name; an unrecognised body returns an empty entry
   (soiName[0] == '\0') and guidance falls back to telemetry-driven behaviour.
****************************************************************************************/
static BodyParams g_curBody = { "", "", "", "", 0, 0, 0, 0, 0, 0.0,
                                0, 0.0f, 0.0f, 0, 0, 0.0f, false, false, false, 0.0f };
static bool       g_targetLocked = false;   // pilot has set an explicit target apoapsis

static inline bool apBodyKnown() { return g_curBody.soiName != nullptr && g_curBody.soiName[0] != '\0'; }

// Suggested parking-orbit altitude for the current body, derived from the shared table:
// just above the atmosphere on atmospheric bodies, or terrain (minSafe) plus margin on
// airless bodies. Returns 0 for an unknown body (caller keeps the existing target).
static float apBodyDefaultOrbit() {
  if (!apBodyKnown()) return 0.0f;
  if (g_curBody.hasAtmo && g_curBody.lowSpace > 0.0f) return g_curBody.lowSpace + 10000.0f;
  return g_curBody.minSafe + max(8000.0f, g_curBody.minSafe * 0.5f);
}

// Minimum safe orbit altitude (terrain / atmosphere clearance) for the current body.
static float apBodyMinSafeOrbit() {
  if (!apBodyKnown()) return 0.0f;
  if (g_curBody.hasAtmo && g_curBody.lowSpace > 0.0f) return g_curBody.lowSpace + 1000.0f;
  return g_curBody.minSafe + 5000.0f;
}

/***************************************************************************************
   Small math helpers
****************************************************************************************/
static inline float apClampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float apDeg2Rad(float d) { return d * 0.0174532925199f; }
static inline float apRad2Deg(float r) { return r * 57.2957795131f; }

// Wrap an angle error into [-180, 180]
static float apWrap180(float a) {
  while (a > 180.0f)  a -= 360.0f;
  while (a < -180.0f) a += 360.0f;
  return a;
}

// Wrap a heading into [0, 360)
static float apWrap360(float a) {
  while (a >= 360.0f) a -= 360.0f;
  while (a < 0.0f)    a += 360.0f;
  return a;
}

/***************************************************************************************
   Launch azimuth from target inclination and site latitude.
   Standard spherical result: sin(azimuth) = cos(inclination) / cos(latitude), with the
   azimuth measured clockwise from north. This is the inertial azimuth (it does not
   correct for the body's surface rotation velocity); use headingBias to trim, or the
   southerly flag for the descending-node solution. Retrograde targets are approximate.
****************************************************************************************/
static float apLaunchAzimuth() {
  float cosI = cosf(apDeg2Rad(g_cfg.targetInclination));
  float cosL = cosf(apDeg2Rad(g_cfg.launchLatitude));
  if (fabsf(cosL) < 1.0e-4f) cosL = (cosL < 0 ? -1.0e-4f : 1.0e-4f);
  float s = apClampf(cosI / cosL, -1.0f, 1.0f);
  float az = apRad2Deg(asinf(s));         // principal value, [-90, 90] from north
  if (g_cfg.launchSoutherly) az = 180.0f - az;   // descending-node / southerly branch
  return apWrap360(az + g_cfg.headingBias);
}

/***************************************************************************************
   Pitch program: 90 deg (straight up) at the turn start, easing to finalPitch by
   turnEndAltitude. `loft` shapes the curve — exponent < 1 pitches over aggressively,
   > 1 stays steeper longer. An initial kick lowers the starting pitch to commit the turn.
****************************************************************************************/
// Body-relative turn-end altitude: a fraction of the atmosphere top on atmospheric
// bodies, or a fraction of the target apoapsis on airless / unknown-atmosphere bodies.
// Falls back to the manual turnEndAltitude when body-profile mode is off.
static float apEffectiveTurnEnd() {
  if (!g_cfg.autoBodyProfile) return g_cfg.turnEndAltitude;
  if (g_tel.hasAtmo && g_curBody.lowSpace > 0.0f)
    return g_cfg.turnEndAtmoFraction * g_curBody.lowSpace;
  return g_cfg.turnEndAirlessFraction * g_cfg.targetApoapsis;
}

static float apScheduledPitch() {
  float span = apEffectiveTurnEnd() - g_cfg.turnStartAltitude;
  if (span < 1.0f) span = 1.0f;
  float frac = (g_tel.altSurface - g_cfg.turnStartAltitude) / span;
  frac = apClampf(frac, 0.0f, 1.0f);
  float top   = 90.0f - g_cfg.initialPitchKick;                 // pitch just after the kick
  float pitch = top - (top - g_cfg.finalPitch) * powf(frac, g_cfg.loft);
  return pitch;
}

/***************************************************************************************
   Acceleration (g-force) limiter: throttle back to hold the felt acceleration below
   maxG (analogous to MechJeb's "limit acceleration"). Uses the gForces telemetry, so it
   needs no mass/thrust knowledge. Applied to every powered phase, not just the turn.
****************************************************************************************/
static float apLimitG(float thr) {
  if (g_cfg.maxG > 0.0f && g_tel.gForce > g_cfg.maxG) {
    float scale = g_cfg.maxG / g_tel.gForce;                     // proportional back-off
    thr = min(thr, max(g_cfg.maxGThrottleFloor, thr * scale));
  }
  return apClampf(thr, 0.0f, 1.0f);
}

/***************************************************************************************
   Throttle manager for powered flight: starts from launchThrottle and applies the
   max-Q, skin-temperature, apoapsis-approach, and max-G limits, taking the most
   restrictive.
****************************************************************************************/
static float apManagedThrottle() {
  float thr = g_cfg.launchThrottle;

  // Max-Q limiting: q = 1/2 * rho * v^2 (atmospheric bodies only)
  g_dynPressure = 0.5f * g_tel.airDensity * g_tel.velSurface * g_tel.velSurface;
  if (g_tel.hasAtmo && g_cfg.maxQ > 0.0f && g_dynPressure > g_cfg.maxQ) {
    float scale = g_cfg.maxQ / g_dynPressure;                   // proportional back-off
    thr = min(thr, max(g_cfg.maxQThrottleFloor, g_cfg.launchThrottle * scale));
  }

  // Skin-temperature limiting: ease off as skin temp approaches its limit
  if (g_cfg.skinTempLimit > 0.0f && g_tel.skinTempFrac > g_cfg.skinTempLimit) {
    float over = (g_tel.skinTempFrac - g_cfg.skinTempLimit) / max(0.01f, 1.0f - g_cfg.skinTempLimit);
    thr = min(thr, max(g_cfg.maxQThrottleFloor, 1.0f - over));
  }

  // Apoapsis-approach taper: throttle down smoothly as apoapsis nears the target
  float taperFrom = g_cfg.apoTaperStart * g_cfg.targetApoapsis;
  if (g_tel.apoapsis >= taperFrom && g_cfg.targetApoapsis > 0.0f) {
    float remaining = (g_cfg.targetApoapsis - g_tel.apoapsis);
    float band      = max(1.0f, (1.0f - g_cfg.apoTaperStart) * g_cfg.targetApoapsis);
    float t         = apClampf(remaining / band, 0.0f, 1.0f);   // 1 at taper start -> 0 at target
    thr = min(thr, max(g_cfg.apoTaperFloor, t));
  }

  return apLimitG(thr);   // acceleration limit (also clamps to [0,1])
}

/***************************************************************************************
   Emit throttle (0..1) to KSP
****************************************************************************************/
static void apSendThrottle(float f) {
  f = apClampf(f, 0.0f, 1.0f);
  g_cmdThrottle = f;
  // Through the throttle link so the Throttle Module's motorised lever rides the
  // managed throttle (max-Q / max-G / apoapsis taper). A pilot grabbing the lever
  // disarms the autopilot on the next update (pilotOverrideDetected).
  thrAutoThrottle(THR_OWNER_ASCENT, f);
}

/***************************************************************************************
   Steering: drive the vehicle toward (cmdPitch above horizon, cmdHeading).

   Attitude errors are computed in the navball frame, then rotated into the body frame
   by the current roll angle so pitch/yaw commands stay correct regardless of roll.
   A PID on each axis produces the stick command; roll is optionally held at targetRoll.
****************************************************************************************/
static void apSteer(float cmdPitch, float cmdHeading, float dt) {
  g_cmdPitch   = cmdPitch;
  g_cmdHeading = cmdHeading;

  AttMeasure m;
  m.pitch = g_tel.pitch; m.heading = g_tel.heading; m.roll = g_tel.roll;
  attUpdateRates(g_att, m, dt);

  AttGains g;
  g.pitchKp = g_cfg.pitchKp; g.pitchKi = g_cfg.pitchKi; g.pitchKd = g_cfg.pitchKd;
  g.yawKp   = g_cfg.yawKp;   g.yawKi   = g_cfg.yawKi;   g.yawKd   = g_cfg.yawKd;
  g.rollKp  = g_cfg.rollKp;  g.rollKi  = g_cfg.rollKi;  g.rollKd  = g_cfg.rollKd;

  AttCommand c = attSteerRocket(g_att, g, m, cmdPitch, cmdHeading,
                                g_cfg.rollControlEnabled, g_cfg.targetRoll,
                                g_cfg.maxControlDeflection, dt);

  rotationMessage msg;
  msg.setPitch((int16_t)(c.pitch * (float)AP_AXIS_FULL));
  msg.setYaw((int16_t)(c.yaw  * (float)AP_AXIS_FULL));
  if (g_cfg.rollControlEnabled) msg.setRoll((int16_t)(c.roll * (float)AP_AXIS_FULL));
  mySimpit.send(ROTATION_MESSAGE, msg);
}

// Command the vehicle to hold orbital prograde (used during coast / circularization
// when we steer actively rather than delegating to stock SAS).
static void apSteerPrograde(float dt) {
  apSteer(g_tel.orbVelPitch, g_tel.orbVelHeading, dt);
}

// True while the module is driving raw rotation commands (and therefore needs stock
// SAS off). During coast/circularization we defer to stock SAS if useStockSASForCoast.
static bool apActiveSteering() {
  switch (g_phase) {
    case AP_PHASE_VERTICAL:
    case AP_PHASE_GRAVITY_TURN: return true;
    case AP_PHASE_COAST:
    case AP_PHASE_CIRCULARIZE:  return !g_cfg.useStockSASForCoast;
    default:                    return false;
  }
}

// Reconcile stock SAS with the current steering mode: SAS off while we send raw
// rotation, SAS on + prograde hold while we defer to it. Only acts on transitions.
static void apReconcileSAS() {
  if (apActiveSteering()) {
    if (!g_sasIsOff) {
      mySimpit.deactivateAction(SAS_ACTION);
      g_sasIsOff = true;
      g_sasProgradeSet = false;
    }
  } else {
    if (!g_sasProgradeSet) {
      mySimpit.activateAction(SAS_ACTION);
      mySimpit.setSASMode(AP_PROGRADE);
      g_sasProgradeSet = true;
      g_sasIsOff = false;
    }
  }
}

/***************************************************************************************
   Auto-staging: fire STAGE_ACTION when the active stage's delta-V is spent, with a
   lockout so a single depletion does not trigger a burst of stagings.
****************************************************************************************/
static void apMaybeStage() {
  if (!g_cfg.autoStage) return;
  uint32_t now = millis();
  if (g_tel.stageDV < g_cfg.stageDVThreshold && (now - g_lastStageMs) > g_cfg.stageMinInterval) {
    mySimpit.activateAction(STAGE_ACTION);
    g_lastStageMs = now;
  }
}

/***************************************************************************************
   Public API
****************************************************************************************/
AscentConfig apDefaultConfig() {
  AscentConfig c;
  // Mission targets — 80 km circular equatorial from KSC
  c.targetApoapsis     = 80000.0f;
  c.targetInclination  = 0.0f;
  c.launchSoutherly    = false;
  c.launchLatitude     = 0.0f;      // KSC is ~0.1 deg; 0 is a fine approximation
  c.headingBias        = 0.0f;

  // Body / sphere-of-influence handling
  c.autoBodyProfile       = true;   // adapt to whatever SoI the craft is in (atmospheric or airless)
  c.turnEndAtmoFraction   = 0.80f;  // atmospheric: level off by ~80% of atmosphere top
  c.turnEndAirlessFraction = 0.25f; // airless: pitch over within ~25% of target apoapsis
  c.enforceMinSafeAltitude = true;  // clamp target up to the body's minimum safe altitude on arm

  // Ascent shape
  c.turnStartAltitude  = 500.0f;
  c.turnStartVelocity  = 60.0f;
  c.turnEndAltitude    = 55000.0f;  // manual fallback (used only when autoBodyProfile is false)
  c.loft               = 1.0f;      // 1.0 = balanced; lower = aggressive, higher = lofted
  c.initialPitchKick   = 3.0f;
  c.finalPitch         = 0.0f;

  // Throttle management
  c.launchThrottle     = 1.0f;
  c.autoLaunch         = false;
  c.maxQ               = 0.0f;      // 0 = off by default; a typical KSP value is ~18000-25000 Pa
  c.maxQThrottleFloor  = 0.5f;
  c.maxG               = 0.0f;      // 0 = off by default; e.g. 4.0 to cap felt acceleration at 4 g
  c.maxGThrottleFloor  = 0.30f;
  c.skinTempLimit      = 0.0f;      // 0 = off; e.g. 0.85 to ease off at 85% skin temp
  c.apoTaperStart      = 0.92f;
  c.apoTaperFloor      = 0.10f;

  // Steering / control authority
  c.aoaLimit           = 5.0f;      // keep commanded pitch within 5 deg of surface prograde
  c.pitchKp = 0.9f; c.pitchKi = 0.05f; c.pitchKd = 0.0f;
  c.yawKp   = 0.9f; c.yawKi   = 0.05f; c.yawKd   = 0.0f;
  c.rollKp  = 0.6f; c.rollKi  = 0.02f; c.rollKd  = 0.0f;
  c.rollControlEnabled = false;
  c.targetRoll         = 0.0f;
  c.maxControlDeflection = 1.0f;

  // Staging
  c.autoStage          = true;
  c.stageDVThreshold   = 5.0f;
  c.stageMinInterval   = 2000;

  // Circularization
  c.circularize        = true;
  c.circStartLeadTime  = 10.0f;
  c.circPeTolerance    = 1000.0f;

  // Safety
  c.telemetryTimeout   = 2000;
  c.useStockSASForCoast = true;
  return c;
}

void apInit() {
  g_cfg   = apDefaultConfig();
  g_phase = AP_PHASE_IDLE;
  g_armed = false;
  g_lastUpdateMs = millis();
}

AscentConfig &apGetConfig() { return g_cfg; }
void apSetConfig(const AscentConfig &cfg) { g_cfg = cfg; }

void apSetTargets(float apoapsisM, float inclinationDeg, float loft) {
  // Convenience mission set; respects the same disarmed-only guard as the field setters.
  apSetTargetAltitude(apoapsisM);
  apSetTargetInclination(inclinationDeg);
  apSetLoft(loft);
}

const char *apCurrentBody() { return g_tel.bodyName; }

void apArm() {
  g_armed          = true;
  g_phase          = AP_PHASE_VERTICAL;
  attReset(g_att);
  arbTakeAttitude(AP_OWNER_ASCENT);   // arming takes the vehicle from any other autopilot (ap_arbiter.ino)
  arbTakeThrottle(AP_OWNER_ASCENT);
  g_sasIsOff       = false;   // force apReconcileSAS() to command SAS off on the first pass
  g_sasProgradeSet = false;
  g_lastStageMs    = millis();
  g_lastUpdateMs = millis();
  // Terrain-clearance guard: never target below the body's minimum safe altitude.
  float minSafe = apBodyMinSafeOrbit();
  if (g_cfg.enforceMinSafeAltitude && minSafe > 0.0f && g_cfg.targetApoapsis < minSafe) {
    g_cfg.targetApoapsis = minSafe;
    mySimpit.printToKSP("Ascent AP: target raised to min safe altitude", PRINT_TO_SCREEN);
  }
  if (g_cfg.autoLaunch) {
    mySimpit.activateAction(STAGE_ACTION);   // ignite first stage
    g_lastStageMs = millis();
  }
}

void apDisarm() {
  g_armed = false;
  g_phase = AP_PHASE_IDLE;
  apSendThrottle(0.0f);
  thrAutoRelease(THR_OWNER_ASCENT);
  arbReleaseAttitude(AP_OWNER_ASCENT);
  arbReleaseThrottle(AP_OWNER_ASCENT);
  // Zero the control axes so we do not leave the stick deflected.
  rotationMessage msg;
  msg.setPitch(0); msg.setYaw(0); msg.setRoll(0);
  mySimpit.send(ROTATION_MESSAGE, msg);
}

bool        apIsArmed()  { return g_armed; }
void        apArbiterDrop() { if (g_armed) { apDisarm(); mySimpit.printToKSP("Ascent AP: another autopilot engaged - disarmed", PRINT_TO_SCREEN); } }
AscentPhase apGetPhase() { return g_phase; }

const char *apPhaseName(AscentPhase phase) {
  switch (phase) {
    case AP_PHASE_IDLE:         return "IDLE";
    case AP_PHASE_VERTICAL:     return "VERTICAL";
    case AP_PHASE_GRAVITY_TURN: return "GRAVITY TURN";
    case AP_PHASE_COAST:        return "COAST";
    case AP_PHASE_CIRCULARIZE:  return "CIRCULARIZE";
    case AP_PHASE_COMPLETE:     return "COMPLETE";
    case AP_PHASE_ABORT:        return "ABORT";
    default:                    return "?";
  }
}

AscentStatus apGetStatus() {
  AscentStatus s;
  s.armed          = g_armed;
  s.phase          = g_phase;
  s.phaseName      = apPhaseName(g_phase);
  s.body           = g_tel.bodyName;
  s.targetApoapsis = g_cfg.targetApoapsis;
  s.apoapsis       = g_tel.apoapsis;
  s.periapsis      = g_tel.periapsis;
  s.cmdPitch       = g_cmdPitch;
  s.cmdHeading     = g_cmdHeading;
  s.cmdThrottle    = g_cmdThrottle;
  s.gForce         = g_tel.gForce;
  s.dynPressure    = g_dynPressure;
  return s;
}

/***************************************************************************************
   Console-facing setters — apply only while DISARMED (return false, no change, if armed).
****************************************************************************************/
bool apSetTargetAltitude(float meters) {
  if (g_armed || meters < 0.0f) return false;
  g_cfg.targetApoapsis = meters;
  g_targetLocked = true;
  return true;
}
bool apSetTargetInclination(float deg) {
  if (g_armed || deg < 0.0f || deg > 180.0f) return false;
  g_cfg.targetInclination = deg;
  return true;
}
bool apSetLaunchSoutherly(bool southerly) {
  if (g_armed) return false;
  g_cfg.launchSoutherly = southerly;
  return true;
}
bool apSetLoft(float exponent) {
  if (g_armed || exponent <= 0.0f) return false;
  g_cfg.loft = exponent;
  return true;
}
bool apSetRoll(bool enabled, float deg) {
  if (g_armed) return false;
  g_cfg.rollControlEnabled = enabled;
  g_cfg.targetRoll = apClampf(deg, -180.0f, 180.0f);
  return true;
}
bool apSetMaxG(float g) {
  if (g_armed || g < 0.0f) return false;
  g_cfg.maxG = g;
  return true;
}

/***************************************************************************************
   Main guidance update — call every loop().
****************************************************************************************/
void apUpdate() {
  if (!g_armed) return;

  uint32_t now = millis();
  float dt = (now - g_lastUpdateMs) * 0.001f;
  g_lastUpdateMs = now;
  if (dt <= 0.0f || dt > 0.5f) dt = 0.02f;   // guard against stalls / first pass

  // --- Failsafe: telemetry loss ---
  if ((now - g_tel.lastTelemMs) > g_cfg.telemetryTimeout) {
    apSendThrottle(0.0f);
    g_phase = AP_PHASE_ABORT;
    g_armed = false;
    return;
  }

  // The pilot has the vehicle: any input on the rotation stick, the translation stick or
  // the throttle lever disarms the ascent autopilot (global rule, control_links.h).
  {
    uint8_t ovr;
    if (pilotOverrideDetected(ovr)) {
      apDisarm();
      mySimpit.printToKSP(ovr == HP_REASON_LEVER ? "Ascent AP: throttle lever - disarmed"
                                                  : "Ascent AP: stick input - disarmed", PRINT_TO_SCREEN);
      return;
    }
  }

  // Keep stock SAS consistent with the current steering mode (off while we send raw
  // rotation; on + prograde while we defer to it during coast/circularization).
  apReconcileSAS();

  float azimuth = apLaunchAzimuth();
  g_cmdHeading  = azimuth;

  switch (g_phase) {

    case AP_PHASE_VERTICAL: {
      apSendThrottle(apLimitG(g_cfg.launchThrottle));
      apSteer(90.0f, azimuth, dt);
      apMaybeStage();
      bool altTrig = g_tel.altSurface >= g_cfg.turnStartAltitude;
      bool velTrig = (g_cfg.turnStartVelocity > 0.0f) && (g_tel.velSurface >= g_cfg.turnStartVelocity);
      if (altTrig || velTrig) g_phase = AP_PHASE_GRAVITY_TURN;
      break;
    }

    case AP_PHASE_GRAVITY_TURN: {
      float pitch = apScheduledPitch();
      // Angle-of-attack limit: keep the commanded pitch within aoaLimit of surface
      // prograde. Aerodynamic guard only — skipped on airless bodies so the craft can
      // pitch over freely to build horizontal velocity.
      if (g_tel.hasAtmo && g_cfg.aoaLimit > 0.0f && g_tel.velSurface > 30.0f) {
        pitch = apClampf(pitch, g_tel.srfVelPitch - g_cfg.aoaLimit, g_tel.srfVelPitch + g_cfg.aoaLimit);
      }
      apSteer(pitch, azimuth, dt);
      apSendThrottle(apManagedThrottle());
      apMaybeStage();

      if (g_tel.apoapsis >= g_cfg.targetApoapsis) {
        apSendThrottle(0.0f);
        g_phase = g_cfg.circularize ? AP_PHASE_COAST : AP_PHASE_COMPLETE;
      }
      break;
    }

    case AP_PHASE_COAST: {
      apSendThrottle(0.0f);
      // Drag in the upper atmosphere can pull apoapsis back down — relight if so.
      if (g_tel.apoapsis < g_cfg.targetApoapsis * 0.995f && g_tel.airDensity > 1.0e-4f) {
        g_phase = AP_PHASE_GRAVITY_TURN;
        break;
      }
      // Prograde is held either by stock SAS (apReconcileSAS) or by us actively.
      if (!g_cfg.useStockSASForCoast) apSteerPrograde(dt);
      if (g_tel.timeToAp <= g_cfg.circStartLeadTime) g_phase = AP_PHASE_CIRCULARIZE;
      break;
    }

    case AP_PHASE_CIRCULARIZE: {
      if (!g_cfg.useStockSASForCoast) apSteerPrograde(dt);
      // Burn until periapsis reaches the target (circular at target altitude).
      if (g_tel.periapsis >= (g_cfg.targetApoapsis - g_cfg.circPeTolerance)) {
        apSendThrottle(0.0f);
        g_phase = AP_PHASE_COMPLETE;
      } else {
        apSendThrottle(apLimitG(g_cfg.launchThrottle));
      }
      break;
    }

    case AP_PHASE_COMPLETE: {
      apSendThrottle(0.0f);
      // Hand back to the pilot with stock SAS enabled in stability-hold (attitude hold).
      mySimpit.activateAction(SAS_ACTION);
      mySimpit.setSASMode(AP_STABILITYASSIST);
      g_armed = false;
      break;
    }

    default:
      apSendThrottle(0.0f);
      g_armed = false;
      break;
  }
}

/***************************************************************************************
   Optional bench-test console (primary Serial). Line-oriented, whitespace-separated.
   Mirrors the console-facing setters so a bench session exercises the same guarded path
   a panel would use:
     ARM | DISARM | STATUS
     ALT <meters> | INC <deg> | LOFT <x> | ROLL <deg> | ROLLOFF | MAXG <g> | SOUTH <0|1>
   Setters apply only while DISARMED and print "(armed - ignored)" otherwise.
****************************************************************************************/
void apSerialConsole() {
  static char buf[48];
  static uint8_t len = 0;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n' || len >= sizeof(buf) - 1) {
      buf[len] = '\0';
      len = 0;
      bool ok = true;
      if      (strncasecmp(buf, "ARM", 3) == 0)     apArm();
      else if (strncasecmp(buf, "DISARM", 6) == 0)  apDisarm();
      else if (strncasecmp(buf, "ALT ", 4) == 0)    ok = apSetTargetAltitude(atof(buf + 4));
      else if (strncasecmp(buf, "INC ", 4) == 0)    ok = apSetTargetInclination(atof(buf + 4));
      else if (strncasecmp(buf, "LOFT ", 5) == 0)   ok = apSetLoft(atof(buf + 5));
      else if (strncasecmp(buf, "ROLL ", 5) == 0)   ok = apSetRoll(true, atof(buf + 5));
      else if (strncasecmp(buf, "ROLLOFF", 7) == 0) ok = apSetRoll(false, g_cfg.targetRoll);
      else if (strncasecmp(buf, "MAXG ", 5) == 0)   ok = apSetMaxG(atof(buf + 5));
      else if (strncasecmp(buf, "SOUTH ", 6) == 0)  ok = apSetLaunchSoutherly(atoi(buf + 6) != 0);
      else if (strncasecmp(buf, "HP ", 3) == 0)     ok = hpConsoleLine(buf + 3);   // hold-mode autopilot (hold_autopilot.ino)
      else if (strncasecmp(buf, "BP ", 3) == 0)     ok = bpConsoleLine(buf + 3);   // burn autopilot (burn_autopilot.ino)
      else if (strncasecmp(buf, "LP ", 3) == 0)     ok = lpConsoleLine(buf + 3);   // landing autopilot (landing_autopilot.ino)
      else if (strncasecmp(buf, "STATUS", 6) == 0) {
        Serial.print(F("AP armed=")); Serial.print(g_armed);
        Serial.print(F(" phase="));   Serial.print(apPhaseName(g_phase));
        Serial.print(F(" body="));    Serial.print(g_tel.bodyName[0] ? g_tel.bodyName : "?");
        Serial.print(F(" tgtAp="));   Serial.print(g_cfg.targetApoapsis, 0);
        Serial.print(F(" Ap="));      Serial.print(g_tel.apoapsis, 0);
        Serial.print(F(" Pe="));      Serial.print(g_tel.periapsis, 0);
        Serial.print(F(" pitch="));   Serial.print(g_cmdPitch, 1);
        Serial.print(F(" hdg="));     Serial.print(g_cmdHeading, 1);
        Serial.print(F(" thr="));     Serial.print(g_cmdThrottle, 2);
        Serial.print(F(" g="));       Serial.println(g_tel.gForce, 1);
      }
      if (!ok) Serial.println(F("(armed - ignored)"));
    } else {
      buf[len++] = ch;
    }
  }
}

/***************************************************************************************
   Telemetry ingest
****************************************************************************************/
static inline void apStamp() { g_tel.lastTelemMs = millis(); }

void apIngestFlightStatus(bool inFlight)                { g_tel.inFlight = inFlight; apStamp(); }
void apIngestAltitude(float sealevel, float surface)    { g_tel.altSea = sealevel; g_tel.altSurface = surface; apStamp(); }
void apIngestVelocity(float orbital, float surface, float vertical) {
  g_tel.velOrbital = orbital; g_tel.velSurface = surface; g_tel.velVertical = vertical; apStamp();
}
void apIngestApsides(float apoapsis, float periapsis)   { g_tel.apoapsis = apoapsis; g_tel.periapsis = periapsis; apStamp(); }
void apIngestApsidesTime(float toAp, float toPe)        { g_tel.timeToAp = toAp; (void)toPe; apStamp(); }
void apIngestOrbit(float inclinationDeg)                { g_tel.inclination = inclinationDeg; apStamp(); }
void apIngestAttitude(float heading, float pitch, float roll,
                      float srfVelHeading, float srfVelPitch,
                      float orbVelHeading, float orbVelPitch) {
  g_tel.heading = heading; g_tel.pitch = pitch; g_tel.roll = roll;
  g_tel.srfVelHeading = srfVelHeading; g_tel.srfVelPitch = srfVelPitch;
  g_tel.orbVelHeading = orbVelHeading; g_tel.orbVelPitch = orbVelPitch; apStamp();
}
void apIngestAtmo(float airDensity, bool hasAtmosphere, bool inAtmosphere) {
  g_tel.airDensity = airDensity; g_tel.hasAtmo = hasAtmosphere; g_tel.inAtmo = inAtmosphere; apStamp();
}
void apIngestGForce(float gForce)                       { g_tel.gForce = gForce; apStamp(); }
void apIngestSkinTemp(float skinTempFraction)           { g_tel.skinTempFrac = skinTempFraction; apStamp(); }
void apIngestStageDeltaV(float stageDeltaV)             { g_tel.stageDV = stageDeltaV; apStamp(); }

void apIngestSOI(const char *bodyName) {
  if (!bodyName) return;
  // Only react on an actual body change so we do not repeatedly overwrite the config.
  if (strncmp(bodyName, g_tel.bodyName, sizeof(g_tel.bodyName)) == 0) return;
  strncpy(g_tel.bodyName, bodyName, sizeof(g_tel.bodyName) - 1);
  g_tel.bodyName[sizeof(g_tel.bodyName) - 1] = '\0';
  g_curBody = getBodyParams(g_tel.bodyName);   // shared celestial-body table
  // Adopt the body's default parking orbit unless the pilot has set an explicit target.
  if (g_cfg.autoBodyProfile && !g_targetLocked) {
    float def = apBodyDefaultOrbit();
    if (def > 0.0f) g_cfg.targetApoapsis = def;
  }
}
