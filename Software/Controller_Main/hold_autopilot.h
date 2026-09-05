/********************************************************************************************************************************
  Hold-Mode Autopilot — Kerbal Controller Mk1 (Master Teensy 4.1)

  Pilot-selectable hold modes for aircraft and rovers, engaged from the AIRCRAFT AUTOPILOT / ROVER AUTOPILOT consoles on
  Info Display 2 (or the bench serial console). Where the ascent autopilot flies a profile and hands back, a hold mode holds
  ONE quantity for as long as it is engaged. Design: Documents/Developer/Hold_Mode_Autopilot.md.

  Mode groups (modes within a group are mutually exclusive; groups combine freely):
    Aircraft pitch group   : ATT (pitch attitude) | AOA (angle of attack) | V/S (vertical speed) | ALT (altitude ASL)
    Aircraft lateral group : ROLL (bank) | HDG (heading, by banking)
    Aircraft thrust group  : IAS | MACH  (autothrottle -> game throttle + Throttle Module lever)
    Rover drive            : CRUISE (signed ground speed -> wheel throttle)
    Rover steer            : HDG | TGT (drive to target bearing)  -> wheel steering
    Rover guards           : SPEED / SLOPE / ROLL limits, always active with CRUISE

  Engaging any hold mode disarms the ascent autopilot; arming the ascent autopilot disconnects every hold mode.

  DESIGN NOTES
  - Self-contained telemetry snapshot fed by hpIngest*() from the Simpit message handler, like the ascent module.
  - Attitude goes through the shared attitude controller (attitude_controller.h) and out via rotation_link, which merges the
    held axes with the pilot's stick. Throttle goes via throttle_link so the motorised lever follows. Wheels go straight to
    Simpit (WHEEL_MESSAGE).
  - Every engage captures the current value as the setpoint (bumpless). Setpoints apply while engaged.

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Written for Jeb's Controller Works.
********************************************************************************************************************************/
#ifndef HOLD_AUTOPILOT_H
#define HOLD_AUTOPILOT_H

#include <Arduino.h>
#include "attitude_controller.h"

/***************************************************************************************
   Modes
****************************************************************************************/
enum HpPitchMode : uint8_t { HP_PITCH_OFF = 0, HP_PITCH_ATT, HP_PITCH_AOA, HP_PITCH_VS, HP_PITCH_ALT };
enum HpLatMode   : uint8_t { HP_LAT_OFF   = 0, HP_LAT_ROLL,  HP_LAT_HDG };
enum HpThrMode   : uint8_t { HP_THR_OFF   = 0, HP_THR_IAS,   HP_THR_MACH };

// Engage handle — one per mode button on the consoles.
enum HpMode : uint8_t {
  HP_MODE_ATT = 0, HP_MODE_AOA, HP_MODE_VS, HP_MODE_ALT,     // aircraft pitch group
  HP_MODE_ROLL, HP_MODE_HDG,                                 // aircraft lateral group
  HP_MODE_IAS, HP_MODE_MACH,                                 // aircraft thrust group
  HP_MODE_CRUISE, HP_MODE_RHDG, HP_MODE_RTGT,                // rover
  HP_MODE_COUNT
};

// Disconnect / refusal reasons (status byte values shared with the consoles).
enum HpReason : uint8_t {
  HP_REASON_NONE = 0, HP_REASON_STICK, HP_REASON_LEVER, HP_REASON_BRAKES, HP_REASON_AIRBORNE,
  HP_REASON_ROLL_LIMIT, HP_REASON_NO_ATMO, HP_REASON_TELEMETRY, HP_REASON_ASCENT, HP_REASON_REFUSED,
  HP_REASON_PILOT   // A/P OFF tap — not annunciated
};

/***************************************************************************************
   Tunable configuration
****************************************************************************************/
struct HoldConfig {
  // Attitude loop
  AttGains acftGains;              // aircraft class
  AttGains rocketGains;            // used when steerLikeRocket
  bool     steerLikeRocket;        // tail-sitters / VTOL / rockets in atmosphere: heading via yaw
  bool     coordinateTurn;         // small rudder term into the bank (aircraft entry)
  float    maxControlDeflection;   // 0..1 clamp on every axis

  // Outer-loop caps and gains
  float    vsMax;                  // m/s  V/S commanded by ALT capture (and the V/S setpoint cap)
  float    pitchMax;               // deg  pitch commanded by the V/S loop
  float    bankMax;                // deg  bank commanded by HDG
  float    altKp;                  // (m/s) per m  ALT -> V/S
  float    vsKp, vsKi;             // deg per (m/s), deg/s per (m/s)  V/S -> pitch
  float    hdgKp;                  // deg bank per deg heading error

  // Autothrottle
  float    iasKp, iasKi;           // throttle per (m/s), per (m/s) per s
  float    machKp, machKi;         // throttle per Mach, per Mach per s
  float    throttleSlew;           // throttle units per second

