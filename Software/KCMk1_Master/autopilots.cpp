/***************************************************************************************
   autopilots.cpp — runs and feeds the ported autopilots, applies the master's rules.
****************************************************************************************/
#include "autopilots.h"
#include "control_links.h"
#include "ascent_autopilot.h"
#include "hold_autopilot.h"
#include "burn_autopilot.h"
#include "landing_autopilot.h"
#include "infodisp_link.h"
#include "body_params.h"
#include "config.h"
#include "state.h"
#include "debug.h"
#include "interlocks.h"
#include "switches.h"
#include "hold.h"
#include <KerbalSimpit.h>
#include <math.h>
#include <string.h>

static float g_stageDv, g_stageBurn;               // DELTAV and BURNTIME feed one estimate together
static bool  g_hasTarget;                          // FLIGHT_STATUS, for TARGETINFO

bool autopilotsEngaged() { return apIsArmed() || hpAnyEngaged() || bpAnyEngaged() || lpAnyEngaged(); }

/***************************************************************************************
   Drop notices (interlocks.h)
****************************************************************************************/
static void dropThrottleModes() {
  if (apIsArmed()) { apDisarm(); kspPrint("Ascent AP: ENGINE SAFE - disarmed"); }
  hpEngage(HP_MODE_IAS, false);
  hpEngage(HP_MODE_MACH, false);
  hpEngage(HP_MODE_CRUISE, false);
  hpEngage(HP_MODE_FOLLOW, false);
  if (bpArmed() || bpExecuting()) bpAbort(HP_REASON_ENG_SAFE);
  lpEngage(LP_MODE_DESC, false);
  lpEngage(LP_MODE_HOVR, false);
  lpEngage(LP_MODE_BRAKE, false);
}

static void onDrop(DropReason why) {
  switch (why) {
    case DROP_AP_INHIB:
      if (holdUsesHoldAutopilot()) return;         // HOLD's own hold modes are not the consoles'
      if (autopilotsEngaged()) arbAllOffReason(HP_REASON_AP_INHIB);
      break;
    case DROP_ENG_SAFE:   dropThrottleModes(); break;
    case DROP_AP_DISC:    arbAllOffReason(HP_REASON_AP_DISC); break;
    case DROP_DEMO:
    case DROP_MSTR_RESET: arbAllOffReason(HP_REASON_PILOT); break;
  }
}

/***************************************************************************************
   Telemetry: every inbound message, as Controller_Main's handler fed it
****************************************************************************************/
template <class T> static bool take(T &out, const byte *msg, byte size) {
  if (size != sizeof(T)) return false;
  memcpy(&out, msg, sizeof(T));
  return true;
}

