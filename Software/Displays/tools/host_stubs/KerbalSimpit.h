// Host stand-in for the KerbalSimpit Arduino library (Simpit-team/KerbalSimpitRevamped-
// Arduino): the message ids, flags, payload structs and client surface the KCMk1
// panels use, with the library's real values so a wrong id fails here too. Not the
// library; the panels' build machine has the real one.
#pragma once
#include <Arduino.h>

enum OutboundPackets : byte {
  SCENE_CHANGE_MESSAGE = 3, ALTITUDE_MESSAGE = 8, APSIDES_MESSAGE = 9,
  LF_MESSAGE = 10, LF_STAGE_MESSAGE = 11, OX_MESSAGE = 12, OX_STAGE_MESSAGE = 13,
  SF_MESSAGE = 14, SF_STAGE_MESSAGE = 15, MONO_MESSAGE = 16, ELECTRIC_MESSAGE = 17,
  EVA_MESSAGE = 18, ORE_MESSAGE = 19, AB_MESSAGE = 20, AB_STAGE_MESSAGE = 21,
  VELOCITY_MESSAGE = 22, APSIDESTIME_MESSAGE = 24, TARGETINFO_MESSAGE = 25, SOI_MESSAGE = 26,
  AIRSPEED_MESSAGE = 27, XENON_GAS_MESSAGE = 28, XENON_GAS_STAGE_MESSAGE = 29,
  TACLS_RESOURCE_MESSAGE = 30, TACLS_WASTE_MESSAGE = 31,
  CUSTOM_RESOURCE_1_MESSAGE = 32, CUSTOM_RESOURCE_2_MESSAGE = 33,
  MANEUVER_MESSAGE = 34, SAS_MODE_INFO_MESSAGE = 35, ORBIT_MESSAGE = 36, ACTIONSTATUS_MESSAGE = 37,
  DELTAV_MESSAGE = 38, DELTAVENV_MESSAGE = 39, BURNTIME_MESSAGE = 40, CAGSTATUS_MESSAGE = 41,
  TEMP_LIMIT_MESSAGE = 42, FLIGHT_STATUS_MESSAGE = 43, ATMO_CONDITIONS_MESSAGE = 44,
  ROTATION_DATA_MESSAGE = 45, VESSEL_NAME_MESSAGE = 46, ROTATION_CMD_MESSAGE = 47,
  TRANSLATION_CMD_MESSAGE = 48, WHEEL_CMD_MESSAGE = 49, THROTTLE_CMD_MESSAGE = 50,
  VESSEL_CHANGE_MESSAGE = 51, INTAKE_AIR_MESSAGE = 52, HYDROGEN_MESSAGE = 53,
  HYDROGEN_STAGE_MESSAGE = 54, URANIUM_MESSAGE = 55, ADVANCED_ACTIONSTATUS_MESSAGE = 56,
  ADVANCED_CAGSTATUS_MESSAGE = 57, INTERSECTS_MESSAGE = 60
};
enum FligthStatusFlags : byte {
  FLIGHT_IN_FLIGHT = 1, FLIGHT_IS_EVA = 2, FLIGHT_IS_RECOVERABLE = 4, FLIGHT_IS_ATMO_TW = 8,
  FLIGHT_CONTROL_LEVEL_0 = 16, FLIGHT_CONTROL_LEVEL_1 = 32, FLIGHT_HAS_TARGET = 64
};
enum AtmoConditionsFlags : byte { HAS_ATMOSPHERE = 1, HAS_OXYGEN = 2, IS_IN_ATMOSPHERE = 4 };
enum ActionGroupIndexes : byte {
  STAGE_ACTION = 1, GEAR_ACTION = 2, LIGHT_ACTION = 4, RCS_ACTION = 8, SAS_ACTION = 16,
  BRAKES_ACTION = 32, ABORT_ACTION = 64
};
enum AutopilotMode : byte {
  AP_STABILITYASSIST = 0, AP_PROGRADE, AP_RETROGRADE, AP_NORMAL, AP_ANTINORMAL, AP_RADIALIN,
  AP_RADIALOUT, AP_TARGET, AP_ANTITARGET, AP_MANEUVER, AP_NAVIGATION, AP_AUTOPILOT
};