  // Rover
  float    cruiseKp, cruiseKi;     // wheel throttle per (m/s), per (m/s) per s
  float    wheelSlew;              // wheel-throttle units per second
  float    steerKpLow, steerKpHigh;// steer per deg heading error at 0 m/s and at steerKpSpeed
  float    steerKpSpeed;           // m/s at which steerKpHigh applies
  float    steerSign;              // +1 / -1: flip if the rover turns away from the setpoint

  // Disconnect rules (pilot-input override thresholds live in rotation_link.ino — the rule
  // is global: any stick, translation or lever input disconnects every autopilot)
  uint32_t telemetryTimeout;       // ms
  uint32_t airborneMs;             // rover: not landed/splashed for this long
  uint32_t noAtmoMs;               // aircraft: out of atmosphere for this long

  // Setpoint ranges (also enforced by the console)
  float attMin, attMax, aoaMin, aoaMax, vsMin, vsMaxSp, altMin, altMax, rollMin, rollMax;
  float iasMin, iasMax, machMin, machMax, cruiseMin, cruiseMax;
  float maxSpeedMin, maxSpeedMax, maxSlopeMin, maxSlopeMax, maxRollMin, maxRollMax;
};

/***************************************************************************************
   Status readout — everything the consoles render (Hold_Mode_Autopilot.md §8.2)
****************************************************************************************/
struct HoldStatus {
  // Aircraft
  uint8_t pitchMode, latMode, thrMode;   // HpPitchMode / HpLatMode / HpThrMode
  uint8_t reason;                        // HpReason of the last aircraft disconnect / refusal
  uint8_t reasonAge;                     // s since it was set, saturates at 255
  bool    anyEngaged, thrustEngaged, leverTouched, leverDriven, ascentArmed;
  float   att, aoa, vs, alt, roll, hdg, ias, mach;   // setpoint echoes
  float   cmdThrottle;                   // 0..1
  // Rover
  bool    cruise, rhdg, rtgt, brakes, slopeGuard, targetAvailable;
  uint8_t roverReason, roverReasonAge;
  float   cruiseSp, rhdgSp, maxSpeed, maxSlope, maxRoll;
  float   cmdWheel;                      // -1..1
};

/***************************************************************************************
   Public API
****************************************************************************************/
HoldConfig  hpDefaultConfig();
void        hpInit();
HoldConfig &hpGetConfig();

bool        hpEngage(HpMode mode, bool on);        // true if applied (false = refused; reason in status)
void        hpLevel();                              // LVL: ROLL 0 + V/S 0, thrust group untouched
void        hpDisconnectAll(uint8_t reason);        // aircraft + rover, SAS to stability assist
void        hpDisconnectAircraft(uint8_t reason);
void        hpDisconnectRover(uint8_t reason);

// Setpoints — apply engaged or not; return false only if out of range.
bool        hpSetAtt(float deg);
bool        hpSetAoa(float deg);
bool        hpSetVs(float mps);
bool        hpSetAlt(float m);
bool        hpSetRoll(float deg);
bool        hpSetHdg(float deg);
bool        hpSetIas(float mps);
bool        hpSetMach(float m);
bool        hpSetCruise(float mps);
bool        hpSetRoverHdg(float deg);
bool        hpSetMaxSpeed(float mps);
bool        hpSetMaxSlope(float deg);
bool        hpSetMaxRoll(float deg);

bool        hpAnyEngaged();
bool        hpAttitudeEngaged();                    // a pitch or lateral mode is holding the airframe
bool        hpThrustEngaged();
bool        hpRoverEngaged();
HoldStatus  hpGetStatus();
const char *hpModeName(HpMode m);
const char *hpReasonName(uint8_t r);

void        hpUpdate();                             // run the loops — call every loop()
bool        hpConsoleLine(const char *line);        // bench console: "ENG ALT", "SET IAS 180", "LVL", "OFF", "STATUS"

/***************************************************************************************
   Telemetry ingest — from the Simpit message handler
****************************************************************************************/
void hpIngestFlightStatus(uint8_t vesselType, uint8_t situation, bool hasTarget);
void hpIngestAltitude(float sealevel);
void hpIngestVelocity(float surface, float vertical);
void hpIngestAirspeed(float ias, float mach);
void hpIngestAttitude(float heading, float pitch, float roll, float srfVelHeading, float srfVelPitch);
void hpIngestAtmo(bool hasAtmosphere, bool inAtmosphere);
void hpIngestBrakes(bool on);
void hpIngestTarget(float bearingDeg);
void hpIngestThrottle(float t01);                   // game throttle echo (THROTTLE_CMD_MESSAGE)
void hpVesselChanged();                             // vessel / scene change: drop everything silently

#endif  // HOLD_AUTOPILOT_H
