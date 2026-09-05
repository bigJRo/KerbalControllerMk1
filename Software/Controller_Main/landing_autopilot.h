/********************************************************************************************************************************
  Landing Autopilot — Kerbal Controller Mk1 (Master Teensy 4.1)

  The LANDING AUTOPILOT console's module: DESC (vertical-speed hold on the throttle), HOVR (radar-altitude hold cascaded
  through DESC), BRAKE (armed suicide burn at a computed ignition altitude, handing off to DESC) and ENTRY (angle of attack
  against orbital retrograde plus roll, handing off by vessel type). Design: Documents/Developer/Mission_Autopilot.md §5, §7.2.

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Written for Jeb's Controller Works.
********************************************************************************************************************************/
#ifndef LANDING_AUTOPILOT_H
#define LANDING_AUTOPILOT_H

#include <Arduino.h>
#include "attitude_controller.h"

enum LpMode : uint8_t { LP_MODE_OFF = 0, LP_MODE_DESC, LP_MODE_HOVR, LP_MODE_BRAKE };

struct LandingConfig {
  AttGains entryGains;
  float    descKp, descKi, descSlew;       // throttle per (m/s), per (m/s) per s, units/s
  float    hovrKp, hovrVsCap;              // (m/s) per m, m/s
  float    radialSwitchSpeed;              // m/s horizontal below which retrograde -> radial-out
  float    brakeFactorEst, brakeFactorMeas, brakeFactorTwr;   // safety factor by acceleration source
  float    brakeLatencyS;                  // |vs| * this added to the ignition altitude
  float    brakeMinAccelG;                 // refuse to arm below this many g
  float    brakeMarginalFrac;              // IGN ALT orange when the descent needs more than this of a
  float    entryHandoffSpeed;              // m/s surface speed (rockets)
  float    entryPlaneMach, entryPlaneQ;    // planes: below this Mach and above this q (Pa) ...
  float    maxControlDeflection;
  uint32_t telemetryTimeout;
  float    descMin, descMax, hovrMin, hovrMax, twrMin, twrMax, marginMin, marginMax, aoaMin, aoaMax, rollMin, rollMax;
};

struct LandingStatus {
  uint8_t mode;                 // LpMode
  bool    entry, engaged, brakeArmed, brakeFiring, attRefRadial, autoStage, landed, brakeMarginal;
  uint8_t reason, reasonAge, accelSource;
  float   descRate, hovrAlt, twrOverride, margin, entryAoa, entryRoll, ignitionAlt, accelEst, cmdThrottle;
};

LandingConfig  lpDefaultConfig();
void           lpInit();
LandingConfig &lpGetConfig();

bool  lpEngage(uint8_t mode, bool on);        // LP_MODE_DESC / HOVR / BRAKE
bool  lpEngageEntry(bool on);
void  lpDisconnectAll(uint8_t reason);
void  lpArbiterDropAttitude();
void  lpArbiterDropThrottle();

bool  lpSetDescRate(float mps);
bool  lpSetHovrAlt(float m);
bool  lpSetTwr(float twr);                    // 0 = measured / estimated
bool  lpSetMargin(float m);
bool  lpSetEntryAoa(float deg);
bool  lpSetEntryRoll(float deg);
void  lpSetAttRef(bool radial);

bool  lpAnyEngaged();
LandingStatus lpGetStatus();
void  lpUpdate();
bool  lpConsoleLine(const char *line);        // "ENG DESC|HOVR|BRAKE|ENTRY [0|1]", "SET RATE -4", "OFF", "STATUS"

// Telemetry ingest
void lpIngestFlightStatus(uint8_t vesselType, uint8_t situation);
void lpIngestAltitude(float sealevel, float surface);
void lpIngestVelocity(float surface, float vertical);
void lpIngestAirspeed(float mach);
void lpIngestAttitude(float heading, float pitch, float roll, float orbVelHeading, float orbVelPitch);
void lpIngestAtmo(float airDensity, bool inAtmosphere);
void lpIngestBody(float gravity, float flyHigh, const char *name);
void lpIngestThrottle(float t01);
void lpVesselChanged();

#endif  // LANDING_AUTOPILOT_H
