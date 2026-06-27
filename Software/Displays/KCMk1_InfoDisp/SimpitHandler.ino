/***************************************************************************************
   SimpitHandler.ino -- KerbalSimpit message handling for Kerbal Controller Mk1 InfoDisp

   Channels subscribed:
     VESSEL_NAME, SOI, FLIGHT_STATUS, ALTITUDE, VELOCITY, AIRSPEED,
     APSIDES, APSIDESTIME, DELTAV, BURNTIME, ORBIT, ROTATION_DATA,
     MANEUVER, TARGETINFO, INTERSECTS, ATMO_CONDITIONS, ACTIONSTATUS,
     CAGSTATUS, THROTTLE_CMD, WHEEL_CMD, ELECTRIC,
     SCENE_CHANGE, VESSEL_CHANGE

   Phase 2: Simpit integration for live KSP telemetry. ✓
   Phase 3: I2C slave interface. ✓
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


// KerbalSimpit simpit object moved to AAA_Globals.ino (#9)


/***************************************************************************************
   SIMPIT MESSAGE HANDLER
   Called by simpit.update() on every subscribed message arrival.
   Populates state fields only — never draws directly. May call switchToScreen()
   on SCENE_CHANGE_MESSAGE and VESSEL_CHANGE_MESSAGE events.
****************************************************************************************/
void onSimpitMessage(byte messageType, byte msg[], byte msgSize) {

  if (debugMode) {
    const char *n;
    switch (messageType) {
      case VESSEL_NAME_MESSAGE:     n = "VESSEL_NAME";     break;
      case SOI_MESSAGE:             n = "SOI";             break;
      case FLIGHT_STATUS_MESSAGE:   n = "FLIGHT_STATUS";   break;
      case ALTITUDE_MESSAGE:        n = "ALTITUDE";        break;
      case VELOCITY_MESSAGE:        n = "VELOCITY";        break;
      case AIRSPEED_MESSAGE:        n = "AIRSPEED";        break;
      case APSIDES_MESSAGE:         n = "APSIDES";         break;
      case APSIDESTIME_MESSAGE:     n = "APSIDESTIME";     break;
      case DELTAV_MESSAGE:          n = "DELTAV";          break;
      case BURNTIME_MESSAGE:        n = "BURNTIME";        break;
      case ORBIT_MESSAGE:           n = "ORBIT";           break;
      case ROTATION_DATA_MESSAGE:   n = "ROTATION_DATA";   break;
      case MANEUVER_MESSAGE:        n = "MANEUVER";        break;
      case TARGETINFO_MESSAGE:      n = "TARGETINFO";      break;
      case INTERSECTS_MESSAGE:      n = "INTERSECTS";      break;
      case ATMO_CONDITIONS_MESSAGE: n = "ATMO_CONDITIONS"; break;
      case ACTIONSTATUS_MESSAGE:    n = "ACTION_STATUS";   break;
      case SAS_MODE_INFO_MESSAGE:   n = "SAS_MODE_INFO";   break;
      case CAGSTATUS_MESSAGE:       n = "CAG_STATUS";      break;
      case THROTTLE_CMD_MESSAGE:    n = "THROTTLE_CMD";    break;
      case WHEEL_CMD_MESSAGE:        n = "WHEEL_CMD";        break;
      case ELECTRIC_MESSAGE:        n = "ELECTRIC";        break;
      case SCENE_CHANGE_MESSAGE:    n = "SCENE_CHANGE";    break;
      case VESSEL_CHANGE_MESSAGE:   n = "VESSEL_CHANGE";   break;
      default:                      n = "UNKNOWN";         break;
    }
    Serial.print(F("InfoDisp: Simpit msg="));
    Serial.println(n);
  }

  switch (messageType) {

    // ── Vessel identity ──────────────────────────────────────────────────────────────

    case VESSEL_NAME_MESSAGE:
      state.vesselName = "";
      for (uint8_t i = 0; i < msgSize; i++) state.vesselName += char(msg[i]);
      break;

    case SOI_MESSAGE:
      {
        String newSOI = "";
        for (uint8_t i = 0; i < msgSize; i++) newSOI += char(msg[i]);
        if (newSOI != state.gameSOI) {
          state.gameSOI = newSOI;
          currentBody = getBodyParams(state.gameSOI);
          // ORB screen title includes SOI name — force its title bar to redraw only on change
          if (activeScreen == screen_ORB) prevScreen = screen_COUNT;
        }
      }
      break;

    case FLIGHT_STATUS_MESSAGE:
      if (msgSize == sizeof(flightStatusMessage)) {
        flightStatusMessage fs = parseMessage<flightStatusMessage>(msg);
        state.situation     = (VesselSituation)fs.vesselSituation;
        state.vesselType    = (VesselType)fs.vesselType;
        state.crewCount     = fs.crewCount;
        state.crewCapacity  = fs.crewCapacity;
        state.ctrlLevel     = fs.getControlLevel();
        state.commNetSignal = fs.commNetSignalStrenghPercentage;
        state.isRecoverable = fs.isRecoverable();
        state.targetAvailable = fs.hasTarget();
        // state.inAtmo populated by ATMO_CONDITIONS_MESSAGE
        // state.sasMode — no field in KerbalSimpit 2.4.0 flightStatusMessage

        // If a vessel switch is pending, now we have the correct vesselType — switch screens
        if (_pendingContextSwitch) {
          _pendingContextSwitch = false;
          if (debugMode) {
            Serial.print(F("InfoDisp: VesselSwitch FLIGHT_STATUS - type="));
            Serial.print((int)state.vesselType);
            Serial.print(F(" tgtAvail="));
            Serial.print(state.targetAvailable);
            Serial.print(F(" tgtDist="));
            Serial.println(state.tgtDistance);
          }
          switchToScreen(contextScreen());
          // TARGETINFO may not have arrived yet — set flag to re-check for docking context
          // once target distance is known (catches switching to a vessel near a dock target)
          _pendingDockCheck = true;
        }

        // Pre-launch board: only for vessel types that would go to LNCH screen.
        // Planes → ACFT and Rovers → ROVR even on the pad, so skip the board for them.
        bool planeOrRover = (state.vesselType == type_Plane || state.vesselType == type_Rover);
        bool isPreLaunch  = (!planeOrRover && (state.situation & sit_PreLaunch) != 0);
        if (isPreLaunch && !_lnchPrelaunchMode && !_lnchPrelaunchDismissed) {
          _lnchPrelaunchMode = true;
          if (activeScreen == screen_LNCH) switchToScreen(screen_LNCH);
        } else if (!isPreLaunch && _lnchPrelaunchMode) {
          // Launched — clear everything including dismissed flag
          _lnchPrelaunchMode      = false;
          _lnchPrelaunchDismissed = false;
          _lnchOrbitalMode        = false;
          _lnchManualOverride     = false;
          if (activeScreen == screen_LNCH) switchToScreen(screen_LNCH);
        } else if (!isPreLaunch) {
          // No longer pre-launch for any reason — clear dismissed flag for next time on pad
          _lnchPrelaunchDismissed = false;
        }
      }
      break;

    // ── Altitude & velocity ──────────────────────────────────────────────────────────

    case ALTITUDE_MESSAGE:
      if (msgSize == sizeof(altitudeMessage)) {
        altitudeMessage a = parseMessage<altitudeMessage>(msg);
        state.altitude  = a.sealevel;
        state.radarAlt  = a.surface;
      }
      break;

    case VELOCITY_MESSAGE:
      if (msgSize == sizeof(velocityMessage)) {
        velocityMessage v = parseMessage<velocityMessage>(msg);
        state.orbitalVel  = v.orbital;
        state.surfaceVel  = v.surface;
        state.verticalVel = v.vertical;
      }
      break;

    case AIRSPEED_MESSAGE:
      if (msgSize == sizeof(airspeedMessage)) {
        airspeedMessage a = parseMessage<airspeedMessage>(msg);
        state.IAS        = a.IAS;
        state.machNumber = a.mach;
        state.gForce     = a.gForces;
      }
      break;

    // ── Apsides ──────────────────────────────────────────────────────────────────────

    case APSIDES_MESSAGE:
      if (msgSize == sizeof(apsidesMessage)) {
        apsidesMessage a = parseMessage<apsidesMessage>(msg);
        state.apoapsis  = a.apoapsis;
        state.periapsis = a.periapsis;
      }
      break;

    case APSIDESTIME_MESSAGE:
      if (msgSize == sizeof(apsidesTimeMessage)) {
        apsidesTimeMessage a = parseMessage<apsidesTimeMessage>(msg);
        state.timeToAp = (float)a.apoapsis;
        state.timeToPe = (float)a.periapsis;
      }
      break;

    // ── Delta-V & burn ───────────────────────────────────────────────────────────────

    case DELTAV_MESSAGE:
      if (msgSize == sizeof(deltaVMessage)) {
        deltaVMessage dv = parseMessage<deltaVMessage>(msg);
        state.stageDeltaV = dv.stageDeltaV;
        state.totalDeltaV = dv.totalDeltaV;
      }
      break;

    case BURNTIME_MESSAGE:
      if (msgSize == sizeof(burnTimeMessage)) {
        burnTimeMessage b = parseMessage<burnTimeMessage>(msg);
        state.stageBurnTime = b.stageBurnTime;
      }
      break;

    case THROTTLE_CMD_MESSAGE:
      if (msgSize == sizeof(throttleMessage)) {
        throttleMessage t = parseMessage<throttleMessage>(msg);
        // Simpit throttle is 0..INT16_MAX; normalise to 0.0..1.0
        state.throttle = (float)t.throttle / (float)INT16_MAX;
      }
      break;

    case WHEEL_CMD_MESSAGE:
      // Outbound telemetry echo of combined wheel input (keyboard + Simpit).
      // wheelMessage struct: int16_t steering, int16_t throttle, uint8_t mask.
      // Note: WHEEL_THROTTLE_MASK is not defined in KerbalSimpit 2.4.0 — read directly.
      // Throttle is signed int16; clamp to 0.0..1.0 (negative = braking/reverse).
      if (msgSize == sizeof(wheelMessage)) {
        wheelMessage w = parseMessage<wheelMessage>(msg);
        // Store signed value: positive = forward, negative = reverse/braking.
        // Range is -1.0..1.0 (INT16_MIN..INT16_MAX scaled).
        state.wheelThrottle = (float)w.throttle / (float)INT16_MAX;
      }
      break;

    case ELECTRIC_MESSAGE:
      // resourceMessage struct: float total, float available
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        if (r.total > 0.0f)
          state.electricChargePercent = (r.available / r.total) * 100.0f;
        else
          state.electricChargePercent = 0.0f;
      }
      break;

    // ── Orbital elements ─────────────────────────────────────────────────────────────

    case ORBIT_MESSAGE:
      if (msgSize == sizeof(orbitInfoMessage)) {
        orbitInfoMessage o = parseMessage<orbitInfoMessage>(msg);
        state.eccentricity  = o.eccentricity;
        state.semiMajorAxis = o.semiMajorAxis;
        state.inclination   = o.inclination;
        state.LAN           = o.longAscendingNode;
        state.argOfPe       = o.argPeriapsis;
        state.trueAnomaly   = o.trueAnomaly;
        state.meanAnomaly   = o.meanAnomaly;
        state.orbitalPeriod = o.period;
      }
      break;

    // ── Attitude ─────────────────────────────────────────────────────────────────────

    case ROTATION_DATA_MESSAGE:
      if (msgSize == sizeof(vesselPointingMessage)) {
        vesselPointingMessage r = parseMessage<vesselPointingMessage>(msg);
        state.heading       = r.heading;
        state.pitch         = r.pitch;
        state.roll          = r.roll;
        state.orbVelHeading = r.orbitalVelocityHeading;
        state.orbVelPitch   = r.orbitalVelocityPitch;
        state.srfVelHeading = r.surfaceVelocityHeading;
        state.srfVelPitch   = r.surfaceVelocityPitch;
      }
      break;

    // ── Maneuver node ────────────────────────────────────────────────────────────────

    case MANEUVER_MESSAGE:
      if (msgSize == sizeof(maneuverMessage)) {
        maneuverMessage m = parseMessage<maneuverMessage>(msg);
        state.mnvrTime     = m.timeToNextManeuver;
        state.mnvrDeltaV   = m.deltaVNextManeuver;
        state.mnvrDuration = m.durationNextManeuver;
        // m.deltaVTotal is the dV remaining in the maneuver plan, NOT the vessel's
        // total propellant dV. Leave state.totalDeltaV owned by DELTAV_MESSAGE only.
        state.mnvrHeading  = m.headingNextManeuver;
        state.mnvrPitch    = m.pitchNextManeuver;
      }
      break;

    // ── Target ───────────────────────────────────────────────────────────────────────

    case TARGETINFO_MESSAGE:
      if (msgSize == sizeof(targetMessage)) {
        targetMessage t = parseMessage<targetMessage>(msg);
        state.tgtDistance   = t.distance;
        state.tgtVelocity   = t.velocity;
        state.tgtHeading    = t.heading;
        state.tgtPitch      = t.pitch;
        state.tgtVelHeading = t.velocityHeading;
        state.tgtVelPitch   = t.velocityPitch;

        // After a vessel switch, contextScreen() runs before tgtDistance is known,
        // so the docking distance check may fail even with a valid nearby target.
        // Re-run contextScreen() once when the first TARGETINFO arrives post-switch.
        // This correctly handles all priorities (plane/rover/lander/dock/orb).
        if (_pendingDockCheck) {
          _pendingDockCheck = false;
          if (debugMode) {
            Serial.print(F("InfoDisp: VesselSwitch TARGETINFO - tgtAvail="));
            Serial.print(state.targetAvailable);
            Serial.print(F(" tgtDist="));
            Serial.println(state.tgtDistance);
          }
          // KSP may report targetAvailable=false even while sending TARGETINFO with
          // a valid distance — use distance alone to confirm a nearby docking target.
          if (state.tgtDistance > 0.0f && state.tgtDistance <= DOCK_DIST_WARN_M) {
            switchToScreen(screen_DOCK);
          } else {
            switchToScreen(contextScreen());
          }
        }
      }
      break;

    // INTERSECTS_MESSAGE is KSP2-only — not registered for KSP1.

    // ── Atmosphere & environment ─────────────────────────────────────────────────────

    case ATMO_CONDITIONS_MESSAGE:
      if (msgSize == sizeof(atmoConditionsMessage)) {
        atmoConditionsMessage a = parseMessage<atmoConditionsMessage>(msg);
        state.airDensity = a.airDensity;
        state.inAtmo     = a.isVesselInAtmosphere();
      }
      break;

    // ── Action groups ────────────────────────────────────────────────────────────────

    case ACTIONSTATUS_MESSAGE:
      if (msgSize >= 1) {
        state.gear_on   = msg[0] & GEAR_ACTION;
        state.brakes_on = msg[0] & BRAKES_ACTION;
        state.rcs_on    = msg[0] & RCS_ACTION;
      }
      break;

    case SAS_MODE_INFO_MESSAGE:
      if (msgSize == sizeof(SASInfoMessage)) {
        SASInfoMessage s = parseSASInfoMessage(msg);
        state.sasMode = s.currentSASMode;  // 255 = SAS disabled, 0 = StabilityAssist, etc.
      }
      break;

    case CAGSTATUS_MESSAGE: {
      // Cast msg directly to cagStatusMessage and use is_action_activated(n).
      // parseCAGStatusMessage() is deprecated — use the struct directly.
      cagStatusMessage *cag = (cagStatusMessage *)msg;
      if (DROGUE_DEPLOY_CAG >= 1 && DROGUE_DEPLOY_CAG <= 256)
        state.drogueDeploy = cag->is_action_activated(DROGUE_DEPLOY_CAG);
      if (DROGUE_CUT_CAG   >= 1 && DROGUE_CUT_CAG   <= 256)
        state.drogueCut    = cag->is_action_activated(DROGUE_CUT_CAG);
      if (MAIN_DEPLOY_CAG  >= 1 && MAIN_DEPLOY_CAG  <= 256)
        state.mainDeploy   = cag->is_action_activated(MAIN_DEPLOY_CAG);
      if (MAIN_CUT_CAG     >= 1 && MAIN_CUT_CAG     <= 256)
        state.mainCut      = cag->is_action_activated(MAIN_CUT_CAG);
      break;
    }

    // ── Scene and vessel lifecycle ───────────────────────────────────────────────────

    case SCENE_CHANGE_MESSAGE:
      // msg[0] == 0 → flight scene; msg[0] == 1 → non-flight (menu, tracking, etc.)
      flightScene = (msg[0] == 0);
      if (debugMode)
        Serial.println(flightScene ? F("InfoDisp: Entering flight scene")
                                   : F("InfoDisp: Leaving flight scene"));
      if (flightScene) {
        demoMode = false;
        switchToScreen(contextScreen());
        simpit.requestMessageOnChannel(0);
      } else {
        // Non-flight (menus, tracking station, etc.) — show standby splash
        demoMode = false;
        drawStandbyScreen(infoDisp);
        activeScreen = screen_LNCH;
        prevScreen   = screen_COUNT;
      }
      break;

    case VESSEL_CHANGE_MESSAGE:
      if (msg[0] == 1) {
        // Vessel switch (focus changed to another vessel).
        // Guard: if we just docked (within 2s), KSP sends a vessel switch immediately
        // as focus transfers to the combined vessel. Don't clear _vesselDocked in that case.
        bool recentDock = (_vesselDocked && (millis() - _dockedTimestamp < 2000UL));
        if (!recentDock) {
          _vesselDocked = false;
        }
        _pendingDockCheck = false;  // clear any stale dock check from previous switch
        if (debugMode) Serial.println(F("InfoDisp: Vessel switch"));
        // Reset LNDG re-entry row mode and parachute deployment state for new vessel
        _lndgReentryMode    = false;   // #34 reset re-entry mode on vessel switch
        _orbAdvancedMode    = false;   // #43 reset ORB advanced mode on vessel switch
        _scftPrevOrbMode     = false;   // #50 reset ATT orbital-mode state on vessel switch
        _lndgReentryRow3PeA = true;
        _lndgReentryRow0TPe = false;
        _lndgReentryRow1SL  = false;
        _drogueDeployed  = false;
        _mainDeployed    = false;
        _drogueCut       = false;
        _mainCut         = false;
        _drogueArmedSafe = false;
        _mainArmedSafe   = false;
        // Invalidate all row caches so everything redraws on the new vessel
        for (uint8_t s = 0; s < SCREEN_COUNT; s++) {
          for (uint8_t r = 0; r < ROW_COUNT; r++) {
            rowCache[s][r].value = "\x01";
            rowCache[s][r].fg    = 0x0001;
            rowCache[s][r].bg    = 0x0001;
          }
        }
        // Don't call contextScreen() here — state.vesselType is still the OLD vessel's
        // type at this point. FLIGHT_STATUS_MESSAGE with the new vessel's type will
        // arrive shortly; set a flag and do the context switch when it does.
        _pendingContextSwitch = true;
        // Request a full telemetry refresh so all fields repopulate immediately
        simpit.requestMessageOnChannel(0);
      } else if (msg[0] == 2) {
        // Docked
        if (debugMode) Serial.println(F("InfoDisp: Docked"));
        _vesselDocked = true;
        _dockedTimestamp = millis();
        // Force DOCK screen chrome to redraw so DOCKED splash appears
        if (activeScreen == screen_DOCK) switchToScreen(screen_DOCK);
      } else if (msg[0] == 3) {
        // Undocked
        if (debugMode) Serial.println(F("InfoDisp: Undocked"));
        _vesselDocked = false;
        // Force DOCK screen chrome to redraw so normal or NO TARGET state appears
        if (activeScreen == screen_DOCK) switchToScreen(screen_DOCK);
      }
      break;
  }
}


