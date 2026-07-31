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

// PID integrator state
static float g_iPitch = 0.0f, g_iYaw = 0.0f, g_iRoll = 0.0f;
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
  bool     hasAtmo       = true;   // current body has an atmosphere (from ATMO_CONDITIONS)
  bool     inAtmo        = true;   // vessel is currently within the atmosphere
  float    skinTempFrac  = 0.0f;   // 0..1
  float    stageDV       = 1.0e6f; // m/s (start high so we do not stage before data arrives)
  char     bodyName[24]  = {0};    // current sphere-of-influence body (from SOI_MESSAGE)
  uint32_t lastTelemMs   = 0;      // freshness stamp
};
static ApTelemetry g_tel;

/***************************************************************************************
   Body profiles — sensible per-body defaults for the stock KSP1 system, keyed on the
   SOI_MESSAGE body name. atmosphereTop is 0 for airless bodies. defaultOrbit and
   minSafeAltitude are convenience/terrain-clearance values — approximate; verify per
   mission. Unknown bodies fall back to telemetry-driven behaviour (see apLookupBody).
   Jool / the Sun are intentionally omitted (no launchable surface).
****************************************************************************************/
struct BodyProfile {
  const char *name;
  float atmosphereTop;    // m (0 = airless)
  float defaultOrbit;     // m — suggested parking orbit
  float minSafeAltitude;  // m — keep orbits above this for terrain clearance
};
static const BodyProfile AP_BODIES[] = {
  //  name          atmoTop   default   minSafe
  { "Kerbin",       70000.0f,  80000.0f,  71000.0f },
  { "Mun",              0.0f,  25000.0f,  12000.0f },
  { "Minmus",           0.0f,  15000.0f,   7000.0f },
  { "Duna",         50000.0f,  60000.0f,  51000.0f },
  { "Ike",              0.0f,  20000.0f,  13000.0f },
  { "Eve",          90000.0f, 100000.0f,  91000.0f },
  { "Gilly",            0.0f,  12000.0f,   7000.0f },
  { "Moho",             0.0f,  25000.0f,  12000.0f },
  { "Dres",             0.0f,  15000.0f,   6000.0f },
  { "Laythe",       50000.0f,  60000.0f,  51000.0f },
  { "Vall",             0.0f,  20000.0f,   8000.0f },
  { "Tylo",             0.0f,  20000.0f,  13000.0f },
  { "Bop",              0.0f,  30000.0f,  22000.0f },
  { "Pol",              0.0f,  12000.0f,   6000.0f },
  { "Eeloo",            0.0f,  15000.0f,   5000.0f },
};
static BodyProfile g_curBody = { "", 0.0f, 0.0f, 0.0f };  // resolved current-body profile
static bool        g_targetLocked = false;                // pilot has set an explicit target apoapsis

// Case-insensitive lookup; returns a fallback profile (atmoTop/defaultOrbit/minSafe = 0)
// for unknown bodies so guidance degrades to telemetry-driven behaviour.
static BodyProfile apLookupBody(const char *name) {
  for (uint8_t i = 0; i < (sizeof(AP_BODIES) / sizeof(AP_BODIES[0])); i++) {
    if (strcasecmp(name, AP_BODIES[i].name) == 0) return AP_BODIES[i];
  }
  BodyProfile unknown = { "", 0.0f, 0.0f, 0.0f };
  return unknown;
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
  if (g_tel.hasAtmo && g_curBody.atmosphereTop > 0.0f)
    return g_cfg.turnEndAtmoFraction * g_curBody.atmosphereTop;
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
   Throttle manager for powered flight: starts from launchThrottle and applies the
   max-Q, skin-temperature, and apoapsis-approach limits, taking the most restrictive.
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

  return apClampf(thr, 0.0f, 1.0f);
}

/***************************************************************************************
   Emit throttle (0..1) to KSP
****************************************************************************************/
static void apSendThrottle(float f) {
  f = apClampf(f, 0.0f, 1.0f);
  g_cmdThrottle = f;
  throttleMessage msg;
  msg.throttle = (int16_t)(f * (float)AP_AXIS_FULL);
  mySimpit.send(THROTTLE_MESSAGE, msg);
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

  // Errors in the navball frame (degrees)
  float ePitch = cmdPitch - g_tel.pitch;              // + means pitch up
  float eHead  = apWrap180(cmdHeading - g_tel.heading);

  // Rotate the pitch/heading error into the body frame using current roll.
  float phi = apDeg2Rad(g_tel.roll);
  float bodyPitchErr =  ePitch * cosf(phi) + eHead * sinf(phi);
  float bodyYawErr   = -ePitch * sinf(phi) + eHead * cosf(phi);

  // Normalise errors to roughly [-1, 1] over a 30 deg error, then PID.
  const float ERR_NORM = 1.0f / 30.0f;
  bodyPitchErr *= ERR_NORM;
  bodyYawErr   *= ERR_NORM;

  g_iPitch = apClampf(g_iPitch + bodyPitchErr * dt, -1.0f, 1.0f);
  g_iYaw   = apClampf(g_iYaw   + bodyYawErr   * dt, -1.0f, 1.0f);

  float pitchCmd = g_cfg.pitchKp * bodyPitchErr + g_cfg.pitchKi * g_iPitch;
  float yawCmd   = g_cfg.yawKp   * bodyYawErr   + g_cfg.yawKi   * g_iYaw;

  float rollCmd = 0.0f;
  if (g_cfg.rollControlEnabled) {
    float eRoll = apWrap180(g_cfg.targetRoll - g_tel.roll) * ERR_NORM;
    g_iRoll = apClampf(g_iRoll + eRoll * dt, -1.0f, 1.0f);
    rollCmd = g_cfg.rollKp * eRoll + g_cfg.rollKi * g_iRoll;
  }

  float lim = g_cfg.maxControlDeflection;
  pitchCmd = apClampf(pitchCmd, -lim, lim);
  yawCmd   = apClampf(yawCmd,   -lim, lim);
  rollCmd  = apClampf(rollCmd,  -lim, lim);

  rotationMessage msg;
  msg.setPitch((int16_t)(pitchCmd * (float)AP_AXIS_FULL));
  msg.setYaw((int16_t)(yawCmd  * (float)AP_AXIS_FULL));
  if (g_cfg.rollControlEnabled) msg.setRoll((int16_t)(rollCmd * (float)AP_AXIS_FULL));
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
  g_cfg.targetApoapsis    = apoapsisM;
  g_cfg.targetInclination = inclinationDeg;
  g_cfg.loft              = loft;
  g_targetLocked          = true;   // an explicit target survives auto body-profile changes
}

