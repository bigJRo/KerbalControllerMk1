/********************************************************************************************************************************
  Burn Autopilot — Kerbal Controller Mk1 (Master Teensy 4.1)

  The ORBITAL AUTOPILOT console's module: one burn executor and three planners (maneuver node, apsis change, plane change),
  plus the rendezvous approach-rate hold. Design: Documents/Developer/Mission_Autopilot.md §4, §7.1, §7.3.

  Burns are two-step (review decision q.3): ARM runs the planner and publishes the plan; EXEC starts the executor. Once
  aligned, with the WARP option on, a second EXEC warps to ignition minus the lead. The executor never warps unasked.

  Phases:  IDLE -arm-> PLANNED -EXEC-> ALIGN -aligned-> WARP READY -EXEC-> WARP -T-ign-> BURN -cut-> DONE
           any abort test -> ABORT (throttle 0, SAS stability assist)
  A plan that changes materially after EXEC drops back to PLANNED so nothing warps on a plan the pilot has not seen.

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Written for Jeb's Controller Works.
********************************************************************************************************************************/
#ifndef BURN_AUTOPILOT_H
#define BURN_AUTOPILOT_H

#include <Arduino.h>

enum BpMode  : uint8_t { BP_MODE_NONE = 0, BP_MODE_NODE, BP_MODE_AP, BP_MODE_PE, BP_MODE_INC };
enum BpPhase : uint8_t { BP_PHASE_IDLE = 0, BP_PHASE_PLANNED, BP_PHASE_ALIGN, BP_PHASE_WARP_READY,
                         BP_PHASE_WARP, BP_PHASE_BURN, BP_PHASE_DONE, BP_PHASE_ABORT };

struct BurnConfig {
  float    alignTolDeg;        // pointing error to call aligned
  float    alignSettleRate;    // deg/s — attitude rates below this ...
  uint32_t alignSettleMs;      // ... for this long (settle backstop)
  uint32_t alignTimeoutMs;     // not aligned within this -> ABORT (ALIGN)
  float    alignLeadS;         // warp target = ignition minus this
  float    taperS;             // taper the throttle over the last N seconds of delta-V
  float    throttleFloor;      // taper floor
  float    cutDv;              // m/s remaining at which the burn cuts
  float    minBurnS;           // overshoot test is armed after this long
  float    replanDvFrac;       // plan changed if delta-V moves by this fraction ...
  float    replanDvMin;        // ... or this many m/s, or
  float    replanTignS;        // ... ignition moves by this many seconds
  float    apprKa, apprKl;     // approach: along-LOS and lateral translation gains
  float    apprDeadband;       // m/s
  float    apprRateDivisor;    // rate setpoint limited to range / this
  float    apprAbortDivisor;   // closing faster than range / this -> disconnect
  float    trnSignX, trnSignY, trnSignZ;   // translation axis sign conventions (+1 / -1)
  uint32_t telemetryTimeout;
  float    apMin, apMax, peMin, peMax, incMin, incMax, apprRateMin, apprRateMax, apprDistMin, apprDistMax;
};

// A planner's output (in the header so the Arduino prototype pass sees the type).
struct BurnPlan {
  uint8_t sasMode;      // AP_MANEUVER / AP_PROGRADE / AP_RETROGRADE / AP_NORMAL / AP_ANTINORMAL
  float   dvTotal;      // m/s (signed for apsis: + prograde, - retrograde)
  float   tIgnition;    // s from now, includes the half-duration lead
  float   duration;     // s
  uint8_t warpInstant;  // TIMEWARP_TO_*
  float   warpDelay;    // s relative to the instant
  bool    valid;
};

struct BurnStatus {
  uint8_t mode, phase, reason, reasonAge;
  bool    armed, executing, autoWarp, autoStage, targetAvailable, nodeAvailable, apprEngaged;
  float   targetAp, targetPe, targetInc, apprRate, apprDist;
  float   dvTotal, dvRemaining, tIgnition, burnDuration, accelEst, cmdThrottle;
};

BurnConfig  bpDefaultConfig();
void        bpInit();
BurnConfig &bpGetConfig();

bool        bpArm(BpMode mode, bool on);           // plan (true = planned; false = refused, reason in status)
bool        bpExecute();                            // EXEC (from PLANNED) or WARP (from WARP READY)
void        bpAbort(uint8_t reason);                // any phase -> ABORT / disarm
bool        bpEngageApproach(bool on);
void        bpArbiterDrop();                        // another module took attitude or throttle

bool        bpSetTargetAp(float m);
bool        bpSetTargetPe(float m);
bool        bpSetTargetInc(float deg);
bool        bpSetApprRate(float mps);
bool        bpSetApprDist(float m);
void        bpSetAutoWarp(bool on);

bool        bpArmed();
bool        bpExecuting();                          // ALIGN .. BURN
bool        bpAnyEngaged();                         // armed, executing or approach hold
BurnStatus  bpGetStatus();
const char *bpPhaseName(uint8_t phase);
void        bpUpdate();
bool        bpConsoleLine(const char *line);        // "ARM NODE|AP|PE|INC", "EXEC", "APPR 1", "SET AP 250000", "OFF", "STATUS"

// Telemetry ingest
void bpIngestNode(float timeTo, float dv, float duration, float heading, float pitch);
void bpIngestOrbit(float ecc, float sma, float inc, float lan, float argPe, float trueAnom, float period);
void bpIngestApsides(float apoapsis, float periapsis);
void bpIngestApsidesTime(float toAp, float toPe);
void bpIngestVelocity(float orbital);
void bpIngestAttitude(float heading, float pitch, float roll, float orbVelHeading, float orbVelPitch);
void bpIngestTarget(bool available, float distance, float velocity, float heading, float pitch,
                    float velHeading, float velPitch);
void bpIngestBody(float radius, float gravity, const char *name);
void bpVesselChanged();

#endif  // BURN_AUTOPILOT_H
