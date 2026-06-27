/***************************************************************************************
   SimpitHandler.ino -- KerbalSimpit message handling for Kerbal Controller Mk1 Annunciator

   Application-specific -- every display type subscribes to different channels and
   maps messages to its own AppState differently, so this lives as a sketch tab
   rather than in the library.

   Channels subscribed:
     VESSEL_NAME, SOI, FLIGHT_STATUS, TEMP_LIMIT, ACTIONSTATUS,
     ALTITUDE, VELOCITY, AIRSPEED, APSIDES,
     DELTAV, BURNTIME,
     ATMO_CONDITIONS,
     ELECTRIC,
     LF_STAGE, OX_STAGE, SF_STAGE,
     MONO,
     TACLS_RESOURCE, TACLS_WASTE,
     THROTTLE_CMD,
     SCENE_CHANGE, VESSEL_CHANGE

   ARP dependency note:
     LF_STAGE, OX_STAGE, SF_STAGE, MONO, ELECTRIC all require the
     Alternate Resource Panel (ARP) mod in KSP1. Without ARP these channels
     never send and the corresponding state fields remain at their initialised
     values (0.0). C&W conditions that check these resources guard against
     zero totals to prevent false triggers.

   TAC-LS dependency note:
     TACLS_RESOURCE and TACLS_WASTE require both ARP and the TAC Life Support
     mod in KSP1. Without TAC-LS the channels never send and tac* fields remain 0.
     CW_LIFE_SUPPORT is suppressed when crewCount is 0 and will not false-trigger
     when TAC-LS resources are absent.
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
      case LF_STAGE_MESSAGE:        msgName = "LF_STAGE";        break;
      case OX_STAGE_MESSAGE:        msgName = "OX_STAGE";        break;
      case SF_STAGE_MESSAGE:        msgName = "SF_STAGE";        break;
      case MONO_MESSAGE:            msgName = "MONO";            break;
      case TACLS_RESOURCE_MESSAGE:  msgName = "TACLS_RESOURCE";  break;
      case TACLS_WASTE_MESSAGE:     msgName = "TACLS_WASTE";     break;
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

        // Map Simpit raw situation bits to display bitmask.
        // Display: bit0=DOCKED (preserved), bit1=PRE-LAUNCH, bit2=FLIGHT,
        //          bit3=SUB-ORBIT, bit4=ORBIT, bit5=ESCAPE, bit6=SPLASH, bit7=LANDED
        uint8_t dispSit = state.vesselSituationState & 0x01;  // preserve DOCKED bit
        if (sit & sit_PreLaunch) bitSet(dispSit, VSIT_PRELAUNCH);
        if (sit & sit_Flying)    bitSet(dispSit, VSIT_FLIGHT);
        if (sit & sit_SubOrb)    bitSet(dispSit, VSIT_SUBORBIT);
        if (sit & sit_Orbit)     bitSet(dispSit, VSIT_ORBIT);
        if (sit & sit_Escaping)  bitSet(dispSit, VSIT_ESCAPE);
        if (sit & sit_Splashed)  bitSet(dispSit, VSIT_SPLASH);
        if (sit & sit_Landed)    bitSet(dispSit, VSIT_LANDED);
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
        // the SOI thumbnail and data fields stale.
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
        state.apoapsis  = a.apoapsis;
        state.periapsis = a.periapsis;  // now stored for CW_PE_LOW
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
        hasO2              = a.hasOxygen();
        inAtmo             = a.isVesselInAtmosphere();
        state.atmoPressure = a.pressure;   // kPa -- used for HIGH_Q proxy calculation
        state.atmoTemp     = a.temperature; // K  -- stored for future use
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

    // --- Propellant resources (all require ARP mod in KSP1) ---

    case LF_STAGE_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseResource(msg);
        state.LF_stage     = r.available;
        state.LF_stage_tot = r.total;
        updateCautionWarningState();
      }
      break;

    case OX_STAGE_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseResource(msg);
        state.OX_stage     = r.available;
        state.OX_stage_tot = r.total;
        updateCautionWarningState();
      }
      break;

    case SF_STAGE_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseResource(msg);
        state.SF_stage     = r.available;
        state.SF_stage_tot = r.total;
        updateCautionWarningState();
      }
      break;

    case MONO_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseResource(msg);
        state.mono     = r.available;
        state.mono_tot = r.total;
        updateCautionWarningState();
      }
      break;

    // --- TAC Life Support resources (require ARP + TAC-LS mods in KSP1) ---
    // TACLSResourceMessage contains three resourceMessage sub-structs:
    //   .food, .water, .oxygen -- each with .available and .total fields.
    // TACLSWasteMessage contains three resourceMessage sub-structs:
    //   .co2, .waste, .wasteWater -- each with .available and .total fields.
    // If TAC-LS is absent these channels never send and tac* fields stay at 0.

    case TACLS_RESOURCE_MESSAGE:
      if (msgSize == sizeof(TACLSResourceMessage)) {
        TACLSResourceMessage t = parseMessage<TACLSResourceMessage>(msg);
        state.tacFood        = t.currentFood;
        state.tacFood_tot    = t.maxFood;
        state.tacWater       = t.currentWater;
        state.tacWater_tot   = t.maxWater;
        state.tacOxygen      = t.currentOxygen;
        state.tacOxygen_tot  = t.maxOxygen;
        updateCautionWarningState();
      }
      break;

    case TACLS_WASTE_MESSAGE:
      if (msgSize == sizeof(TACLSWasteMessage)) {
        TACLSWasteMessage t = parseMessage<TACLSWasteMessage>(msg);
        state.tacCO2       = t.currentCO2;
        state.tacCO2_tot   = t.maxCO2;
        state.tacWaste     = t.currentWaste;
        state.tacWaste_tot = t.maxWaste;
        state.tacWW        = t.currentLiquidWaste;
        state.tacWW_tot    = t.maxLiquidWaste;
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
      if (debugMode) Serial.println(flightScene
                                    ? F("Annunciator: Entering flight scene")
                                    : F("Annunciator: Leaving flight scene"));
      if (flightScene) {
        switchToScreen(screen_Main);
        // Request immediate refresh on all subscribed channels so static values
        // (full tanks, stable orbit, etc.) populate without waiting for a change event.
        simpit.requestMessageOnChannel(0);
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
        // Recompute C&W immediately using the preserved state. FLIGHT_STATUS and
        // THROTTLE_CMD are event-driven and won't resend if values haven't changed
        // on the new vessel -- without this, C&W bits stay stale until the next
        // change event triggers a recompute.
        updateCautionWarningState();
        // Request a full telemetry refresh so all display fields repopulate
        // immediately rather than waiting for a change event on each channel.
        simpit.requestMessageOnChannel(0);
      } else if (msg[0] == 2) {
        if (debugMode) Serial.println(F("Annunciator: Docked"));
        docked = true;
        bitSet(state.vesselSituationState, VSIT_DOCKED);
      } else if (msg[0] == 3) {
        if (debugMode) Serial.println(F("Annunciator: Undocked"));
        docked = false;
        bitClear(state.vesselSituationState, VSIT_DOCKED);
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

  // Core telemetry
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
  simpit.registerChannel(THROTTLE_CMD_MESSAGE);
  simpit.registerChannel(SCENE_CHANGE_MESSAGE);
  simpit.registerChannel(VESSEL_CHANGE_MESSAGE);

  // Resources (all require ARP mod in KSP1)
  simpit.registerChannel(ELECTRIC_MESSAGE);
  simpit.registerChannel(LF_STAGE_MESSAGE);
  simpit.registerChannel(OX_STAGE_MESSAGE);
  simpit.registerChannel(SF_STAGE_MESSAGE);
  simpit.registerChannel(MONO_MESSAGE);

  // TAC Life Support (requires ARP + TAC-LS mods in KSP1)
  simpit.registerChannel(TACLS_RESOURCE_MESSAGE);
  simpit.registerChannel(TACLS_WASTE_MESSAGE);
}
