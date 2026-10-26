/***************************************************************************************
   interlocks.cpp — ENGINE and A/P interlocks, drop notices.
****************************************************************************************/
#include "interlocks.h"
#include "config.h"
#include "state.h"
#include "debug.h"
#include "ksp_out.h"
#include "module_bus.h"
#include <KerbalModuleCommon.h>

static const uint8_t MAX_LISTENERS = 6;
static DropFn      g_listener[MAX_LISTENERS];
static uint8_t     g_listenerN;

static EngineState g_eng;
static uint32_t    g_armedAtMs;                    // when ARMING began
static bool        g_zeroPending;                  // 0 % throttle to KSP still to send
static bool        g_wasFlight;
static uint32_t    g_lastLeverCmdMs, g_lastPrecCmdMs;

static const char *const kEngName[] = { "SAFE", "ARMING", "ARMED" };

const char *dropReasonName(DropReason why) {
  static const char *const n[] = { "A/P INHIB", "ENG SAFE", "DEMO", "MSTR RESET" };
  return why <= DROP_MSTR_RESET ? n[why] : "?";
}

bool interlockOnDrop(DropFn fn) {
  if (g_listenerN >= MAX_LISTENERS) return false;
  g_listener[g_listenerN++] = fn;
  return true;
}

void interlockDrop(DropReason why) {
  dbg(DBG_AP, "drop: %s", dropReasonName(why));
  for (uint8_t i = 0; i < g_listenerN; i++) g_listener[i](why);
}

EngineState interlockEngineState() { return g_eng; }
bool        interlockThrottleLive() { return g_eng == ENG_ARMED; }
bool        interlockEngineSafe()   { return !g_panel.engArm; }

static void setEng(EngineState s) {
  if (s == g_eng) return;
  dbg(DBG_OWNER, "ENGINE %s -> %s", kEngName[g_eng], kEngName[s]);
  g_eng = s;
}

void interlockSetEngine(bool arm, bool firstRead) {
  g_panel.engArm = arm;
  if (firstRead) {                                 // no flip: take the switch as it is
    setEng(arm ? ENG_ARMED : ENG_SAFE);
    g_zeroPending = !arm;
    return;
  }
  dbg(DBG_STATE, "ENGINE switch %s", arm ? "ARM" : "SAFE");
  g_zeroPending = true;
  g_lastLeverCmdMs = millis() - ENG_LEVER_CMD_MS;  // drive the lever on this pass
  if (arm) {
    setEng(ENG_ARMING);
    g_armedAtMs = millis();
    moduleRequestRead(MOD_THROTTLE);
  } else {
    setEng(ENG_SAFE);
    interlockDrop(DROP_ENG_SAFE);
  }
}

void interlockSetApArm(bool arm, bool firstRead) {
  bool was = g_panel.apArm;
  g_panel.apArm = arm;
  if (firstRead || arm == was) return;
  dbg(DBG_STATE, "A/P switch %s", arm ? "ARM" : "INHIB");
  if (!arm) interlockDrop(DROP_AP_INHIB);
}

/***************************************************************************************
   Throttle module: lever position, touch and precision from its last packet
   (I2C spec §9.4: [3] flags bit0 enabled, bit1 precision, bit2 touched; [5..6] BE value)
****************************************************************************************/
struct Lever { bool known, active, precision, touched; uint16_t pos; uint32_t ms; };

static Lever lever() {
  Lever l = {};
  uint8_t p[KMC_THROTTLE_PACKET_SIZE];
  if (!moduleLastPacket(MOD_THROTTLE, p, &l.ms)) return l;
  l.known     = true;
  l.active    = moduleLifecycle(MOD_THROTTLE) == KMC_STATUS_ACTIVE;
  l.precision = (p[3] & 0x02) != 0;
  l.touched   = (p[3] & 0x04) != 0;
  l.pos       = (uint16_t)(p[5] << 8 | p[6]);
  return l;
}

static void throttleService() {
  uint32_t now = millis();
  Lever l = lever();
  bool present = modulePresent(MOD_THROTTLE);

  // The lever to 0 % while SAFE or ARMING. A DISABLED module holds its lever at 0 % by
  // itself and refuses lever commands, so only an ACTIVE one is driven.
  if (g_eng != ENG_ARMED && l.known && l.active && l.pos > ENG_LEVER_ZERO && !l.touched &&
      now - g_lastLeverCmdMs >= ENG_LEVER_CMD_MS) {
    uint8_t zero[2] = { 0, 0 };
    if (moduleCommand(MOD_THROTTLE, KMC_CMD_SET_THROTTLE, zero, 2)) dbg(DBG_OWNER, "lever to 0 %% (at %u)", l.pos);
    g_lastLeverCmdMs = now;
  }

  // ARMING ends when the lever reports 0 % after the flip (or there is no lever).
  if (g_eng == ENG_ARMING) {
    if (!present) { dbg(DBG_OWNER, "no Throttle module: armed"); setEng(ENG_ARMED); }
    else if (l.known && l.active && (int32_t)(l.ms - g_armedAtMs) >= 0 && l.pos <= ENG_LEVER_ZERO) setEng(ENG_ARMED);
  }

  // Precision follows THRTL FINE only while ARMED.
  bool wantPrec = g_panel.thrPrec && g_eng == ENG_ARMED;
  if (l.known && l.active && l.precision != wantPrec && now - g_lastPrecCmdMs >= ENG_LEVER_CMD_MS) {
    uint8_t p = wantPrec ? 1 : 0;
    if (moduleCommand(MOD_THROTTLE, KMC_CMD_SET_PRECISION, &p, 1)) dbg(DBG_OWNER, "throttle precision %s", wantPrec ? "on" : "off");
    g_lastPrecCmdMs = now;
  }

  // 0 % to KSP: on SAFE, on ARM, and on entering a flight scene before ARMED. Stage 5's
  // per-frame throttle merge sends 0 % continuously while the engine is not ARMED.
  bool flight = g_vessel.flightScene;
  if (flight && !g_wasFlight && g_eng != ENG_ARMED) g_zeroPending = true;
  g_wasFlight = flight;
  if (g_zeroPending && flight && kspAllowed()) {
    kspThrottle(0);
    g_zeroPending = false;
  }
}

void interlockService() { throttleService(); }

static void cmdInterlocks(const char *) {
  Lever l = lever();
  conPrintf("ENGINE %s (switch %s), throttle inputs %s", kEngName[g_eng], g_panel.engArm ? "ARM" : "SAFE",
            interlockThrottleLive() ? "taken" : "ignored");
  if (l.known) conPrintf("  lever %u%s%s%s", l.pos, l.active ? "" : " (module not active)", l.touched ? " touched" : "",
                         l.precision ? " precision" : "");
  else         conPrintf("  lever: no packet yet");
  conPrintf("A/P %s; %u drop listener(s)", g_panel.apArm ? "ARM" : "INHIB", g_listenerN);
}

void interlocksInit() {
  g_listenerN = 0;
  g_eng = ENG_SAFE;
  g_zeroPending = true;
  g_wasFlight = false;
  g_lastLeverCmdMs = g_lastPrecCmdMs = millis();
  g_armedAtMs = 0;
  consoleRegister("interlocks", "ENGINE and A/P interlocks, lever", cmdInterlocks);
}
