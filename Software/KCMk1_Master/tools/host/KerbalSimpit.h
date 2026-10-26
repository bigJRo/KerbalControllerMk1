// Host stand-in for the KerbalSimpit Arduino library, for the KCMk1_Master host build
// and selftest. The ids, flags and payload structs are the displays' stand-in's
// (Software/Displays/tools/host_stubs/KerbalSimpit.h, the library's real values); the
// client here RECORDS what the master sends, so a test can check what reached "KSP".
// host_build.py --simpit compiles the master against the real library instead.
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
// Packets to KSP (the master's commands). Separate enum in the library: the ids overlap.
enum InboundPackets : byte {
  CAGACTIVATE_MESSAGE = 10, CAGDEACTIVATE_MESSAGE = 11, CAGTOGGLE_MESSAGE = 12,
  AGACTIVATE_MESSAGE = 13, AGDEACTIVATE_MESSAGE = 14, AGTOGGLE_MESSAGE = 15,
  ROTATION_MESSAGE = 16, TRANSLATION_MESSAGE = 17, WHEEL_MESSAGE = 18, THROTTLE_MESSAGE = 19,
  SAS_MODE_MESSAGE = 20, CAMERA_CONTROL_MODE = 21, CAMERA_ROTATION_MESSAGE = 22,
  TIMEWARP_MESSAGE = 24, CUSTOM_LOG = 25, KEYBOARD_EMULATOR = 26, CUSTOM_AXIS_MESSAGE = 27,
  NAVBALLMODE_MESSAGE = 28, TIMEWARP_TO_MESSAGE = 30, SETSINGLE_AG_MESSAGE = 58,
  SETSINGLE_CAG_MESSAGE = 59
};
enum TimewarpTo {
  TIMEWARP_TO_NOW = 0, TIMEWARP_TO_NEXT_MANEUVER = 1, TIMEWARP_TO_NEXT_BURN = 2,
  TIMEWARP_TO_NEXT_SOI = 3, TIMEWARP_TO_APOAPSIS = 4, TIMEWARP_TO_PERIAPSIS = 5,
  TIMEWARP_TO_NEXT_MORNING = 6
};
enum CustomLogStatus { VERBOSE_ONLY = 1, PRINT_TO_SCREEN = 2, NO_HEADER = 4 };
enum KeyboardEmulatorModifier { SHIFT_MOD = 1, CTRL_MOD = 2, ALT_MOD = 4, KEY_DOWN_MOD = 8, KEY_UP_MOD = 16 };
enum RotationAxes { PITCH_ROT = 1, ROLL_ROT = 2, YAW_ROT = 4, ZOOM_ROT = 8 };
enum TranslationAxes { X_TRANS = 1, Y_TRANS = 2, Z_TRANS = 4 };
enum WheelAxes { STEER_WHEEL = 1, THROTTLE_WHEEL = 2 };
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
struct throttleMessage        { int16_t throttle; };
struct SASInfoMessage         { byte currentSASMode; int16_t SASModeAvailability; } __attribute__((packed));
struct cagStatusMessage       { byte status[32]; bool is_action_activated(byte i) { return (status[i / 8] >> (i % 8)) & 1; } };

