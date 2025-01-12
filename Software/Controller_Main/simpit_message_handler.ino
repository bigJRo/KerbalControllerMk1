/***************************************************************************************
   SIMPIT REGISTER INPUT CHANNELS
   Contains simpit register channel objections for all the items that are required by
   this microcontroller
   - No inputs
   - No outputs
****************************************************************************************/
void registerInputChannels() {  //Game message that registers the necessary messages with the Simput Plugin
  mySimpit.registerChannel(ACTIONSTATUS_MESSAGE);
  mySimpit.registerChannel(CAGSTATUS_MESSAGE);
  mySimpit.registerChannel(SAS_MODE_INFO_MESSAGE);
  mySimpit.registerChannel(FLIGHT_STATUS_MESSAGE);
  mySimpit.registerChannel(ALTITUDE_MESSAGE);
  mySimpit.registerChannel(VELOCITY_MESSAGE);
  mySimpit.registerChannel(AIRSPEED_MESSAGE);
  mySimpit.registerChannel(APSIDES_MESSAGE);
  mySimpit.registerChannel(DELTAV_MESSAGE);
  mySimpit.registerChannel(BURNTIME_MESSAGE);
  mySimpit.registerChannel(TEMP_LIMIT_MESSAGE);
  mySimpit.registerChannel(SOI_MESSAGE);
  mySimpit.registerChannel(THROTTLE_CMD_MESSAGE);
  mySimpit.registerChannel(ATMO_CONDITIONS_MESSAGE);
  mySimpit.registerChannel(VESSEL_NAME_MESSAGE);
  mySimpit.registerChannel(ELECTRIC_MESSAGE);
  mySimpit.registerChannel(EVA_MESSAGE);
  mySimpit.registerChannel(SCENE_CHANGE_MESSAGE);
  mySimpit.registerChannel(VESSEL_CHANGE_MESSAGE);
}


