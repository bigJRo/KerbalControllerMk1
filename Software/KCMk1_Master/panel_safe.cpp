/***************************************************************************************
   panel_safe.cpp — ABORT and STAGE.
****************************************************************************************/
#include "panel_safe.h"
#include "config.h"
#include "state.h"
#include "debug.h"
#include "ksp_out.h"

struct SafeLine {
  uint8_t  pin;
  bool     raw, stable;
  uint32_t rawSinceMs;
};

static SafeLine g_abort, g_stage;
static uint32_t g_lastStageMs;
static bool     g_stageFired;

bool safeLineLevel(bool stage) { return stage ? g_stage.stable : g_abort.stable; }

static void lineInit(SafeLine &l, uint8_t pin) {
  l.pin = pin;
  l.raw = l.stable = digitalReadFast(pin) == HIGH;   // the start-up level is not an edge
  l.rawSinceMs = millis();
}

// True on a debounced rising edge.
static bool lineRose(SafeLine &l, uint32_t now) {
  bool r = digitalReadFast(l.pin) == HIGH;
  if (r != l.raw) { l.raw = r; l.rawSinceMs = now; }
  if (l.raw == l.stable || now - l.rawSinceMs < SAFE_DEBOUNCE_MS) return false;
  l.stable = l.raw;
  return l.stable;
}

void safeLinesInit() {
  lineInit(g_abort, PIN_SAFE_ABORT);
  lineInit(g_stage, PIN_SAFE_STAGE);
  g_lastStageMs = 0;
  g_stageFired = false;
}

void safeLinesService() {
  uint32_t now = millis();
  if (lineRose(g_abort, now)) {
    bool sent = kspActionGroup(ABORT_ACTION, true);
    dbg(DBG_STATE, "ABORT%s", sent ? "" : (g_master.demo ? " (demo: not sent)" : " (KSP link down: not sent)"));
  }
  if (lineRose(g_stage, now)) {
    if (!(g_master.demo || g_vessel.flightScene)) {
      dbg(DBG_STATE, "STAGE ignored: not in a flight scene");
    } else if (g_stageFired && now - g_lastStageMs < SAFE_STAGE_LOCKOUT_MS) {
      dbg(DBG_STATE, "STAGE ignored: %lu ms after the last one", (unsigned long)(now - g_lastStageMs));
    } else {
      g_stageFired = true;
      g_lastStageMs = now;
      bool sent = kspActionGroup(STAGE_ACTION, true);
      dbg(DBG_STATE, "STAGE%s", sent ? "" : " (not sent)");
    }
  }
}
