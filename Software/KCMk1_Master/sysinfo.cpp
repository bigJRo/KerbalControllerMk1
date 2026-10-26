/***************************************************************************************
   sysinfo.cpp — Sys Info Display link.

   Boot lines sent before the display has answered are kept (the last SI_REPLAY lines)
   and replayed when it does, so a display that boots slower than the master still
   shows the whole boot from the top.
****************************************************************************************/
#include "sysinfo.h"
#include "config.h"
#include "state.h"
#include "i2c_bus.h"
#include "debug.h"

static const uint8_t SI_REPLAY = 48;   // a whole boot, with every module listed

struct SiLine { uint8_t attr; char text[SI_TEXT_MAX + 1]; };

static bool     g_present;
static bool     g_bootDone;
static uint8_t  g_seq;
static SiLine   g_replay[SI_REPLAY];
static uint8_t  g_replayN;
static bool     g_replayOverflow;
static uint32_t g_lastCtlMs, g_lastStatusMs, g_lastProbeMs;
static SiStatus g_status;
static SiSwitchErr g_swErr[SI_SWERR_MAX];
static uint8_t  g_swErrN;
static bool     g_swErrDirty;

bool siPresent() { return g_present; }

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8); }

static bool siWrite(const uint8_t *buf, uint8_t n) {
  if (!g_present) return false;
  if (busWrite(ADDR_SYSINFO, buf, n) == BUS_OK) return true;
  g_present = false;               // stop sending; re-probed every SYSINFO_REPROBE_MS
  g_master.sysInfoPresent = false;
  dbgErr(DBG_DISPLAY, "Sys Info Display stopped answering");
  return false;
}

static uint8_t textLen(const char *t) {
  uint8_t n = 0;
  while (n < SI_TEXT_MAX && t[n]) n++;
  return n;
}

static void sendLine(uint8_t attr, const char *text) {
  uint8_t len = textLen(text);
  uint8_t f[4 + SI_TEXT_MAX];
  f[0] = SI_FRAME_BOOT_LINE; f[1] = ++g_seq; f[2] = attr; f[3] = len;
  memcpy(f + 4, text, len);
  siWrite(f, (uint8_t)(4 + len));
}

static void sendControl() {
  uint8_t ctrl = 0;
  if (g_master.debug) ctrl |= 0x01;
  if (g_master.demo)  ctrl |= 0x02;
  uint8_t f[2] = { SI_FRAME_CONTROL, ctrl };
  siWrite(f, 2);
}

static bool readStatus(uint8_t *flags) {
  uint8_t b[SI_READ_LEN];
  if (busRead(ADDR_SYSINFO, b, SI_READ_LEN) != BUS_OK || b[0] != SYNC_SYSINFO) return false;
  if (flags) *flags = b[1];
  return true;
}

void siInit() {
  g_present = g_bootDone = g_replayOverflow = false;
  g_seq = g_replayN = g_swErrN = 0;
  g_swErrDirty = false;
  memset(&g_status, 0, sizeof g_status);
  g_lastCtlMs = g_lastStatusMs = g_lastProbeMs = millis();
}

static void replay() {
  if (g_replayOverflow) sendLine(SEV_WARN, "(earlier boot lines dropped)");
  for (uint8_t i = 0; i < g_replayN && g_present; i++) sendLine(g_replay[i].attr, g_replay[i].text);
}

bool siProbeBoot(uint32_t timeoutMs) {
  uint32_t t0 = millis();
  do {
    if (readStatus(nullptr)) {
      g_present = true;
      g_master.sysInfoPresent = true;
      replay();
      return true;
    }
    delay(50);
  } while (millis() - t0 < timeoutMs);
  return false;
}

