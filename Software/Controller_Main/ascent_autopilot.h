/********************************************************************************************************************************
  Ascent Autopilot — Kerbal Controller Mk1 (Master Teensy 4.1)

  A self-contained guidance module that flies a launch-to-orbit profile through the master controller's KerbalSimpit link:
  vertical climb -> gravity turn (targeting a commanded inclination) -> coast to apoapsis -> circularization burn.

  DESIGN NOTES
  ------------
  - Self-contained: this module keeps its OWN telemetry snapshot (ApTelemetry), fed by the apIngest*() calls that the Simpit
    message handler invokes. It deliberately does NOT depend on the master's loose telemetry globals, so it compiles and runs
    independently of the in-progress Controller_Main integration.
  - Outgoing control: sends ROTATION_MESSAGE (pitch/yaw/roll) and THROTTLE_MESSAGE, plus STAGE_ACTION / SAS_ACTION and
    setSASMode() — all already proven in this firmware.
  - Steering assumes KSP1 + KerbalSimpit. Control-axis full-scale is +/-INT16_MAX; throttle full-scale is 0..INT16_MAX.

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Written for Jeb's Controller Works.
********************************************************************************************************************************/
#ifndef ASCENT_AUTOPILOT_H
#define ASCENT_AUTOPILOT_H

#include <Arduino.h>

/***************************************************************************************
   Flight-phase state machine
****************************************************************************************/
enum AscentPhase {
  AP_PHASE_IDLE = 0,     // Disarmed — pilot has control, module sends nothing
  AP_PHASE_VERTICAL,     // Straight-up climb until the turn-start trigger
  AP_PHASE_GRAVITY_TURN, // Pitch program + azimuth steering + managed throttle
  AP_PHASE_COAST,        // Engines off, coasting to apoapsis (optionally SAS prograde)
  AP_PHASE_CIRCULARIZE,  // Prograde burn near apoapsis to raise periapsis to target
  AP_PHASE_COMPLETE,     // Target orbit achieved — control handed back to pilot
  AP_PHASE_ABORT         // Failsafe tripped (e.g. telemetry loss) — throttle cut
};

/***************************************************************************************
   Tunable configuration — the pilot-facing surface.
   Fetch a mutable reference with apGetConfig() and edit before apArm(), or use the
   apSet*() convenience helpers. apDefaultConfig() supplies ~80 km equatorial LKO from
   KSC defaults.
****************************************************************************************/
struct AscentConfig {
  // --- Mission targets ---
  float    targetApoapsis;      // m    Desired orbit altitude (also the target periapsis for a circular orbit)
  float    targetInclination;   // deg  Desired inclination, 0..180
  bool     launchSoutherly;     // Use the supplementary (southerly/descending-node) azimuth solution
  float    launchLatitude;      // deg  Launch-site latitude for the azimuth calculation (KSC ~= 0)
  float    headingBias;         // deg  Manual trim added to the computed launch azimuth

  // --- Ascent / gravity-turn shape ---
  float    turnStartAltitude;   // m    Surface altitude at which the pitch-over begins
  float    turnStartVelocity;   // m/s  OR surface speed to begin the turn, whichever comes first (0 disables)
  float    turnEndAltitude;     // m    Surface altitude at which the pitch program reaches finalPitch
  float    loft;                // -    Pitch-schedule exponent (turn aggressiveness): <1 aggressive, >1 lofted/gentle
  float    initialPitchKick;    // deg  Immediate pitch-over applied at turn start to commit the turn
  float    finalPitch;          // deg  Pitch above horizon held until engine cutoff (0..15)

  // --- Throttle management ---
  float    launchThrottle;      // 0..1 Throttle for the powered ascent
  bool     autoLaunch;          // Fire STAGE_ACTION once on arm to ignite the first stage
  float    maxQ;                // Pa   Dynamic-pressure limit; throttle backs off above it (0 = disabled)
  float    maxQThrottleFloor;   // 0..1 Lowest throttle the max-Q limiter will command
  float    skinTempLimit;       // 0..1 Throttle back above this skin-temperature fraction (0 = disabled)
  float    apoTaperStart;       // 0..1 Begin throttle taper when apoapsis >= this fraction of target
  float    apoTaperFloor;       // 0..1 Minimum throttle during the apoapsis taper before cutoff

