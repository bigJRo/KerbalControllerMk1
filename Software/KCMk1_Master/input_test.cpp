/***************************************************************************************
   input_test.cpp — frames for Information Display 1's input test screen (input_test.h).
****************************************************************************************/
#include "input_test.h"
#include "config.h"
#include "state.h"
#include "debug.h"
#include "i2c_bus.h"
#include "carriers.h"
#include "module_bus.h"
#include "lever.h"
#include "panel_safe.h"
#include <KerbalModuleCommon.h>
#include <string.h>

static uint32_t g_lastInputsMs, g_lastModulesMs, g_encMs;
static int8_t   g_enc2;
static bool     g_wasOpen;

bool inputTestOpen() { return carrierInfoDisp1Screen() == INPUT_TEST_SCREEN; }

static int8_t pct(const uint8_t *be) {
  int32_t v = (int16_t)(be[0] << 8 | be[1]);
  v = v * 100 / 32767;
  return (int8_t)(v > 100 ? 100 : v < -100 ? -100 : v);
}

// The last packet of a module, if it has one; *recent = its press bits when it is fresh.
static uint8_t lastPacket(ModId id, uint8_t *p, uint8_t pressByte, uint8_t *recent) {
  uint32_t ms = 0;
  uint8_t n = moduleLastPacket(id, p, &ms);
  *recent = (n > pressByte && millis() - ms < INPUT_TEST_RECENT_MS) ? (uint8_t)(p[pressByte] & 0x0F) : 0;
  return n;
}

void inputTestBuildInputs(uint8_t *f) {
  memset(f, 0, INPUT_TEST_INPUTS_LEN);
  f[0] = SYNC_INPUT_TEST; f[1] = 0;
  uint16_t present = 0, active = 0, known = 0;
  for (uint8_t m = 0; m < MOD_COUNT; m++) {
    uint8_t p[16];
    if (modulePresent((ModId)m)) present |= (uint16_t)(1u << m);
    if (moduleLifecycle((ModId)m) == KMC_STATUS_ACTIVE) active |= (uint16_t)(1u << m);
    if (moduleInputs((ModId)m, nullptr) || moduleLastPacket((ModId)m, p) > 0) known |= (uint16_t)(1u << m);
  }
  f[2] = (uint8_t)present; f[3] = (uint8_t)(present >> 8);
  f[4] = (uint8_t)active;  f[5] = (uint8_t)(active >> 8);
  f[6] = (uint8_t)known;   f[7] = (uint8_t)(known >> 8);

  static const struct { ModId id; uint8_t bytes; } kButtons[] = {
    { MOD_UI, 2 }, { MOD_FUNCTION, 3 }, { MOD_ACTION, 2 }, { MOD_STABILITY, 2 },
    { MOD_VEHICLE, 3 }, { MOD_TIME, 2 }, { MOD_AUX, 2 },
  };
  uint8_t o = 8;
  for (auto &b : kButtons) {
    uint32_t lv = 0;
    moduleInputs(b.id, &lv);
    for (uint8_t i = 0; i < b.bytes; i++) f[o++] = (uint8_t)(lv >> (8 * i));
  }

  static const ModId kSticks[2] = { MOD_ROTATION, MOD_TRANSLATION };
  for (uint8_t s = 0; s < 2; s++) {
    uint8_t p[16], r;
    if (lastPacket(kSticks[s], p, 3, &r) >= KMC_JOYSTICK_PACKET_SIZE) {
      f[24 + 4 * s] = p[5];
      for (uint8_t a = 0; a < 3; a++) f[25 + 4 * s + a] = (uint8_t)pct(p + 6 + 2 * a);
    }
  }
  static const ModId kDisp[2] = { MOD_GPWS, MOD_PREWARP };
  for (uint8_t d = 0; d < 2; d++) {
    uint8_t p[16], r;
    if (lastPacket(kDisp[d], p, 3, &r) >= KMC_DISPLAY_PACKET_SIZE) f[32 + d] = (uint8_t)(r | (p[5] & 0x0F) << 4);
  }
  {
    LeverState l = leverState();
    uint8_t p[16], r = 0;
    lastPacket(MOD_THROTTLE, p, 4, &r);
    f[34] = l.known ? (uint8_t)((uint32_t)l.pos * 100 / LEVER_FULL) : 0xFF;
    f[35] = (uint8_t)((l.touched ? 1 : 0) | (l.precision ? 2 : 0) | (l.active ? 4 : 0) | r << 4);
  }
  {
    uint8_t p[16], r = 0;
    lastPacket(MOD_DUALENC, p, 3, &r);
    uint8_t g = (g_panel.ctrlGrp >= 1 && g_panel.ctrlGrp <= 6) ? g_panel.ctrlGrp : 1;
    f[36] = (uint8_t)((r & 0x03) | g << 4);
    f[37] = (uint8_t)g_enc2;
  }
  f[38] = g_panel.ctrlMode;
  f[39] = (uint8_t)((safeLineLevel(false) ? 1 : 0) | (safeLineLevel(true) ? 2 : 0));
}

void inputTestBuildModules(uint8_t *f) {
  memset(f, 0, INPUT_TEST_MODULES_LEN);
  f[0] = SYNC_INPUT_TEST; f[1] = 1;
  for (uint8_t m = 0; m < MOD_COUNT; m++) f[2 + m] = intLineOf(m);
  static const ModId kDisp[2] = { MOD_GPWS, MOD_PREWARP };
  for (uint8_t d = 0; d < 2; d++) {
    uint8_t p[16];
    uint16_t v = moduleLastPacket(kDisp[d], p) >= KMC_DISPLAY_PACKET_SIZE ? (uint16_t)(p[6] << 8 | p[7]) : 0;
    f[15 + 2 * d] = (uint8_t)v; f[16 + 2 * d] = (uint8_t)(v >> 8);
  }
}

// ENC2 count: the encoder's deltas are cleared on every read, so add up each new packet's.
static void trackEncoder() {
  uint8_t p[16];
  uint32_t ms = 0;
  if (moduleLastPacket(MOD_DUALENC, p, &ms) >= 7 && ms != g_encMs) { g_encMs = ms; g_enc2 = (int8_t)(g_enc2 + (int8_t)p[6]); }
}

void inputTestService() {
  trackEncoder();
  bool open = inputTestOpen();
  if (open != g_wasOpen) { dbg(DBG_DISPLAY, "input test screen %s", open ? "open" : "closed"); g_wasOpen = open; }
  if (!open) return;
  uint32_t now = millis();
  if (now - g_lastInputsMs >= INPUT_TEST_INPUTS_MS) {
    g_lastInputsMs = now;
    uint8_t f[INPUT_TEST_INPUTS_LEN];
    inputTestBuildInputs(f);
    busWrite(ADDR_INFODISP_1, f, INPUT_TEST_INPUTS_LEN);
  }
  if (now - g_lastModulesMs >= INPUT_TEST_MODULES_MS) {
    g_lastModulesMs = now;
    uint8_t f[INPUT_TEST_MODULES_LEN];
    inputTestBuildModules(f);
    busWrite(ADDR_INFODISP_1, f, INPUT_TEST_MODULES_LEN);
  }
}

void inputTestInit() {
  g_lastInputsMs = g_lastModulesMs = 0;
  g_encMs = 0; g_enc2 = 0; g_wasOpen = false;
}
