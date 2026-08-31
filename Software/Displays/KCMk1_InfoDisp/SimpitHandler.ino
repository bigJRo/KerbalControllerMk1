/***************************************************************************************
   SimpitHandler.ino -- KerbalSimpit message handling for Kerbal Controller Mk1 InfoDisp

   Channels subscribed:
     VESSEL_NAME, SOI, FLIGHT_STATUS, ALTITUDE, VELOCITY, AIRSPEED,
     APSIDES, APSIDESTIME, DELTAV, BURNTIME, ORBIT, ROTATION_DATA,
     MANEUVER, TARGETINFO, ATMO_CONDITIONS, ACTIONSTATUS, TEMP_LIMIT,
     CAGSTATUS, SAS_MODE_INFO, THROTTLE_CMD, WHEEL_CMD, ELECTRIC,
     SCENE_CHANGE, VESSEL_CHANGE
     (INTERSECTS is KSP2-only and intentionally not registered.)

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
/***************************************************************************************
   FLIGHT-SCENE ENTRY
   Shared by the explicit SCENE_CHANGE and by the inference in FLIGHT_STATUS below, so
   the two routes into a flight scene cannot drift apart.
****************************************************************************************/
static void enterFlightScene() {
  flightScene = true;
  demoMode    = false;
  // Entering a flight scene is a fresh start — release any held manual pick so the
  // panel opens on its context screen rather than wherever it was parked.
  clearManualScreenLatch();
  // Same for the reference overrides on the two attitude screens: a pinned SRF/ORB or
  // SL/RDR belongs to the flight it was pinned in.
  modeClearOverride(_scftVelRefOverride);
  modeClearOverride(_acftAltRefOverride);
  if (contextSwitchAllowed()) switchToScreen(contextScreen());
  simpit.requestMessageOnChannel(0);
}


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
      case TEMP_LIMIT_MESSAGE:      n = "TEMP_LIMIT";      break;
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

        // Simpit sends FLIGHT_STATUS only from a flight scene, so receiving it while
        // we believe we are not in one means the SCENE_CHANGE that would have told us
        // never arrived. That is the normal case when a panel boots into a flight
        // already in progress — SCENE_CHANGE is an event, not a state you can ask for,
        // so a display powered up (or USB re-enumerated) mid-flight would otherwise sit
        // on the standby splash until the pilot happened to change scene or vessel.
        // Adopting it here rather than on an earlier channel is deliberate: this
        // message's vessel data is applied just above, so the context ladder routes on
        // the real vessel instead of on default state.
        if (!flightScene) {
          if (debugMode) Serial.println(F("InfoDisp: flight scene inferred from FLIGHT_STATUS"));
          enterFlightScene();
        }

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
          // A held manual pick and self-pinning screens (RE-ENTRY) stay put — don't
          // let an auto route steal them.
          if (contextSwitchAllowed()) switchToScreen(contextScreen());
          // TARGETINFO may not have arrived yet — set flag to re-check for docking context
          // once target distance is known (catches switching to a vessel near a dock target)
          _pendingDockCheck = true;
        }

        // Pre-launch board. The plane/rover exclusion this used to carry existed only
        // because vessel-type routing sat above the pre-launch rule on a single
        // display: a spaceplane on the pad went to ACFT, so arming the board would
        // have set a mode for a screen the pilot could not see. The two ladders no
        // longer compete — pre-launch is a phase, answered on the mission panel — so
        // the board is armed for every vessel type on the pad, spaceplanes included.
        bool isPreLaunch = (state.situation & sit_PreLaunch) != 0;
        if (isPreLaunch && !_lnchPrelaunchMode && !_lnchPrelaunchDismissed) {
          _lnchPrelaunchMode = true;
          if (activeScreen == screen_LNCH) switchToScreen(screen_LNCH);
        } else if (!isPreLaunch && _lnchPrelaunchMode) {
          // Launched — clear everything including dismissed flag
          _lnchPrelaunchMode      = false;
          _lnchPrelaunchDismissed = false;
          _lnchOrbitalMode        = false;
          _lnchCoastLatched       = false;
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

    case TEMP_LIMIT_MESSAGE:
      // tempLimitMessage: byte tempLimitPercentage (internal/core), byte skinTempLimitPercentage.
      // Both are the vessel's hottest part as a % of its temperature limit (0-100).
      if (msgSize == sizeof(tempLimitMessage)) {
        tempLimitMessage t = parseMessage<tempLimitMessage>(msg);
        state.coreTempPct = t.tempLimitPercentage;
        state.skinTempPct = t.skinTempLimitPercentage;
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
        // m.deltaVTotal is the dV remaining in the maneuver PLAN (all nodes),
        // distinct from the vessel's total propellant dV (owned by DELTAV_MESSAGE).
        state.mnvrTotalDeltaV = m.deltaVTotal;
        state.mnvrHeading  = m.headingNextManeuver;
        state.mnvrPitch    = m.pitchNextManeuver;
      }
      break;

    // ── Target ───────────────────────────────────────────────────────────────────────

    case TARGETINFO_MESSAGE:
      if (msgSize == sizeof(targetMessage)) {
        targetMessage t = parseMessage<targetMessage>(msg);
        state.tgtDistance   = t.distance;
        state.tgtHeading    = t.heading;
        state.tgtPitch      = t.pitch;
        state.tgtVelHeading = t.velocityHeading;
        state.tgtVelPitch   = t.velocityPitch;

        // Closure rate, signed: negative = closing. Simpit does NOT send a signed
        // value -- KerbalSimpitRevamped's TargetInfo.cs sets
        //     myTargetInfo.velocity = FlightGlobals.ship_tgtVelocity.magnitude;
        // which is never negative. Taking it as signed left every consumer's
        // "closing" test false forever: T+INT (NAV), T+Int (TARGET) and T+Dock
        // (DOCKING) all read "---" permanently, and V.Close on TARGET and DOCKING
        // could never reach nominal green. Demo mode hid it, because Demo.ino writes
        // state.tgtVelocity directly with a signed sine and never comes through here.
        //
        // The magnitude is the speed along the whole relative velocity vector, so it
        // also overstates closure whenever the craft is not heading straight at the
        // target. Both directions arrive as navball heading/pitch pairs from the same
        // Simpit encoder, so projecting one onto the other recovers the true
        // line-of-sight rate. Only the ANGLE between the two matters, which makes this
        // insensitive to any axis-ordering difference between kspDirUnit's ENU frame
        // and Simpit's -- both vectors are built by the same function here.
        {
          float vHat[3], dHat[3];
          kspDirUnit(t.velocityHeading, t.velocityPitch, vHat);
          kspDirUnit(t.heading,         t.pitch,         dHat);
          const float cosT = vHat[0]*dHat[0] + vHat[1]*dHat[1] + vHat[2]*dHat[2];
          // ship_tgtVelocity is the craft's velocity relative to the target, so it
          // points along craft->target while closing: cosT > 0. Negate for the
          // panel-wide "negative = closing" convention.
          state.tgtVelocity = -t.velocity * cosT;
        }

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
          // Re-run the ladder now that target distance is known. The DOCK special
          // case the old code had here is redundant: the mission ladder tests the
          // same distance in rule 3, and on the vehicle-type unit DOCK is not a
          // destination at all, so routing there would have been wrong.
          // A held manual pick and self-pinning screens (RE-ENTRY) stay put.
          if (contextSwitchAllowed()) switchToScreen(contextScreen());
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
      if (msgSize != sizeof(cagStatusMessage)) break;   // guard: match the other handlers
      // Cast msg directly to cagStatusMessage and use is_action_activated(n).
      // parseCAGStatusMessage() is deprecated — use the struct directly.
      cagStatusMessage *cag = (cagStatusMessage *)msg;
      if (DROGUE_DEPLOY_CAG >= 1)
        state.drogueDeploy = cag->is_action_activated(DROGUE_DEPLOY_CAG);
      if (DROGUE_CUT_CAG   >= 1)
        state.drogueCut    = cag->is_action_activated(DROGUE_CUT_CAG);
      if (MAIN_DEPLOY_CAG  >= 1)
        state.mainDeploy   = cag->is_action_activated(MAIN_DEPLOY_CAG);
      if (MAIN_CUT_CAG     >= 1)
        state.mainCut      = cag->is_action_activated(MAIN_CUT_CAG);
      if (AIRBRAKE_CAG     >= 1)
        state.airbrake_on  = cag->is_action_activated(AIRBRAKE_CAG);
      break;
    }

    // ── Scene and vessel lifecycle ───────────────────────────────────────────────────

    case SCENE_CHANGE_MESSAGE:
      if (msgSize < 1) break;   // guard: single-byte payload
      // msg[0] == 0 → flight scene; msg[0] == 1 → non-flight (menu, tracking, etc.)
      if (debugMode)
        Serial.println((msg[0] == 0) ? F("InfoDisp: Entering flight scene")
                                     : F("InfoDisp: Leaving flight scene"));
      if (msg[0] == 0) {
        enterFlightScene();
      } else {
        flightScene = false;
        // Non-flight (menus, tracking station, etc.) — show standby splash
        demoMode = false;
        drawStandbyScreen(infoDisp);
        activeScreen = SCREEN_HOME;   // park on this panel's home screen behind the splash
        prevScreen   = screen_COUNT;
      }
      break;

    case VESSEL_CHANGE_MESSAGE:
      if (msgSize < 1) break;   // guard: single-byte payload
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
        _pfdManualOverride  = false;   // reset PFD title-cycle override; use context for new vessel
        clearManualScreenLatch();      // new vessel — release the held manual pick
        modeClearOverride(_scftVelRefOverride);   // and the held attitude references
        modeClearOverride(_acftAltRefOverride);
        _lndgReentryRow0TPe = false;
        _lndgReentryRow1SL  = false;
        _drogueDeployed  = false;
        _mainDeployed    = false;
        _drogueCut       = false;
        _mainCut         = false;
        _drogueArmedSafe = false;
        _mainArmedSafe   = false;
        // Invalidate all row caches so everything redraws on the new vessel
        invalidateAllRowCache();
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
  simpit.registerChannel(TEMP_LIMIT_MESSAGE);
  simpit.registerChannel(SCENE_CHANGE_MESSAGE);
  simpit.registerChannel(VESSEL_CHANGE_MESSAGE);
}