  // --- Steering / control authority ---
  float    aoaLimit;            // deg  Max angle between commanded pitch and surface prograde (0 = disabled)
  float    pitchKp, pitchKi, pitchKd;
  float    yawKp,   yawKi,   yawKd;
  float    rollKp,  rollKi,  rollKd;
  bool     rollControlEnabled;  // Actively hold targetRoll during ascent
  float    targetRoll;          // deg  Roll angle to hold when roll control is enabled
  float    maxControlDeflection;// 0..1 Clamp on commanded axis magnitude (control-authority / precision limit)

  // --- Staging ---
  bool     autoStage;           // Auto-fire STAGE_ACTION when the active stage is spent
  float    stageDVThreshold;    // m/s  Stage when stage delta-V drops below this
  uint32_t stageMinInterval;    // ms   Lockout between successive auto-stagings

  // --- Circularization ---
  bool     circularize;         // Perform a circularization burn at apoapsis after coast
  float    circStartLeadTime;   // s    Begin the circularization burn when time-to-apoapsis falls below this
  float    circPeTolerance;     // m    Stop the burn when |target - periapsis| is within this

  // --- Safety / behaviour ---
  uint32_t telemetryTimeout;    // ms   Cut throttle + disarm if no telemetry arrives for this long
  bool     useStockSASForCoast; // Use stock SAS prograde hold during coast instead of active steering
};

/***************************************************************************************
   Live status readout (for displays / logging)
****************************************************************************************/
struct AscentStatus {
  bool        armed;
  AscentPhase phase;
  float       cmdPitch;      // deg  Commanded pitch (above horizon)
  float       cmdHeading;    // deg  Commanded heading / launch azimuth
  float       cmdThrottle;   // 0..1 Commanded throttle
  float       dynPressure;   // Pa   Current dynamic pressure estimate
  float       apoapsis;      // m
  float       periapsis;     // m
};

/***************************************************************************************
   Public API
****************************************************************************************/
AscentConfig apDefaultConfig();            // Sane LKO defaults
void         apInit();                     // Initialise module state (call in setup)
AscentConfig &apGetConfig();               // Mutable reference to the active config
void         apSetConfig(const AscentConfig &cfg);

void         apSetTargets(float apoapsisM, float inclinationDeg, float loft);  // Quick mission set

void         apArm();                      // Engage the autopilot from the current state
void         apDisarm();                   // Hand control back to the pilot (zeroes commands)
bool         apIsArmed();
AscentPhase  apGetPhase();
AscentStatus apGetStatus();

void         apUpdate();                    // Run guidance + emit commands — call every loop()

// Optional bench-test console: parses simple commands on the primary Serial port
// (ARM, DISARM, ALT <m>, INC <deg>, LOFT <x>, STATUS). Call from loop() if desired.
void         apSerialConsole();

/***************************************************************************************
   Telemetry ingest — call these from the Simpit message handler as packets arrive.
   Each call refreshes the module's snapshot and stamps the freshness timestamp.
****************************************************************************************/
void apIngestFlightStatus(bool inFlight);
void apIngestAltitude(float sealevel, float surface);
void apIngestVelocity(float orbital, float surface, float vertical);
void apIngestApsides(float apoapsis, float periapsis);
void apIngestApsidesTime(float timeToApoapsis, float timeToPeriapsis);
void apIngestOrbit(float inclinationDeg);
void apIngestAttitude(float heading, float pitch, float roll,
                      float srfVelHeading, float srfVelPitch,
                      float orbVelHeading, float orbVelPitch);
void apIngestAtmo(float airDensity);
void apIngestSkinTemp(float skinTempFraction);   // 0..1 (skinTempLimitPercentage / 100)
void apIngestStageDeltaV(float stageDeltaV);

#endif  // ASCENT_AUTOPILOT_H
