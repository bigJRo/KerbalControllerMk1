/***************************************************************************************
   simpit_link.cpp — handshake, channel registration, inbound dispatch.
****************************************************************************************/
#include "simpit_link.h"
#include "config.h"
#include "state.h"
#include "debug.h"

static KerbalSimpit g_simpit(SerialUSB1);
static uint32_t g_lastTryMs;
static uint32_t g_attempts;

KerbalSimpit &simpitClient() { return g_simpit; }
bool     simpitConnected() { return g_master.kspConnected; }
uint32_t simpitAttempts()  { return g_attempts; }

/***************************************************************************************
   Inbound dispatch. One row per channel: the id, the payload size it must have, and
   its handler. A packet of the wrong size is counted and dropped, never half-used.
****************************************************************************************/
typedef void (*InboundFn)(const byte *msg);

static void onScene(const byte *msg) {
  bool flight = msg[0] == 0;                     // 0 = entering flight
  if (flight != g_vessel.flightScene) dbg(DBG_KSP, "scene: %s", flight ? "flight" : "not flight");
  g_vessel.flightScene = flight;
  g_vessel.actionsValid = false;                 // each flight scene starts without a status
  if (!flight) { g_vessel.twIndex = 0; g_vessel.inFlight = false; g_vessel.eva = false; return; }
  // The plugin sends the action groups on a change; ask for them now, so SWITCH ERR can
  // be checked from the start of the scene. (A request, not a command: ksp_out's gate is
  // for commands, and the link is up whenever this runs.)
  g_simpit.requestMessageOnChannel(ACTIONSTATUS_MESSAGE);
}

static void onActionStatus(const byte *msg) {
  if (!g_vessel.actionsValid || msg[0] != g_vessel.actions) dbg(DBG_KSP, "action groups %02X", msg[0]);
  g_vessel.actions = msg[0];
  g_vessel.actionsValid = true;
}

static void onFlightStatus(const byte *msg) {
  flightStatusMessage fs;
  memcpy(&fs, msg, sizeof fs);
  bool warp = fs.currentTWIndex > 0;
  if (warp != (g_vessel.twIndex > 0)) dbg(DBG_KSP, "time warp %s", warp ? "on" : "off");
  if (fs.isInEVA() != g_vessel.eva) dbg(DBG_KSP, "EVA %s", fs.isInEVA() ? "start" : "end");
  g_vessel.inFlight   = fs.isInFlight();
  g_vessel.eva        = fs.isInEVA();
  g_vessel.hasTarget  = fs.hasTarget();
  g_vessel.twIndex    = fs.currentTWIndex;
  g_vessel.situation  = fs.vesselSituation;
  g_vessel.vesselType = fs.vesselType;
}

static void onVesselChange(const byte *msg) {
  static const char *const kind[] = { "?", "switched", "docked", "undocked" };
  dbg(DBG_KSP, "vessel %s", msg[0] < 4 ? kind[msg[0]] : "?");
}

struct InboundRow { byte type; byte size; InboundFn fn; const char *name; };
static const InboundRow kInbound[] = {
  { SCENE_CHANGE_MESSAGE,  1,                           onScene,        "SCENE_CHANGE" },
  { FLIGHT_STATUS_MESSAGE, sizeof(flightStatusMessage), onFlightStatus, "FLIGHT_STATUS" },
  { VESSEL_CHANGE_MESSAGE, 1,                           onVesselChange, "VESSEL_CHANGE" },
  { ACTIONSTATUS_MESSAGE,  1,                           onActionStatus, "ACTIONSTATUS" },
};
static const uint8_t kInboundN = sizeof kInbound / sizeof kInbound[0];
static uint32_t g_rxCount[kInboundN], g_rxBad[kInboundN], g_rxLastMs[kInboundN];

static void inbound(byte type, byte msg[], byte size) {
  uint32_t now = millis();
  g_vessel.lastRxMs = now;
  g_vessel.rxCount++;
  for (uint8_t i = 0; i < kInboundN; i++) {
    if (kInbound[i].type != type) continue;
    if (size != kInbound[i].size) {
      g_rxBad[i]++;
      dbg(DBG_FROMKSP, "%s: %u bytes, expected %u — dropped", kInbound[i].name, size, kInbound[i].size);
      return;
    }
    g_rxCount[i]++;
    g_rxLastMs[i] = now;
    kInbound[i].fn(msg);
    return;
  }
}

static void registerChannels() {
  for (uint8_t i = 0; i < kInboundN; i++) g_simpit.registerChannel(kInbound[i].type);
}

static void cmdSimpit(const char *) {
  uint32_t now = millis();
  conPrintf("KSP link: %s, %lu attempts, %lu messages, last %lu ms ago%s",
            g_master.kspConnected ? "connected" : "down", (unsigned long)g_attempts,
            (unsigned long)g_vessel.rxCount,
            (unsigned long)(g_vessel.rxCount ? now - g_vessel.lastRxMs : 0),
            g_master.simpitLost ? " (SIMPIT LOST)" : "");
  conPrintf("  scene %s, in flight %d, EVA %d, warp %u", g_vessel.flightScene ? "flight" : "other",
            g_vessel.inFlight, g_vessel.eva, g_vessel.twIndex);
  for (uint8_t i = 0; i < kInboundN; i++)
    conPrintf("  %-14s %6lu rx  %4lu bad  last %lu ms ago", kInbound[i].name, (unsigned long)g_rxCount[i],
              (unsigned long)g_rxBad[i], (unsigned long)(g_rxCount[i] ? now - g_rxLastMs[i] : 0));
}

void simpitInit() {
  SerialUSB1.begin(115200);
  g_attempts = 0;
  g_lastTryMs = millis();
  memset(g_rxCount, 0, sizeof g_rxCount);
  memset(g_rxBad, 0, sizeof g_rxBad);
  memset(g_rxLastMs, 0, sizeof g_rxLastMs);
  consoleRegister("simpit", "KSP link, channels, message ages", cmdSimpit);
}

bool simpitAttempt() {
  g_attempts++;
  g_lastTryMs = millis();
  if (!g_simpit.init()) return false;
  g_master.kspConnected = true;
  g_vessel.lastRxMs = millis();
  g_vessel.actionsValid = false;
  g_simpit.inboundHandler(inbound);
  registerChannels();
  dbg(DBG_KSP, "connected (attempt %lu)", (unsigned long)g_attempts);
  return true;
}

void simpitService() {
  uint32_t now = millis();
  if (!g_master.kspConnected) {
    g_master.simpitLost = false;
    if (g_master.demo) return;                   // nothing to KSP in demo, not even a handshake
    if (now - g_lastTryMs < SIMPIT_RETRY_MS) return;
    simpitAttempt();
    return;
  }
  g_simpit.update();
  uint32_t silent = now - g_vessel.lastRxMs;
  bool lost = g_vessel.flightScene && silent >= SIMPIT_LOST_MS;
  if (lost != g_master.simpitLost) {
    if (lost) dbgErr(DBG_KSP, "nothing from KSP for %lu ms in flight", (unsigned long)silent);
    else      dbg(DBG_KSP, "KSP traffic resumed");
  }
  g_master.simpitLost = lost;
  if (g_vessel.flightScene && silent >= SIMPIT_REHANDSHAKE_MS && !g_master.demo) {
    dbgErr(DBG_KSP, "link silent for %lu ms: reconnecting", (unsigned long)silent);
    g_master.kspConnected = false;
    g_master.simpitLost = false;
    g_vessel.flightScene = false;
    g_lastTryMs = now - SIMPIT_RETRY_MS;         // retry on the next pass
  }
}
