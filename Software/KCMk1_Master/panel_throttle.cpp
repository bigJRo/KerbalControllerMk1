/***************************************************************************************
   panel_throttle.cpp — Throttle Module (0x2C): the lever (lever.h) and its four buttons
   (Inventory §7.2, design §8.9).

     100 %  lever to full          UP    lever up THR_BUTTON_STEP
     0 %    lever to 0             DOWN  lever down THR_BUTTON_STEP
   The buttons move the lever (the module reports them; it does not act on them); the
   throttle channel then follows the lever like any pilot input (ksp_frame.cpp). They
   are ignored while the ENGINE is not ARMED, while an autopilot owns the throttle, and
   in FINE (the module's precision mode positions the lever relative to its anchor, so
   a lever target would not be the throttle asked for). Demo: they move the lever.
****************************************************************************************/
#include "panels.h"
#include "lever.h"
#include "debug.h"
#include "interlocks.h"
#include "ksp_frame.h"
#include <KerbalModuleCommon.h>

static const uint16_t THR_BUTTON_STEP = 3277;     // 10 %

enum : uint8_t { THR_B100 = 0x01, THR_BUP = 0x02, THR_BDOWN = 0x04, THR_B00 = 0x08 };

LeverState leverState() {
  LeverState l = {};
  uint8_t p[KMC_THROTTLE_PACKET_SIZE];
  if (!moduleLastPacket(MOD_THROTTLE, p, &l.ms)) return l;
  l.known     = true;
  l.active    = moduleLifecycle(MOD_THROTTLE) == KMC_STATUS_ACTIVE;
  l.precision = (p[3] & 0x02) != 0;
  l.touched   = (p[3] & 0x04) != 0;
  l.pos       = (uint16_t)(p[5] << 8 | p[6]);
  if (l.pos > LEVER_FULL) l.pos = LEVER_FULL;
  return l;
}

bool leverCommand(uint16_t pos) {
  if (pos > LEVER_FULL) pos = LEVER_FULL;
  uint8_t b[2] = { (uint8_t)(pos >> 8), (uint8_t)(pos & 0xFF) };
  return moduleCommand(MOD_THROTTLE, KMC_CMD_SET_THROTTLE, b, 2);
}

bool leverHold(bool on) {
  uint8_t b = on ? 1 : 0;
  return moduleCommand(MOD_THROTTLE, KMC_CMD_SET_LEVER_HOLD, &b, 1);
}

static void onEvent(const ModEvent &e) {
  panelNoteEvent(e);
  if (!panelsEnabled() || !(panelLive() || panelDemo())) return;
  uint8_t b = (uint8_t)e.pressed;
  if (!b) return;
  const char *why = !interlockThrottleLive() ? "ENGINE not armed" : thrAutoOwned() ? "autopilot has the throttle" : nullptr;
  LeverState l = leverState();
  if (!why && l.precision) why = "FINE";
  if (why) { dbg(DBG_OWNER, "throttle button ignored: %s", why); return; }
  int32_t t = l.pos;
  if (b & THR_B100) t = LEVER_FULL;
  else if (b & THR_B00) t = 0;
  else if (b & THR_BUP) t += THR_BUTTON_STEP;
  else if (b & THR_BDOWN) t -= THR_BUTTON_STEP;
  if (t < 0) t = 0;
  if (t > LEVER_FULL) t = LEVER_FULL;
  leverCommand((uint16_t)t);
  dbg(DBG_OWNER, "throttle button: lever to %ld", (long)t);
}

void panelThrottleInit() { moduleBusSetHandler(MOD_THROTTLE, onEvent); }