// Command payloads, with the library's constructors (no default constructor where it has none).
struct rotationMessage {
  int16_t pitch = 0, roll = 0, yaw = 0; byte mask = 0;
  void setPitch(int16_t v) { pitch = v; mask |= PITCH_ROT; }
  void setRoll(int16_t v)  { roll = v;  mask |= ROLL_ROT; }
  void setYaw(int16_t v)   { yaw = v;   mask |= YAW_ROT; }
  void setPitchRollYaw(int16_t p, int16_t r, int16_t y) { setPitch(p); setRoll(r); setYaw(y); }
} __attribute__((packed));
struct translationMessage {
  int16_t X = 0, Y = 0, Z = 0; byte mask = 0;
  void setX(int16_t v) { X = v; mask |= X_TRANS; }
  void setY(int16_t v) { Y = v; mask |= Y_TRANS; }
  void setZ(int16_t v) { Z = v; mask |= Z_TRANS; }
  void setXYZ(int16_t x, int16_t y, int16_t z) { setX(x); setY(y); setZ(z); }
} __attribute__((packed));
struct wheelMessage {
  int16_t steer = 0, throttle = 0; byte mask = 0;
  void setSteer(int16_t v)    { steer = v;    mask |= STEER_WHEEL; }
  void setThrottle(int16_t v) { throttle = v; mask |= THROTTLE_WHEEL; }
  void setSteerThrottle(int16_t s, int16_t t) { setSteer(s); setThrottle(t); }
} __attribute__((packed));
struct timewarpToMessage {
  byte instant; float delay;
  timewarpToMessage(byte i, float d) : instant(i), delay(d) {}
} __attribute__((packed));
struct keyboardEmulatorMessage {
  byte modifier; int16_t keyCode;
  keyboardEmulatorMessage(int16_t k) : modifier(0), keyCode(k) {}
  keyboardEmulatorMessage(int16_t k, byte m) : modifier(m), keyCode(k) {}
} __attribute__((packed));

template <class T> T parseMessage(byte *msg) { T t; memcpy(&t, msg, sizeof(T)); return t; }
inline resourceMessage  parseResource(byte *m)        { return parseMessage<resourceMessage>(m); }
inline SASInfoMessage   parseSASInfoMessage(byte *m)  { return parseMessage<SASInfoMessage>(m); }
inline cagStatusMessage parseCAGStatusMessage(byte *m){ return parseMessage<cagStatusMessage>(m); }

// Host harness hooks: whether init() finds KSP, and a record of everything sent.
struct SimpitSent { byte type; std::vector<uint8_t> b; uint32_t ms; };
struct StubSimpit {
  bool kspUp = true; int initCalls = 0, updateCalls = 0;
  std::vector<byte> registered;
  void (*handler)(byte, byte *, byte) = nullptr;
  std::vector<SimpitSent> sent;
  void (*onInit)() = nullptr;                 // test hook: runs at each init() call
};
inline StubSimpit &stubSimpit() { static StubSimpit s; return s; }

class KerbalSimpit {
 public:
  KerbalSimpit(Stream &) {}
  bool init() { StubSimpit &s = stubSimpit(); s.initCalls++; if (s.onInit) s.onInit(); return s.kspUp; }
  void update() { stubSimpit().updateCalls++; }
  void inboundHandler(void (*h)(byte, byte *, byte)) { stubSimpit().handler = h; }
  void registerChannel(byte c) { stubSimpit().registered.push_back(c); }
  void deregisterChannel(byte) {}
  void requestMessageOnChannel(byte) {}
  void printToKSP(String, byte = 0) {}
  template <class T> void send(byte t, T &m) { record(t, (const uint8_t *)&m, sizeof(T)); }
  template <class T> void send(byte t, T &m, byte n) { record(t, (const uint8_t *)&m, n); }
  void activateCAG(byte g)   { uint8_t b = g; record(10, &b, 1); }
  void deactivateCAG(byte g) { uint8_t b = g; record(11, &b, 1); }
  void toggleCAG(byte g)     { uint8_t b = g; record(12, &b, 1); }
  void activateAction(byte a)   { uint8_t b = a; record(13, &b, 1); }
  void deactivateAction(byte a) { uint8_t b = a; record(14, &b, 1); }
  void toggleAction(byte a)     { uint8_t b = a; record(15, &b, 1); }
  void setSASMode(byte m)    { uint8_t b = m; record(SAS_MODE_MESSAGE, &b, 1); }
  void setCameraMode(byte m) { uint8_t b = m; record(CAMERA_CONTROL_MODE, &b, 1); }
  void cycleNavBallMode()    { record(NAVBALLMODE_MESSAGE, nullptr, 0); }
 private:
  void record(byte t, const uint8_t *p, size_t n) {
    stubSimpit().sent.push_back({t, std::vector<uint8_t>(p, p + n), millis()});
  }
};