void siBootLine(BootSev sev, bool replace, const char *text) {
  uint8_t attr = (uint8_t)((sev & 0x03) | (replace ? 0x80 : 0));
  // Keep a replay copy. A REPLACE line overwrites the last kept line, as on screen.
  if (replace && g_replayN) g_replayN--;
  if (g_replayN == SI_REPLAY) {
    memmove(g_replay, g_replay + 1, sizeof(SiLine) * (SI_REPLAY - 1));
    g_replayN--;
    g_replayOverflow = true;
  }
  g_replay[g_replayN].attr = attr;
  strncpy(g_replay[g_replayN].text, text, SI_TEXT_MAX);
  g_replay[g_replayN].text[SI_TEXT_MAX] = 0;
  g_replayN++;
  sendLine(attr, text);
}

void siBootDone() {
  g_bootDone = true;
  uint8_t f[1] = { SI_FRAME_BOOT_DONE };
  siWrite(f, 1);
}

void siSetStatus(const SiStatus &s) { g_status = s; }

void siSetSwitchErrors(const SiSwitchErr *list, uint8_t n) {
  if (n > SI_SWERR_MAX) n = SI_SWERR_MAX;
  if (n == g_swErrN && memcmp(list, g_swErr, n * sizeof(SiSwitchErr)) == 0) return;
  memcpy(g_swErr, list, n * sizeof(SiSwitchErr));
  g_swErrN = n;
  g_swErrDirty = true;
}

static void sendStatus() {
  uint8_t f[SI_STATUS_LEN];
  f[0] = SI_FRAME_STATUS;
  f[1] = (uint8_t)((g_status.powerValid ? 1 : 0) | (g_status.tempValid ? 2 : 0) | (g_status.fanValid ? 4 : 0));
  put16(f + 2, (uint16_t)(g_status.busV * 100.0f + 0.5f));
  float mA = g_status.currentA * 1000.0f;
  put16(f + 4, (uint16_t)(int16_t)(mA + (mA < 0 ? -0.5f : 0.5f)));   // rounded, not truncated
  put16(f + 6, (uint16_t)(g_status.powerW * 10.0f + 0.5f));
  f[8] = (uint8_t)g_status.tempC;
  put16(f + 9, g_status.fanRpm);
  f[11] = (uint8_t)((g_master.kspConnected && !g_master.simpitLost ? SI_LINK_MASTER : 0) | g_master.displayLinks);
  f[12] = (uint8_t)(SI_LINK_MASTER | g_master.displaysKnown);
  siWrite(f, SI_STATUS_LEN);
}

static void sendSwitchErrors() {
  uint8_t f[2 + 3 * SI_SWERR_MAX];
  f[0] = SI_FRAME_SWITCH_ERR; f[1] = g_swErrN;
  for (uint8_t i = 0; i < g_swErrN; i++) {
    f[2 + 3 * i] = g_swErr[i].id; f[3 + 3 * i] = g_swErr[i].expected; f[4 + 3 * i] = g_swErr[i].actual;
  }
  if (siWrite(f, (uint8_t)(2 + 3 * g_swErrN))) g_swErrDirty = false;
}

void siService() {
  uint32_t now = millis();
  if (!g_present) {
    if (now - g_lastProbeMs < SYSINFO_REPROBE_MS) return;
    g_lastProbeMs = now;
    if (!readStatus(nullptr)) return;
    g_present = true;
    g_master.sysInfoPresent = true;
    dbg(DBG_DISPLAY, "Sys Info Display answered");
    if (g_bootDone) {                // late: skip the boot log, go straight to status
      siBootDone();
      g_swErrDirty = true;
    } else {
      replay();
    }
  }
  if (now - g_lastCtlMs >= SYSINFO_STATUS_MS) {
    g_lastCtlMs = now;
    sendControl();
  }
  if (g_bootDone && now - g_lastStatusMs >= SYSINFO_STATUS_MS) {
    g_lastStatusMs = now;
    sendStatus();
  }
  if (g_bootDone && g_swErrDirty) sendSwitchErrors();
}
