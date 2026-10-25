/***************************************************************************************
   simpit_message_handler.ino — the master's KerbalSimpit link.

   The master has its own Simpit connection (SerialUSB1); each display panel has another.
   This tab registers the channels the master needs, feeds every message to the
   autopilots' ingest functions, keeps the panel-LED state the module handlers read
   (action-group and CAG status), and keeps the little cross-message state below.

   Start-up does not block: initSimpitObject() makes one handshake attempt (the library
   gives up after about a second without KSP), and simpitService(), called every loop,
   retries every SIMPIT_RETRY_MS until KSP answers, then runs mySimpit.update(). The
   I2C links keep running while KSP is not up.
****************************************************************************************/
static const uint32_t SIMPIT_RETRY_MS = 2000;

static bool     g_simpitConnected = false;
static uint32_t g_simpitLastTryMs = 0;
static bool     g_simpitFlightScene = false;
static bool     g_simpitHasTarget   = false;   // FLIGHT_STATUS, used with TARGETINFO
static uint8_t  g_simpitTwIndex     = 0;       // time-warp index, 0 = 1x
static float    g_simpitStageDV     = 0.0f;    // DELTAV and BURNTIME arrive separately; the
static float    g_simpitStageBurn   = 0.0f;    // acceleration estimate needs both

