/***************************************************************************************
   panel_encoders.cpp — Dual Encoder (0x2D) (Inventory §10.8, design §8.9).

     ENC1         control group 1–6, wrapping (the Annunciator shows it); works in demo
     ENC1 button  control group back to 1
     ENC2         next / previous external camera: '=' / '-' (Hullcam VDS), one key per
                  detent, at most ENC_KEYS_MAX per read
     ENC2 button  camera reset: Backspace (Hullcam VDS)
   Packet (I2C spec §9.5): [3] button rising edges (bit 0 ENC1, bit 1 ENC2), [5] ENC1
   delta, [6] ENC2 delta (signed, accumulated since the last read).
****************************************************************************************/
#include "panels.h"
#include "state.h"
#include "debug.h"
#include "keys.h"
#include "cag.h"

static const uint8_t ENC_KEYS_MAX = 5;

static void setGroup(uint8_t g) {
  if (g == g_panel.ctrlGrp) return;
  g_panel.ctrlGrp = g;
  dbg(DBG_STATE, "control group %u (CAG offset %u)", g, (g - 1) * CAG_GROUP_STRIDE);
}

static void onEvent(const ModEvent &e) {
  panelNoteEvent(e);
  if (!panelsEnabled() || !e.pkt) return;
  int8_t d1 = (int8_t)e.pkt[5], d2 = (int8_t)e.pkt[6];
  if (d1) {
    int g = (g_panel.ctrlGrp - 1 + d1) % CAG_GROUPS;
    if (g < 0) g += CAG_GROUPS;
    setGroup((uint8_t)(g + 1));
  }
  if (e.pressed & 0x01) setGroup(1);
  if (!panelLive()) return;                        // the camera keys go to KSP
  uint8_t n = (uint8_t)(d2 < 0 ? -d2 : d2);
  if (n > ENC_KEYS_MAX) n = ENC_KEYS_MAX;
  for (uint8_t i = 0; i < n; i++) keyTap(d2 > 0 ? VK_OEM_PLUS : VK_OEM_MINUS);
  if (e.pressed & 0x02) keyTap(VK_BACKSPACE);
}

void panelEncodersInit() { moduleBusSetHandler(MOD_DUALENC, onEvent); }