struct resourceMessage        { float total; float available; };
struct TACLSResourceMessage   { float currentFood, maxFood, currentWater, maxWater, currentOxygen, maxOxygen; };
struct TACLSWasteMessage      { float currentWaste, maxWaste, currentLiquidWaste, maxLiquidWaste, currentCO2, maxCO2; };
struct CustomResourceMessage  { float currentResource1, maxResource1, currentResource2, maxResource2,
                                      currentResource3, maxResource3, currentResource4, maxResource4; };
struct altitudeMessage        { float sealevel; float surface; };
struct apsidesMessage         { float periapsis; float apoapsis; };
struct apsidesTimeMessage     { int32_t periapsis; int32_t apoapsis; };
struct orbitInfoMessage       { float eccentricity, semiMajorAxis, inclination, longAscendingNode,
                                      argPeriapsis, trueAnomaly, meanAnomaly, period; };
struct flightStatusMessage {
  byte flightStatusFlags, vesselSituation, currentTWIndex, crewCapacity,
       crewCount, commNetSignalStrenghPercentage, currentStage, vesselType;
  bool isInFlight()      { return flightStatusFlags & FLIGHT_IN_FLIGHT; }
  bool isInEVA()         { return flightStatusFlags & FLIGHT_IS_EVA; }
  bool isRecoverable()   { return flightStatusFlags & FLIGHT_IS_RECOVERABLE; }
  bool isInAtmoTW()      { return flightStatusFlags & FLIGHT_IS_ATMO_TW; }
  byte getControlLevel() { return (flightStatusFlags >> 4) & 3; }
  bool hasTarget()       { return flightStatusFlags & FLIGHT_HAS_TARGET; }
};
struct atmoConditionsMessage {
  byte atmoCharacteristics; float airDensity, temperature, pressure;
  bool hasAtmosphere()        { return atmoCharacteristics & HAS_ATMOSPHERE; }
  bool hasOxygen()            { return atmoCharacteristics & HAS_OXYGEN; }
  bool isVesselInAtmosphere() { return atmoCharacteristics & IS_IN_ATMOSPHERE; }
};
struct velocityMessage        { float orbital, surface, vertical; };
struct targetMessage          { float distance, velocity, heading, pitch, velocityHeading, velocityPitch; };
struct intersectsMessage      { float distanceAtIntersect1; int32_t timeToIntersect1; float velocityAtIntersect1;
                                float distanceAtIntersect2; int32_t timeToIntersect2; float velocityAtIntersect2; };
struct airspeedMessage        { float IAS, mach, gForces; };
struct maneuverMessage        { float timeToNextManeuver, deltaVNextManeuver, durationNextManeuver,
                                      deltaVTotal, headingNextManeuver, pitchNextManeuver; };
struct vesselPointingMessage  { float heading, pitch, roll, orbitalVelocityHeading, orbitalVelocityPitch,
                                      surfaceVelocityHeading, surfaceVelocityPitch; };
struct deltaVMessage          { float stageDeltaV, totalDeltaV; };
struct deltaVEnvMessage       { float stageDeltaVASL, totalDeltaVASL, stageDeltaVVac, totalDeltaVVac; };
struct burnTimeMessage        { float stageBurnTime, totalBurnTime; };
struct tempLimitMessage       { byte tempLimitPercentage, skinTempLimitPercentage; };
struct wheelMessage           { int16_t steer, throttle; byte mask; };
struct throttleMessage        { int16_t throttle; };
struct SASInfoMessage         { byte currentSASMode; int16_t SASModeAvailability; };
struct cagStatusMessage       { byte status[32]; bool is_action_activated(byte i) { return (status[i / 8] >> (i % 8)) & 1; } };

template <class T> T parseMessage(byte *msg) { T t; memcpy(&t, msg, sizeof(T)); return t; }
inline resourceMessage  parseResource(byte *m)        { return parseMessage<resourceMessage>(m); }
inline SASInfoMessage   parseSASInfoMessage(byte *m)  { return parseMessage<SASInfoMessage>(m); }
inline cagStatusMessage parseCAGStatusMessage(byte *m){ return parseMessage<cagStatusMessage>(m); }

class KerbalSimpit {
 public:
  KerbalSimpit(Stream &) {}
  bool init() { return true; }
  void update() {}
  void inboundHandler(void (*)(byte, byte *, byte)) {}
  void registerChannel(byte) {}
  void deregisterChannel(byte) {}
  void requestMessageOnChannel(byte) {}
  void printToKSP(String, byte = 0) {}
  template <class T> void send(byte, T &) {}
};