/***************************************************************************************
   INIT SIMPIT
   Connects to KSP via SerialUSB1 and registers all telemetry channels.
   Blocks until the handshake succeeds.
****************************************************************************************/
void initSimpit() {
  simpit.inboundHandler(onSimpitMessage);
  while (!simpit.init()) {
    if (debugMode) Serial.println(F("InfoDisp: Simpit handshake failed, retrying..."));
    delay(500);
  }
  if (debugMode) Serial.println(F("InfoDisp: Simpit connected."));
  simpitConnected = true;

  simpit.registerChannel(VESSEL_NAME_MESSAGE);
  simpit.registerChannel(SOI_MESSAGE);
  simpit.registerChannel(FLIGHT_STATUS_MESSAGE);
  simpit.registerChannel(ALTITUDE_MESSAGE);
  simpit.registerChannel(VELOCITY_MESSAGE);
  simpit.registerChannel(AIRSPEED_MESSAGE);
  simpit.registerChannel(APSIDES_MESSAGE);
  simpit.registerChannel(APSIDESTIME_MESSAGE);
  simpit.registerChannel(DELTAV_MESSAGE);
  simpit.registerChannel(BURNTIME_MESSAGE);
  simpit.registerChannel(ORBIT_MESSAGE);
  simpit.registerChannel(ROTATION_DATA_MESSAGE);
  simpit.registerChannel(MANEUVER_MESSAGE);
  simpit.registerChannel(TARGETINFO_MESSAGE);
  // INTERSECTS_MESSAGE omitted — KSP2 only, not available in KSP1
  simpit.registerChannel(ATMO_CONDITIONS_MESSAGE);
  simpit.registerChannel(ACTIONSTATUS_MESSAGE);
  simpit.registerChannel(SAS_MODE_INFO_MESSAGE);
  simpit.registerChannel(CAGSTATUS_MESSAGE);
  simpit.registerChannel(THROTTLE_CMD_MESSAGE);
  simpit.registerChannel(WHEEL_CMD_MESSAGE);
  simpit.registerChannel(ELECTRIC_MESSAGE);
  simpit.registerChannel(SCENE_CHANGE_MESSAGE);
  simpit.registerChannel(VESSEL_CHANGE_MESSAGE);
}