void autopilotsIngest(byte type, const byte *msg, byte size) {
  switch (type) {
    case ACTIONSTATUS_MESSAGE:
      if (size == 1) hpIngestBrakes((msg[0] & BRAKES_ACTION) != 0);
      break;
    case FLIGHT_STATUS_MESSAGE: {
      flightStatusMessage m;
      if (!take(m, msg, size)) break;
      apIngestFlightStatus(m.isInFlight());
      g_hasTarget = m.hasTarget();
      hpIngestFlightStatus(m.vesselType, m.vesselSituation, m.hasTarget());
      lpIngestFlightStatus(m.vesselType, m.vesselSituation);
      break;
    }
    case SCENE_CHANGE_MESSAGE:
      if (size >= 1 && msg[0] != 0) { hpVesselChanged(); bpVesselChanged(); lpVesselChanged(); }   // leaving flight
      break;
    case VESSEL_CHANGE_MESSAGE:
      if (size >= 1 && msg[0] == 1) { hpVesselChanged(); bpVesselChanged(); lpVesselChanged(); }   // switched vessel
      break;
    case THROTTLE_CMD_MESSAGE: {
      throttleMessage m;
      if (!take(m, msg, size)) break;
      hpIngestThrottle((float)m.throttle / 32767.0f);
      lpIngestThrottle((float)m.throttle / 32767.0f);
      break;
    }
    case ALTITUDE_MESSAGE: {
      altitudeMessage m;
      if (!take(m, msg, size)) break;
      apIngestAltitude(m.sealevel, m.surface);
      hpIngestAltitude(m.sealevel);
      lpIngestAltitude(m.sealevel, m.surface);
      break;
    }
    case VELOCITY_MESSAGE: {
      velocityMessage m;
      if (!take(m, msg, size)) break;
      apIngestVelocity(m.orbital, m.surface, m.vertical);
      hpIngestVelocity(m.surface, m.vertical);
      lpIngestVelocity(m.surface, m.vertical);
      bpIngestVelocity(m.orbital);
      break;
    }
    case AIRSPEED_MESSAGE: {
      airspeedMessage m;
      if (!take(m, msg, size)) break;
      apIngestGForce(m.gForces);
      hpIngestAirspeed(m.IAS, m.mach);
      lpIngestAirspeed(m.mach);
      aeIngestGForce(m.gForces);
      break;
    }
    case APSIDES_MESSAGE: {
      apsidesMessage m;
      if (!take(m, msg, size)) break;
      apIngestApsides(m.apoapsis, m.periapsis);
      bpIngestApsides(m.apoapsis, m.periapsis);
      break;
    }
    case APSIDESTIME_MESSAGE: {
      apsidesTimeMessage m;
      if (!take(m, msg, size)) break;
      apIngestApsidesTime((float)m.apoapsis, (float)m.periapsis);
      bpIngestApsidesTime((float)m.apoapsis, (float)m.periapsis);
      break;
    }
    case ORBIT_MESSAGE: {
      orbitInfoMessage m;
      if (!take(m, msg, size)) break;
      apIngestOrbit(m.inclination);
      bpIngestOrbit(m.eccentricity, m.semiMajorAxis, m.inclination, m.longAscendingNode, m.argPeriapsis,
                    m.trueAnomaly, m.period);
      break;
    }
    case ROTATION_DATA_MESSAGE: {
      vesselPointingMessage m;
      if (!take(m, msg, size)) break;
      apIngestAttitude(m.heading, m.pitch, m.roll, m.surfaceVelocityHeading, m.surfaceVelocityPitch,
                       m.orbitalVelocityHeading, m.orbitalVelocityPitch);
      hpIngestAttitude(m.heading, m.pitch, m.roll, m.surfaceVelocityHeading, m.surfaceVelocityPitch);
      bpIngestAttitude(m.heading, m.pitch, m.roll, m.orbitalVelocityHeading, m.orbitalVelocityPitch);
      lpIngestAttitude(m.heading, m.pitch, m.roll, m.orbitalVelocityHeading, m.orbitalVelocityPitch);
      break;
    }
    case DELTAV_MESSAGE: {
      deltaVMessage m;
      if (!take(m, msg, size)) break;
      g_stageDv = m.stageDeltaV;
      apIngestStageDeltaV(m.stageDeltaV);
      aeIngestStage(m.stageDeltaV, g_stageBurn);
      break;
    }
    case BURNTIME_MESSAGE: {
      burnTimeMessage m;
      if (!take(m, msg, size)) break;
      g_stageBurn = m.stageBurnTime;
      aeIngestStage(g_stageDv, m.stageBurnTime);
      break;
    }
    case TEMP_LIMIT_MESSAGE: {
      tempLimitMessage m;
      if (!take(m, msg, size)) break;
      apIngestSkinTemp(m.skinTempLimitPercentage / 100.0f);
      break;
    }
    case SOI_MESSAGE: {
      char body[24];
      uint8_t n = 0;
      for (uint8_t i = 0; i < size && n < sizeof body - 1; i++) body[n++] = (char)msg[i];
      body[n] = '\0';
      apIngestSOI(body);
      BodyParams b = getBodyParams(body);
      bpIngestBody(b.radius, b.gravity, body);
      lpIngestBody(b.gravity, b.flyHigh, body);
      break;
    }
    case ATMO_CONDITIONS_MESSAGE: {
      atmoConditionsMessage m;
      if (!take(m, msg, size)) break;
      apIngestAtmo(m.airDensity, m.hasAtmosphere(), m.isVesselInAtmosphere());
      hpIngestAtmo(m.hasAtmosphere(), m.isVesselInAtmosphere());
      lpIngestAtmo(m.airDensity, m.isVesselInAtmosphere());
      aeIngestAtmo(m.isVesselInAtmosphere());
      break;
    }
    case TARGETINFO_MESSAGE: {
      targetMessage m;
      if (!take(m, msg, size)) break;
      // Simpit's target velocity is the magnitude of the whole relative velocity: project
      // it on the line of sight for a signed closing rate (> 0 = opening).
      const float d2r = 0.0174532925199f;
      float p = m.pitch * d2r, h = m.heading * d2r;
      float losN = cosf(p) * cosf(h), losE = cosf(p) * sinf(h), losU = sinf(p);
      p = m.velocityPitch * d2r; h = m.velocityHeading * d2r;
      float vN = cosf(p) * cosf(h), vE = cosf(p) * sinf(h), vU = sinf(p);
      float closing = m.velocity * (losN * vN + losE * vE + losU * vU);
      hpIngestTarget(g_hasTarget, m.heading, m.pitch, m.distance, closing);
      bpIngestTarget(g_hasTarget, m.distance, m.velocity, m.heading, m.pitch, m.velocityHeading, m.velocityPitch);
      break;
    }
    case MANEUVER_MESSAGE: {
      maneuverMessage m;
      if (!take(m, msg, size)) break;
      bpIngestNode(m.timeToNextManeuver, m.deltaVNextManeuver, m.durationNextManeuver, m.headingNextManeuver,
                   m.pitchNextManeuver);
      break;
    }
  }
}

