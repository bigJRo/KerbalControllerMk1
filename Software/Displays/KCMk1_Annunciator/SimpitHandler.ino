/***************************************************************************************
   SimpitHandler.ino -- KerbalSimpit message handling for Kerbal Controller Mk1 Annunciator

   Application-specific -- every display type subscribes to different channels and
   maps messages to its own AppState differently, so this lives as a sketch tab
   rather than in the library.

   Channels subscribed:
     VESSEL_NAME, SOI, FLIGHT_STATUS, TEMP_LIMIT, ACTIONSTATUS,
     ALTITUDE, VELOCITY, AIRSPEED, APSIDES, DELTAV, BURNTIME,
     ATMO_CONDITIONS, ELECTRIC, THROTTLE_CMD,
     SCENE_CHANGE, VESSEL_CHANGE
****************************************************************************************/
#include "KCMk1_Annunciator.h"



/***************************************************************************************
   SIMPIT MESSAGE HANDLER
   Called by simpit.update() whenever a subscribed message arrives.
   Populates state and logic globals. Does NOT touch prev or draw anything.
   Calls updateCautionWarningState() after any message that affects C&W inputs.
****************************************************************************************/
void onSimpitMessage(byte messageType, byte msg[], byte msgSize) {

  // Debug: log every incoming message by name so the Serial Monitor is readable.
  if (debugMode) {
    const char *msgName;
    switch (messageType) {
      case VESSEL_NAME_MESSAGE:     msgName = "VESSEL_NAME";     break;
      case SOI_MESSAGE:             msgName = "SOI";             break;
      case FLIGHT_STATUS_MESSAGE:   msgName = "FLIGHT_STATUS";   break;
      case TEMP_LIMIT_MESSAGE:      msgName = "TEMP_LIMIT";      break;
      case ACTIONSTATUS_MESSAGE:    msgName = "ACTION_STATUS";   break;
      case ALTITUDE_MESSAGE:        msgName = "ALTITUDE";        break;
      case VELOCITY_MESSAGE:        msgName = "VELOCITY";        break;
      case AIRSPEED_MESSAGE:        msgName = "AIRSPEED";        break;
      case APSIDES_MESSAGE:         msgName = "APSIDES";         break;
      case DELTAV_MESSAGE:          msgName = "DELTAV";          break;
      case BURNTIME_MESSAGE:        msgName = "BURNTIME";        break;
      case ATMO_CONDITIONS_MESSAGE: msgName = "ATMO_CONDITIONS"; break;
      case ELECTRIC_MESSAGE:        msgName = "ELECTRIC";        break;
      case THROTTLE_CMD_MESSAGE:    msgName = "THROTTLE_CMD";    break;
      case SCENE_CHANGE_MESSAGE:    msgName = "SCENE_CHANGE";    break;
      case VESSEL_CHANGE_MESSAGE:   msgName = "VESSEL_CHANGE";   break;
      default:                      msgName = "UNKNOWN";         break;
    }
    Serial.print(F("Annunciator: Simpit msg="));
    Serial.println(msgName);
  }

  switch (messageType) {

    case VESSEL_NAME_MESSAGE:
      state.vesselName = "";
      for (uint8_t i = 0; i < msgSize; i++) state.vesselName += char(msg[i]);
      break;

    case SOI_MESSAGE:
      state.gameSOI = "";
      for (uint8_t i = 0; i < msgSize; i++) state.gameSOI += char(msg[i]);
      currentBody = getBodyParams(state.gameSOI);
      break;

    case FLIGHT_STATUS_MESSAGE:
      if (msgSize == sizeof(flightStatusMessage)) {
        flightStatusMessage fs = parseMessage<flightStatusMessage>(msg);
        uint8_t sit = fs.vesselSituation;
        rawSituation = sit;

        // Map Simpit raw situation bits to display bitmask
        // Display: bit0=DOCKED, bit1=PRE-LAUNCH, bit2=FLIGHT, bit3=SUB-ORBIT,
        //          bit4=ORBIT,  bit5=ESCAPE,     bit6=SPLASH
        uint8_t dispSit = state.vesselSituationState & 0x01;  // preserve DOCKED bit
        if (sit & sit_PreLaunch) bitSet(dispSit, VSIT_PRELAUNCH);
        if (sit & sit_Flying)    bitSet(dispSit, VSIT_FLIGHT);
        if (sit & sit_SubOrb)    bitSet(dispSit, VSIT_SUBORBIT);
        if (sit & sit_Orbit)     bitSet(dispSit, VSIT_ORBIT);
        if (sit & sit_Escaping)  bitSet(dispSit, VSIT_ESCAPE);
        if (sit & sit_Splashed)  bitSet(dispSit, VSIT_SPLASH);
        state.vesselSituationState = dispSit;

        state.vesselType  = (VesselType)fs.vesselType;
        state.crewCount   = fs.crewCount;
        state.commNet     = fs.commNetSignalStrenghPercentage;
        state.stage       = fs.currentStage;
        state.twIndex     = fs.currentTWIndex;
        physTW            = fs.isInAtmoTW();
        inFlight          = fs.isInFlight();
        bool wasEVA       = inEVA;
        inEVA             = fs.isInEVA();
        hasTarget         = fs.hasTarget();
        isRecoverable     = fs.isRecoverable();
        updateCautionWarningState();

        // Force a full screen redraw when EVA state changes (Kerbal exits or
        // boards back). The vessel switch message handles the exit case but
        // the re-board may arrive before FLIGHT_STATUS updates inEVA, leaving
        // the SOI thumbnail and data fields stale. Setting prevScreen here
        // guarantees drawStaticMain() + the invalidation block fire on the
        // next loop pass regardless of which direction the transition went.
        if (inEVA != wasEVA) prevScreen = screen_COUNT;
      }
      break;

    case TEMP_LIMIT_MESSAGE:
      if (msgSize == sizeof(tempLimitMessage)) {
        tempLimitMessage t = parseMessage<tempLimitMessage>(msg);
        state.maxTemp  = (uint8_t)t.tempLimitPercentage;
        state.skinTemp = (uint8_t)t.skinTempLimitPercentage;
        updateCautionWarningState();
      }
      break;

    case ACTIONSTATUS_MESSAGE:
      if (msgSize == 1) {
        state.gear_on   = msg[0] & GEAR_ACTION;
        state.brakes_on = msg[0] & BRAKES_ACTION;
        state.lights_on = msg[0] & LIGHT_ACTION;
        state.RCS_on    = msg[0] & RCS_ACTION;
        state.SAS_on    = msg[0] & SAS_ACTION;
        state.abort_on  = msg[0] & ABORT_ACTION;
        updateCautionWarningState();
      }
      break;

    case ALTITUDE_MESSAGE:
      if (msgSize == sizeof(altitudeMessage)) {
        altitudeMessage a = parseMessage<altitudeMessage>(msg);
        state.alt_sl   = a.sealevel;
        state.alt_surf = a.surface;
        updateCautionWarningState();
      }
      break;

    case VELOCITY_MESSAGE:
      if (msgSize == sizeof(velocityMessage)) {
        velocityMessage v = parseMessage<velocityMessage>(msg);
        state.vel_surf = v.surface;
        state.vel_vert = v.vertical;
        updateCautionWarningState();
      }
      break;

    case AIRSPEED_MESSAGE:
      if (msgSize == sizeof(airspeedMessage)) {
        airspeedMessage a = parseMessage<airspeedMessage>(msg);
        state.gForces = a.gForces;
        updateCautionWarningState();
      }
      break;

    case APSIDES_MESSAGE:
      if (msgSize == sizeof(apsidesMessage)) {
        apsidesMessage a = parseMessage<apsidesMessage>(msg);
        state.apoapsis = a.apoapsis;
        updateCautionWarningState();
      }
      break;

    case DELTAV_MESSAGE:
      if (msgSize == sizeof(deltaVMessage)) {
        deltaVMessage dv = parseMessage<deltaVMessage>(msg);
        state.stageDV = dv.stageDeltaV;
        updateCautionWarningState();
      }
      break;

    case BURNTIME_MESSAGE:
      if (msgSize == sizeof(burnTimeMessage)) {
        burnTimeMessage b = parseMessage<burnTimeMessage>(msg);
        state.stageBurnTime = b.stageBurnTime;
        updateCautionWarningState();
      }
      break;

    case ATMO_CONDITIONS_MESSAGE:
      if (msgSize == sizeof(atmoConditionsMessage)) {
        atmoConditionsMessage a = parseMessage<atmoConditionsMessage>(msg);
        hasO2  = a.hasOxygen();
        inAtmo = a.isVesselInAtmosphere();
        updateCautionWarningState();
      }
      break;

    case ELECTRIC_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseResource(msg);
        state.EC       = r.available;
        state.EC_total = r.total;
        updateCautionWarningState();
      }
      break;

    case THROTTLE_CMD_MESSAGE:
      if (msgSize == sizeof(throttleMessage)) {
        throttleMessage t = parseMessage<throttleMessage>(msg);
        state.throttleCmd = (uint8_t)map(t.throttle, 0, INT16_MAX, 0, 100);
        updateCautionWarningState();
      }
      break;

    case SCENE_CHANGE_MESSAGE:
      // KerbalSimpit SCENE_CHANGE sends msg[0]=0 for flight scenes, msg[0]=1 for
      // non-flight (menu, tracking station, etc.) -- hence the inversion.
      flightScene = !msg[0];
      if (debugMode) Serial.println(flightScene ? F("Annunciator: Entering flight scene") : F("Annunciator: Leaving flight scene"));
      if (flightScene) {
        switchToScreen(screen_Main);
      } else {
        if (audioEnabled) audioSilence();
        resetDisplays();
        // switchToScreen() would call invalidateAllState() a second time on top of
        // resetDisplays(), corrupting the sentinel values. Set screen state directly.
        activeScreen     = screen_Standby;
        prevScreen       = screen_COUNT;
        lastScreenSwitch = millis();
      }
      break;

    case VESSEL_CHANGE_MESSAGE:
      if (msg[0] == 1) {
        if (debugMode) Serial.println(F("Annunciator: Vessel switch"));
        if (audioEnabled) audioSilence();
        resetDisplays();
        switchToScreen(activeScreen);
        prevScreen = screen_COUNT;
        // Recompute C&W immediately using the preserved state. FLIGHT_STATUS_MESSAGE
        // and THROTTLE_CMD_MESSAGE are event-driven and won't resend if values
        // haven't changed on the new vessel -- without this, MECO and other C&W
        // bits stay stale until the next change event triggers a recompute.
        updateCautionWarningState();
      } else if (msg[0] == 2) {
        if (debugMode) Serial.println(F("Annunciator: Docked"));
        docked = true;
        bitSet(state.vesselSituationState, 0);
      } else if (msg[0] == 3) {
        if (debugMode) Serial.println(F("Annunciator: Undocked"));
        docked = false;
        bitClear(state.vesselSituationState, 0);
      }
      break;
  }
}