bool simpitConnected()   { return g_simpitConnected; }
bool simpitFlightScene() { return g_simpitFlightScene; }
bool simpitWarping()     { return g_simpitTwIndex > 0; }


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
  mySimpit.registerChannel(APSIDESTIME_MESSAGE);  // Time-to-apoapsis for circularization timing (ascent autopilot)
  mySimpit.registerChannel(ORBIT_MESSAGE);        // Inclination for the ascent autopilot azimuth target
  mySimpit.registerChannel(ROTATION_DATA_MESSAGE);// Attitude + surface prograde for the ascent autopilot steering loop
  mySimpit.registerChannel(TARGETINFO_MESSAGE);   // Target bearing for the rover drive-to-target hold (hold autopilot)
  mySimpit.registerChannel(MANEUVER_MESSAGE);     // Node time / dV / pointing for the burn autopilot
  mySimpit.registerChannel(DELTAV_MESSAGE);
  mySimpit.registerChannel(BURNTIME_MESSAGE);
  mySimpit.registerChannel(TEMP_LIMIT_MESSAGE);
  mySimpit.registerChannel(SOI_MESSAGE);
  mySimpit.registerChannel(THROTTLE_CMD_MESSAGE);
  mySimpit.registerChannel(ATMO_CONDITIONS_MESSAGE);
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
        hpIngestBrakes(brakes_on);  // brakes drop rover cruise (hold autopilot)
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
      }
      break;
    case FLIGHT_STATUS_MESSAGE:
      if (msgSize == sizeof(flightStatusMessage)) {
        flightStatusMessage myFlightStatus;
        myFlightStatus = parseMessage<flightStatusMessage>(msg);
        apIngestFlightStatus(myFlightStatus.isInFlight());  // feed ascent autopilot
        g_simpitHasTarget = myFlightStatus.hasTarget();
        g_simpitTwIndex   = myFlightStatus.currentTWIndex;  // the Annunciator's WARP tile
        hpIngestFlightStatus(myFlightStatus.vesselType, myFlightStatus.vesselSituation, myFlightStatus.hasTarget());  // feed hold autopilot
        lpIngestFlightStatus(myFlightStatus.vesselType, myFlightStatus.vesselSituation);  // feed landing autopilot
      }
      break;
    case SCENE_CHANGE_MESSAGE:
      g_simpitFlightScene = !msg[0];
      if (g_simpitFlightScene) {
        if (debug) { Serial.println("***In the flight scene***"); }
      } else {
        if (debug) { Serial.println("***Leaving the flight scene***"); }
        g_simpitTwIndex = 0;
        hpVesselChanged();   // leaving the flight scene drops every hold mode, silently
        bpVesselChanged(); lpVesselChanged();
      }
      break;
    case VESSEL_CHANGE_MESSAGE:
      if (msg[0] == 1) {
        hpVesselChanged();   // a new vessel starts with no hold modes
        bpVesselChanged(); lpVesselChanged();
        if (debug) { Serial.println("*******Vessel Change Message*******"); }
      } else if (msg[0] == 2) {
        if (debug) { Serial.println("*******Craft has docked*******"); }
      } else if (msg[0] == 3) {
        if (debug) { Serial.println("*******Craft has un-docked*******"); }
      }
      break;
    case THROTTLE_CMD_MESSAGE:
      if (msgSize == sizeof(throttleMessage)) {
        throttleMessage myThrottle;
        myThrottle = parseMessage<throttleMessage>(msg);
        hpIngestThrottle((float)myThrottle.throttle / (float)INT16_MAX);  // autothrottle bumpless engage
        lpIngestThrottle((float)myThrottle.throttle / (float)INT16_MAX);
      }
      break;
    case ALTITUDE_MESSAGE:
      if (msgSize == sizeof(altitudeMessage)) {
        altitudeMessage myAltitude;
        myAltitude = parseMessage<altitudeMessage>(msg);
        alt_surf = myAltitude.surface;
        apIngestAltitude(myAltitude.sealevel, myAltitude.surface);  // feed ascent autopilot
        hpIngestAltitude(myAltitude.sealevel);                      // feed hold autopilot (ALT hold)
        lpIngestAltitude(myAltitude.sealevel, myAltitude.surface);   // feed landing autopilot (radar altitude)
      }
      break;
    case VELOCITY_MESSAGE:
      if (msgSize == sizeof(velocityMessage)) {
        velocityMessage myVelocity;
        myVelocity = parseMessage<velocityMessage>(msg);
        apIngestVelocity(myVelocity.orbital, myVelocity.surface, myVelocity.vertical);  // feed ascent autopilot
        hpIngestVelocity(myVelocity.surface, myVelocity.vertical);                     // feed hold autopilot (V/S, CRUISE)
        lpIngestVelocity(myVelocity.surface, myVelocity.vertical);                     // feed landing autopilot
        bpIngestVelocity(myVelocity.orbital);                                          // feed burn autopilot
      }
      break;
    case AIRSPEED_MESSAGE:
      if (msgSize == sizeof(airspeedMessage)) {
        airspeedMessage myAirspeed;
        myAirspeed = parseMessage<airspeedMessage>(msg);
        apIngestGForce(myAirspeed.gForces);  // feed ascent autopilot max-G limiter
        hpIngestAirspeed(myAirspeed.IAS, myAirspeed.mach);  // feed hold autopilot (IAS / MACH)
        lpIngestAirspeed(myAirspeed.mach);                  // feed landing autopilot (plane hand-off)
        aeIngestGForce(myAirspeed.gForces);                 // acceleration estimate (measured during a burn)
      }
      break;
    case APSIDES_MESSAGE:
      if (msgSize == sizeof(apsidesMessage)) {
        apsidesMessage myApsides;
        myApsides = parseMessage<apsidesMessage>(msg);
        apIngestApsides(myApsides.apoapsis, myApsides.periapsis);  // feed ascent autopilot
        bpIngestApsides(myApsides.apoapsis, myApsides.periapsis);  // feed burn autopilot
      }
      break;
    case APSIDESTIME_MESSAGE:
      if (msgSize == sizeof(apsidesTimeMessage)) {
        apsidesTimeMessage myApsidesTime;
        myApsidesTime = parseMessage<apsidesTimeMessage>(msg);
        apIngestApsidesTime((float)myApsidesTime.apoapsis, (float)myApsidesTime.periapsis);  // feed ascent autopilot
        bpIngestApsidesTime((float)myApsidesTime.apoapsis, (float)myApsidesTime.periapsis);  // feed burn autopilot
      }
      break;
    case ORBIT_MESSAGE:
      if (msgSize == sizeof(orbitInfoMessage)) {
        orbitInfoMessage myOrbit;
        myOrbit = parseMessage<orbitInfoMessage>(msg);
        apIngestOrbit(myOrbit.inclination);  // feed ascent autopilot azimuth target
        bpIngestOrbit(myOrbit.eccentricity, myOrbit.semiMajorAxis, myOrbit.inclination, myOrbit.longAscendingNode,
                      myOrbit.argPeriapsis, myOrbit.trueAnomaly, myOrbit.period);  // feed burn autopilot planners
      }
      break;
    case ROTATION_DATA_MESSAGE:
      if (msgSize == sizeof(vesselPointingMessage)) {
        vesselPointingMessage myPointing;
        myPointing = parseMessage<vesselPointingMessage>(msg);
        apIngestAttitude(myPointing.heading, myPointing.pitch, myPointing.roll,
                         myPointing.surfaceVelocityHeading, myPointing.surfaceVelocityPitch,
                         myPointing.orbitalVelocityHeading, myPointing.orbitalVelocityPitch);  // feed ascent autopilot steering
        hpIngestAttitude(myPointing.heading, myPointing.pitch, myPointing.roll,
                         myPointing.surfaceVelocityHeading, myPointing.surfaceVelocityPitch);  // feed hold autopilot
        bpIngestAttitude(myPointing.heading, myPointing.pitch, myPointing.roll,
                         myPointing.orbitalVelocityHeading, myPointing.orbitalVelocityPitch);  // feed burn autopilot
        lpIngestAttitude(myPointing.heading, myPointing.pitch, myPointing.roll,
                         myPointing.orbitalVelocityHeading, myPointing.orbitalVelocityPitch);  // feed landing autopilot (ENTRY)
      }
      break;
    case DELTAV_MESSAGE:
      if (msgSize == sizeof(deltaVMessage)) {
        deltaVMessage myDV;
        myDV = parseMessage<deltaVMessage>(msg);
        g_simpitStageDV = myDV.stageDeltaV;
        apIngestStageDeltaV(myDV.stageDeltaV);  // feed ascent autopilot auto-staging
        aeIngestStage(myDV.stageDeltaV, g_simpitStageBurn);  // acceleration estimate + shared auto-stage
      }
      break;
    case BURNTIME_MESSAGE:
      if (msgSize == sizeof(burnTimeMessage)) {
        burnTimeMessage myBurn;
        myBurn = parseMessage<burnTimeMessage>(msg);
        g_simpitStageBurn = myBurn.stageBurnTime;
        aeIngestStage(g_simpitStageDV, myBurn.stageBurnTime);
      }
      break;
    case TEMP_LIMIT_MESSAGE:
      if (msgSize == sizeof(tempLimitMessage)) {
        tempLimitMessage myTemp;
        myTemp = parseMessage<tempLimitMessage>(msg);
        apIngestSkinTemp(myTemp.skinTempLimitPercentage / 100.0f);  // feed ascent autopilot heat limiter
      }
      break;
    case SOI_MESSAGE: {
      char bodyBuf[24];
      uint8_t n = 0;
      for (uint8_t i = 0; i < msgSize; i++) {
        if (n < sizeof(bodyBuf) - 1) bodyBuf[n++] = char(msg[i]);
      }
      bodyBuf[n] = '\0';
      apIngestSOI(bodyBuf);  // feed ascent autopilot body-profile / SoI adaptation
      {
        BodyParams b = getBodyParams(bodyBuf);   // shared celestial-body table
        bpIngestBody(b.radius, b.gravity, bodyBuf);        // mu for the planners; SOI change aborts a burn
        lpIngestBody(b.gravity, b.flyHigh, bodyBuf);       // ignition altitude; plane hand-off ceiling
      }
      break;
    }
    case ATMO_CONDITIONS_MESSAGE:
      if (msgSize == sizeof(atmoConditionsMessage)) {
        atmoConditionsMessage myAtmoConditions;
        myAtmoConditions = parseMessage<atmoConditionsMessage>(msg);
        apIngestAtmo(myAtmoConditions.airDensity, myAtmoConditions.hasAtmosphere(),
                     myAtmoConditions.isVesselInAtmosphere());  // feed ascent autopilot (max-Q + airless/atmospheric branch)
        hpIngestAtmo(myAtmoConditions.hasAtmosphere(), myAtmoConditions.isVesselInAtmosphere());  // feed hold autopilot
        lpIngestAtmo(myAtmoConditions.airDensity, myAtmoConditions.isVesselInAtmosphere());      // feed landing autopilot
        aeIngestAtmo(myAtmoConditions.isVesselInAtmosphere());                                    // measured accel only in vacuum
      }
      break;
    case TARGETINFO_MESSAGE:
      if (msgSize == sizeof(targetMessage)) {
        targetMessage myTarget;
        myTarget = parseMessage<targetMessage>(msg);
        // Simpit's target velocity is the unsigned magnitude of the whole relative velocity.
        // Project it on the line of sight for a signed closing rate (> 0 = opening).
        float losN, losE, losU, vN, vE, vU;
        {
          float p = myTarget.pitch * 0.0174532925199f, h = myTarget.heading * 0.0174532925199f;
          losN = cosf(p) * cosf(h); losE = cosf(p) * sinf(h); losU = sinf(p);
          p = myTarget.velocityPitch * 0.0174532925199f; h = myTarget.velocityHeading * 0.0174532925199f;
          vN = cosf(p) * cosf(h); vE = cosf(p) * sinf(h); vU = sinf(p);
        }
        float closing = myTarget.velocity * (losN * vN + losE * vE + losU * vU);
        hpIngestTarget(g_simpitHasTarget, myTarget.heading, myTarget.pitch, myTarget.distance, closing);   // NAV / GS / TGT / FOLLOW
        bpIngestTarget(g_simpitHasTarget, myTarget.distance, myTarget.velocity, myTarget.heading, myTarget.pitch,
                       myTarget.velocityHeading, myTarget.velocityPitch);                          // approach-rate hold
      }
      break;
    case MANEUVER_MESSAGE:
      if (msgSize == sizeof(maneuverMessage)) {
        maneuverMessage myNode;
        myNode = parseMessage<maneuverMessage>(msg);
        bpIngestNode(myNode.timeToNextManeuver, myNode.deltaVNextManeuver, myNode.durationNextManeuver,
                     myNode.headingNextManeuver, myNode.pitchNextManeuver);   // node executor
      }
      break;
  }
}


/***************************************************************************************
   START / SERVICE THE SIMPIT LINK
   initSimpitObject(): one handshake attempt (about a second at most without KSP).
   simpitService(): every loop -- retry every SIMPIT_RETRY_MS until connected, then
   mySimpit.update(), which dispatches inbound messages to messageHandler().
****************************************************************************************/
bool initSimpitObject() {
  g_simpitLastTryMs = millis();
  if (!mySimpit.init()) {
    if (debug) { Serial.println("Simpit: no answer from KSP, will retry"); }
    return false;
  }
  g_simpitConnected = true;
  mySimpit.printToKSP("Connected Master MC", PRINT_TO_SCREEN);  // handshake complete
  mySimpit.inboundHandler(messageHandler);  // the library calls this for every inbound packet
  registerInputChannels();
  if (debug) { Serial.println("Simpit: connected"); }
  return true;
}

void simpitService() {
  if (g_simpitConnected) {
    mySimpit.update();
    return;
  }
  if (millis() - g_simpitLastTryMs >= SIMPIT_RETRY_MS) initSimpitObject();
}