void autopilotsChannels(void (*reg)(byte channel)) {
  static const byte kCh[] = {
    ALTITUDE_MESSAGE, VELOCITY_MESSAGE, AIRSPEED_MESSAGE, APSIDES_MESSAGE, APSIDESTIME_MESSAGE,
    ROTATION_DATA_MESSAGE, TARGETINFO_MESSAGE, DELTAV_MESSAGE, BURNTIME_MESSAGE, TEMP_LIMIT_MESSAGE,
    SOI_MESSAGE, THROTTLE_CMD_MESSAGE,
  };
  for (byte c : kCh) reg(c);
}

/***************************************************************************************
   Service and consoles
****************************************************************************************/
static void conLine(bool (*fn)(const char *), const char *args, const char *name) {
  while (*args == ' ') args++;
  if (!fn(args)) conPrintf("%s: not understood (STATUS lists the state)", name);
}
static void cmdAp(const char *a) { conLine(apConsoleLine, a, "ap"); }
static void cmdHp(const char *a) { conLine(hpConsoleLine, a, "hp"); }
static void cmdBp(const char *a) { conLine(bpConsoleLine, a, "bp"); }
static void cmdLp(const char *a) { conLine(lpConsoleLine, a, "lp"); }

void autopilotsInit() {
  apInit();
  hpInit();
  bpInit();
  lpInit();
  arbInit();
  idlInit();
  g_stageDv = g_stageBurn = 0;
  g_hasTarget = false;
  interlockOnDrop(onDrop);
  consoleRegister("ap", "ascent autopilot: ap STATUS | ARM | DISARM | ALT m | ...", cmdAp);
  consoleRegister("hp", "hold autopilot: hp STATUS | ENG ALT | SET IAS 180 | OFF | ...", cmdHp);
  consoleRegister("bp", "burn autopilot: bp STATUS | ARM NODE | EXEC | OFF | ...", cmdBp);
  consoleRegister("lp", "landing autopilot: lp STATUS | ENG DESC | OFF | ...", cmdLp);
}

void autopilotsService() {
  bool live = !g_master.demo && g_master.kspConnected && g_vessel.flightScene;
  if (live) {
    apUpdate();
    hpUpdate();
    bpUpdate();
    lpUpdate();
  } else if (autopilotsEngaged()) {
    arbAllOffReason(HP_REASON_PILOT);              // no flight, no autopilot
  }
  idlService();
  bool any = autopilotsEngaged();
  g_panel.autopilot = any || holdEngaged();
  switchErrSuppress(SW_SAS, SUPP_CONSOLE_AP, any);
}