/***************************************************************************************
   SIMPIT MESSAGE HANDLER
   Established the simpit message handler for the main microncontroller
   - No inputs
   - No outputs
****************************************************************************************/
void messageHandler(byte messageType, byte msg[], byte msgSize) {
  switch (messageType) {
    case ACTIONSTATUS_MESSAGE:
      if (msgSize == 1) {
        gear_on = msg[0] & GEAR_ACTION;
        brakes_on = msg[0] & BRAKES_ACTION;
        lights_on = msg[0] & LIGHT_ACTION;
        RCS_on = msg[0] & RCS_ACTION;
        SAS_on = msg[0] & SAS_ACTION;
        abort_on = msg[0] & ABORT_ACTION;
      }
      break;
    case CAGSTATUS_MESSAGE:
      if (msgSize == sizeof(cagStatusMessage)) {
        cagStatusMessage cagStatus;
        cagStatus = parseCAGStatusMessage(msg);
        action1_on = cagStatus.is_action_activated(ag1);
        action2_on = cagStatus.is_action_activated(ag2);
        action3_on = cagStatus.is_action_activated(ag3);
        action4_on = cagStatus.is_action_activated(ag4);
        action5_on = cagStatus.is_action_activated(ag5);
        action6_on = cagStatus.is_action_activated(ag6);
        action7_on = cagStatus.is_action_activated(ag7);
        action8_on = cagStatus.is_action_activated(ag8);
        action9_on = cagStatus.is_action_activated(ag9);
        action10_on = cagStatus.is_action_activated(ag10);
        solarArray_on = cagStatus.is_action_activated(solar_array);
        antenna_on = cagStatus.is_action_activated(antenna);
        cargoDoor_on = cagStatus.is_action_activated(cargo_door);
        radiator_on = cagStatus.is_action_activated(radiator);
        drogue_on = cagStatus.is_action_activated(drogue);
        parachute_on = cagStatus.is_action_activated(parachute);
        ladder_on = cagStatus.is_action_activated(ladder);
      }
      break;
    case SAS_MODE_INFO_MESSAGE:
      if (msgSize == sizeof(SASInfoMessage)) {
        SASInfoMessage sasMode;
        sasMode = parseMessage<SASInfoMessage>(msg);
        SAS_mode = sasMode.currentSASMode;
        setSASLogic(SAS_mode); // function sets SAS Group per custom function
        break;
      }
    case FLIGHT_STATUS_MESSAGE:
      if (msgSize == sizeof(flightStatusMessage)) {
        flightStatusMessage myFlightStatus;
        myFlightStatus = parseMessage<flightStatusMessage>(msg);
        inFlight = myFlightStatus.isInFlight();
        inEVA = myFlightStatus.isInEVA();
        physTW = myFlightStatus.isInAtmoTW();
        hasTarget = myFlightStatus.hasTarget();
        stage = myFlightStatus.currentStage;
        crewCount = myFlightStatus.crewCount;
        crewCapacity = myFlightStatus.crewCapacity;
        commNet = myFlightStatus.commNetSignalStrenghPercentage;
        twIndex = myFlightStatus.currentTWIndex;
        vesselType = myFlightStatus.vesselType;
        vesselSituation = myFlightStatus.vesselSituation;
        isRecoverable = myFlightStatus.isRecoverable();
        vesselCtrlLvl = myFlightStatus.getControlLevel();
      }
      break;
    case SCENE_CHANGE_MESSAGE:
      flightScene = !msg[0];
      if (flightScene) {
        if (debug) { Serial.println("***In the flight scene***"); }
        tftDispMode = 1;
      } else {
        if (debug) { Serial.println("***Leaving the fight scene***"); }
        //tftDispMode = 0;
        resetDisplays();
        noTone(AUDIO_PIN);
      }
      break;
    case VESSEL_CHANGE_MESSAGE:
      if (msg[0] == 1) {
        resetDisplays();
        if (debug) { Serial.println("*******Vessel Change Message*******"); }
      } else if (msg[0] == 2) {
        if (debug) { Serial.println("*******Craft has docked*******"); }
        docked = true;
        dockUpdate = true;
      } else if (msg[0] == 3) {
        if (debug) { Serial.println("*******Craft has un-docked*******"); }
        docked = false;
        dockUpdate = true;
      }
      break;
    case THROTTLE_CMD_MESSAGE:
      if (msgSize == sizeof(throttleMessage)) {
        throttleMessage myThrottle;
        myThrottle = parseMessage<throttleMessage>(msg);
        gameThrottleCmd = map(myThrottle.throttle, 0, INT16_MAX, 0, 1023);
        Serial.println("********THROTTLE CMD MESSAGE RECEIVED********");
        Serial.print("myThrottle.throttle = ");
        Serial.println(myThrottle.throttle);
        Serial.print("gameThrottleCmd = ");
        Serial.println(gameThrottleCmd);
      }
      break;
    case ALTITUDE_MESSAGE:
      if (msgSize == sizeof(altitudeMessage)) {
        altitudeMessage myAltitude;
        myAltitude = parseMessage<altitudeMessage>(msg);
        alt_sl = myAltitude.sealevel;
        alt_surf = myAltitude.surface;
      }
      break;
    case VELOCITY_MESSAGE:
      if (msgSize == sizeof(velocityMessage)) {
        velocityMessage myVelocity;
        myVelocity = parseMessage<velocityMessage>(msg);
        vel_orb = myVelocity.orbital;
        vel_surf = myVelocity.surface;
        vel_vert = myVelocity.vertical;
      }
      break;
    case AIRSPEED_MESSAGE:
      if (msgSize == sizeof(airspeedMessage)) {
        airspeedMessage myAirspeed;
        myAirspeed = parseMessage<airspeedMessage>(msg);
        gForces = myAirspeed.gForces;
      }
      break;
    case APSIDES_MESSAGE:
      if (msgSize == sizeof(apsidesMessage)) {
        apsidesMessage myApsides;
        myApsides = parseMessage<apsidesMessage>(msg);
        periapsis = myApsides.periapsis;
        apoapsis = myApsides.apoapsis;
      }
      break;
    case DELTAV_MESSAGE:
      if (msgSize == sizeof(deltaVMessage)) {
        deltaVMessage myDV;
        myDV = parseMessage<deltaVMessage>(msg);
        stageDV = myDV.stageDeltaV;
        totalDV = myDV.totalDeltaV;
      }
      break;
    case BURNTIME_MESSAGE:
      if (msgSize == sizeof(burnTimeMessage)) {
        burnTimeMessage myBurn;
        myBurn = parseMessage<burnTimeMessage>(msg);
        stageBurnTime = myBurn.stageBurnTime;
        totalBurnTime = myBurn.totalBurnTime;
      }
      break;
    case TEMP_LIMIT_MESSAGE:
      if (msgSize == sizeof(tempLimitMessage)) {
        tempLimitMessage myTemp;
        myTemp = parseMessage<tempLimitMessage>(msg);
        maxTemp = myTemp.tempLimitPercentage;
        skinTemp = myTemp.skinTempLimitPercentage;
      }
      break;
    case SOI_MESSAGE:
      strSOI = "";
      for (uint8_t i = 0; i < msgSize; i++) {
        strSOI += char(msg[i]);
      }
      break;
    case ATMO_CONDITIONS_MESSAGE:
      if (msgSize == sizeof(atmoConditionsMessage)) {
        atmoConditionsMessage myAtmoConditions;
        myAtmoConditions = parseMessage<atmoConditionsMessage>(msg);
        airDensity = myAtmoConditions.airDensity;
        airTemp = myAtmoConditions.temperature;
        airPressure = myAtmoConditions.pressure;
        hasAtmo = myAtmoConditions.hasAtmosphere();
        hasO2 = myAtmoConditions.hasOxygen();
        inAtmo = myAtmoConditions.isVesselInAtmosphere();
      }
      break;
    case VESSEL_NAME_MESSAGE:
      gameVesselName = "";
      for (uint8_t i = 0; i < msgSize; i++) {
        gameVesselName += char(msg[i]);
      }
      break;
    case ELECTRIC_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage myEC;
        myEC = parseResource(msg);
        EC = myEC.available;
        EC_total = myEC.total;
      }
      break;
    case EVA_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage myEVA;
        myEVA = parseResource(msg);
        eva_fuel = myEVA.available;
        eva_fuel_total = myEVA.total;
      }
      break;
  }
}


/***************************************************************************************
   Initialize SIMPIT OBJECT
   Executes the simpit object, registers input channels and establishes the Simpit
   message handler
   - No inputs
   - No outputs
****************************************************************************************/
void initSimpitObject() {
  mySimpit.init();                                              //Init Simpit Object
  mySimpit.printToKSP("Connected Master MC", PRINT_TO_SCREEN);  // Display a message in KSP to indicate handshaking is complete.
  mySimpit.inboundHandler(messageHandler);                      // Sets our callback function. The KerbalSimpit library will call this function every time a packet is received
  registerInputChannels();                                      // Register the required channels with Simpit

  tft.println("  Kerbal Simpit Object Initialized");
}