const char *apCurrentBody() { return g_tel.bodyName; }

void apArm() {
  g_armed          = true;
  g_phase          = AP_PHASE_VERTICAL;
  g_iPitch = g_iYaw = g_iRoll = 0.0f;
  g_sasIsOff       = false;   // force apReconcileSAS() to command SAS off on the first pass
  g_sasProgradeSet = false;
  g_lastStageMs    = millis();
  g_lastUpdateMs = millis();
  // Terrain-clearance guard: never target below the body's minimum safe altitude.
  if (g_cfg.enforceMinSafeAltitude && g_curBody.minSafeAltitude > 0.0f &&
      g_cfg.targetApoapsis < g_curBody.minSafeAltitude) {
    g_cfg.targetApoapsis = g_curBody.minSafeAltitude;
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
  // Zero the control axes so we do not leave the stick deflected.
  rotationMessage msg;
  msg.setPitch(0); msg.setYaw(0); msg.setRoll(0);
  mySimpit.send(ROTATION_MESSAGE, msg);
}

bool        apIsArmed()  { return g_armed; }
AscentPhase apGetPhase() { return g_phase; }

AscentStatus apGetStatus() {
  AscentStatus s;
  s.armed       = g_armed;
  s.phase       = g_phase;
  s.cmdPitch    = g_cmdPitch;
  s.cmdHeading  = g_cmdHeading;
  s.cmdThrottle = g_cmdThrottle;
  s.dynPressure = g_dynPressure;
  s.apoapsis    = g_tel.apoapsis;
  s.periapsis   = g_tel.periapsis;
  return s;
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

  // Keep stock SAS consistent with the current steering mode (off while we send raw
  // rotation; on + prograde while we defer to it during coast/circularization).
  apReconcileSAS();

  float azimuth = apLaunchAzimuth();
  g_cmdHeading  = azimuth;

  switch (g_phase) {

    case AP_PHASE_VERTICAL: {
      apSendThrottle(g_cfg.launchThrottle);
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
        apSendThrottle(g_cfg.launchThrottle);
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
   Optional bench-test console (primary Serial). Line-oriented, whitespace-separated:
     ARM | DISARM | STATUS
     ALT <meters> | INC <deg> | LOFT <x>
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
      if      (strncasecmp(buf, "ARM", 3) == 0)    apArm();
      else if (strncasecmp(buf, "DISARM", 6) == 0) apDisarm();
      else if (strncasecmp(buf, "ALT ", 4) == 0) { g_cfg.targetApoapsis = atof(buf + 4); g_targetLocked = true; }
      else if (strncasecmp(buf, "INC ", 4) == 0)   g_cfg.targetInclination = atof(buf + 4);
      else if (strncasecmp(buf, "LOFT ", 5) == 0)  g_cfg.loft              = atof(buf + 5);
      else if (strncasecmp(buf, "STATUS", 6) == 0) {
        Serial.print(F("AP armed=")); Serial.print(g_armed);
        Serial.print(F(" body="));    Serial.print(g_tel.bodyName[0] ? g_tel.bodyName : "?");
        Serial.print(F(" atmo="));    Serial.print(g_tel.hasAtmo);
        Serial.print(F(" tgtAp="));   Serial.print(g_cfg.targetApoapsis, 0);
        Serial.print(F(" phase="));   Serial.print((int)g_phase);
        Serial.print(F(" pitch="));   Serial.print(g_cmdPitch, 1);
        Serial.print(F(" hdg="));     Serial.print(g_cmdHeading, 1);
        Serial.print(F(" thr="));     Serial.print(g_cmdThrottle, 2);
        Serial.print(F(" Ap="));      Serial.print(g_tel.apoapsis, 0);
        Serial.print(F(" Pe="));      Serial.println(g_tel.periapsis, 0);
      }
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
void apIngestSkinTemp(float skinTempFraction)           { g_tel.skinTempFrac = skinTempFraction; apStamp(); }
void apIngestStageDeltaV(float stageDeltaV)             { g_tel.stageDV = stageDeltaV; apStamp(); }

void apIngestSOI(const char *bodyName) {
  if (!bodyName) return;
  // Only react on an actual body change so we do not repeatedly overwrite the config.
  if (strncmp(bodyName, g_tel.bodyName, sizeof(g_tel.bodyName)) == 0) return;
  strncpy(g_tel.bodyName, bodyName, sizeof(g_tel.bodyName) - 1);
  g_tel.bodyName[sizeof(g_tel.bodyName) - 1] = '\0';
  g_curBody = apLookupBody(g_tel.bodyName);
  // Adopt the body's default parking orbit unless the pilot has set an explicit target.
  if (g_cfg.autoBodyProfile && !g_targetLocked && g_curBody.defaultOrbit > 0.0f) {
    g_cfg.targetApoapsis = g_curBody.defaultOrbit;
  }
}
