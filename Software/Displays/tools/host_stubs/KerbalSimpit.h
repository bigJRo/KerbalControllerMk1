// Host stand-in for KerbalSimpit 2.x: the message ids, payload structs and the client
// surface the KCMk1 display sketches use. Ids are placeholders; only names matter here.
#pragma once
#include "Arduino.h"
enum SimpitMessageIds : byte {
  SCENE_CHANGE_MESSAGE = 1, VESSEL_CHANGE_MESSAGE, VESSEL_NAME_MESSAGE, FLIGHT_STATUS_MESSAGE,
  LF_MESSAGE, LF_STAGE_MESSAGE, OX_MESSAGE, OX_STAGE_MESSAGE, SF_MESSAGE, SF_STAGE_MESSAGE,
  XENON_GAS_MESSAGE, XENON_GAS_STAGE_MESSAGE, MONO_MESSAGE, ELECTRIC_MESSAGE, EVA_MESSAGE,
  ORE_MESSAGE, AB_MESSAGE, AB_STAGE_MESSAGE, TACLS_RESOURCE_MESSAGE, TACLS_WASTE_MESSAGE,
  CUSTOM_RESOURCE_1_MESSAGE, CUSTOM_RESOURCE_2_MESSAGE,
  ALTITUDE_MESSAGE, VELOCITY_MESSAGE, APSIDES_MESSAGE, APSIDES_TIME_MESSAGE, ROTATION_DATA_MESSAGE,
  ORBIT_INFO, AIRSPEED_MESSAGE, DELTAV_MESSAGE, DELTAVENV_MESSAGE, BURNTIME_MESSAGE, TEMP_LIMIT_MESSAGE,
  ATMO_CONDITIONS_MESSAGE, SOI_MESSAGE, MANEUVER_MESSAGE, TARGETINFO_MESSAGE, ACTIONSTATUS_MESSAGE,
  SAS_MODE_INFO_MESSAGE, ADVANCED_ACTIONSTATUS_MESSAGE, CAGSTATUS_MESSAGE
};
enum FlightStatusFlags : byte {
  FLIGHT_IN_FLIGHT = 1, FLIGHT_IS_EVA = 2, FLIGHT_IS_RECOVERABLE = 4, FLIGHT_IS_IN_ATMOSPHERE = 8,
  FLIGHT_CONTROL_LEVEL0 = 16, FLIGHT_CONTROL_LEVEL1 = 32, FLIGHT_HAS_TARGET = 64
};
struct resourceMessage { float total; float available; };
struct TACLSResourceMessage { float currentFood, maxFood, currentWater, maxWater, currentOxygen, maxOxygen; };
struct TACLSWasteMessage { float currentWaste, maxWaste, currentLiquidWaste, maxLiquidWaste, currentCO2, maxCO2; };
struct CustomResourceMessage { float currentResource1, maxResource1, currentResource2, maxResource2,
                               currentResource3, maxResource3, currentResource4, maxResource4; };
struct flightStatusMessage { byte flightStatusFlags, vesselSituation, currentTWIndex, crewCapacity,
                             crewCount, commNetSignalStrenghPercentage, currentStage, vesselType; };
template <class T> T parseMessage(byte *msg) { T t; memcpy(&t, msg, sizeof(T)); return t; }
class KerbalSimpit {
 public:
  KerbalSimpit(HostSerial &) {}
  bool init() { return true; }
  void update() {}
  void inboundHandler(void (*)(byte, byte *, byte)) {}
  void registerChannel(byte) {}
  void deregisterChannel(byte) {}
  void requestMessageOnChannel(byte) {}
  void printToKSP(String, int = 0) {}
};