/***************************************************************************************
   INIT SIMPIT
   Connects to KSP and registers all channels.
   Blocks until handshake succeeds.
****************************************************************************************/
void initSimpit() {
  simpit.inboundHandler(onSimpitMessage);
  while (!simpit.init()) {
    if (debugMode) Serial.println(F("Annunciator: Simpit handshake failed, retrying..."));
    delay(500);
  }
  if (debugMode) Serial.println(F("Annunciator: Simpit connected."));
  simpitConnected = true;

  simpit.registerChannel(VESSEL_NAME_MESSAGE);
  simpit.registerChannel(SOI_MESSAGE);
  simpit.registerChannel(FLIGHT_STATUS_MESSAGE);
  simpit.registerChannel(TEMP_LIMIT_MESSAGE);
  simpit.registerChannel(ACTIONSTATUS_MESSAGE);
  simpit.registerChannel(ALTITUDE_MESSAGE);
  simpit.registerChannel(VELOCITY_MESSAGE);
  simpit.registerChannel(AIRSPEED_MESSAGE);
  simpit.registerChannel(APSIDES_MESSAGE);
  simpit.registerChannel(DELTAV_MESSAGE);
  simpit.registerChannel(BURNTIME_MESSAGE);
  simpit.registerChannel(ATMO_CONDITIONS_MESSAGE);
  simpit.registerChannel(ELECTRIC_MESSAGE);
  simpit.registerChannel(THROTTLE_CMD_MESSAGE);
  simpit.registerChannel(SCENE_CHANGE_MESSAGE);
  simpit.registerChannel(VESSEL_CHANGE_MESSAGE);
}
