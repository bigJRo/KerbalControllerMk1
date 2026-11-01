/***************************************************************************************
   carriers.cpp — display carrier commands and status reads.
****************************************************************************************/
#include "carriers.h"
#include "config.h"
#include "state.h"
#include "i2c_bus.h"
#include "debug.h"
#include "boot.h"
#include "sysinfo.h"
#include <stdio.h>

static const uint8_t REQ_STATUS     = 0x1;
static const uint8_t REQ_PROCEED    = 0x2;
static const uint8_t REQ_DISPLAY_RESET = 0x4;
static const uint8_t ANN_CMD_LEN    = 9;
static const int16_t GPWS_DEFAULT_M = 200;

struct CarrierDef { uint8_t addr, sync, statusLen, linkBit; const char *name; };
static const CarrierDef kCar[CAR_COUNT] = {
  { ADDR_ANNUNCIATOR, SYNC_ANNUNCIATOR, 6,  SI_LINK_ANNUNCIATOR, "ANNUNCIATOR" },
  { ADDR_INFODISP_1,  SYNC_INFODISP,    10, SI_LINK_INFODISP_1,  "INFO DISPLAY 1" },
  { ADDR_INFODISP_2,  SYNC_INFODISP,    10, SI_LINK_INFODISP_2,  "INFO DISPLAY 2" },
  { ADDR_RESDISP,     SYNC_RESDISP,     4,  SI_LINK_RESDISP,     "RESOURCE DISPLAY" },
};

static uint8_t  g_gpwsCfg;
static int16_t  g_gpwsThr;
static uint8_t  g_lastAnn[ANN_CMD_LEN];
static uint8_t  g_lastCtrl[CAR_COUNT];
static bool     g_sent;
static uint32_t g_lastPushMs, g_lastStatusMs;
static bool     g_answering[CAR_COUNT];
static uint8_t  g_id1Screen = 0xFF;    // Info Display 1's screen, status byte 2 (0xFF unknown)
static uint8_t  g_status[CAR_COUNT][10];   // each display's last good status packet
static bool     g_resetPending;                // DISPLAY_RESET on the next push

const char *carrierName(CarrierId c) { return c < CAR_COUNT ? kCar[c].name : "?"; }

void carrierSetGpws(uint8_t cfg, int16_t thresholdM) { g_gpwsCfg = cfg & 0x0F; g_gpwsThr = thresholdM; }

void carrierDisplayReset() { g_resetPending = true; }

static uint8_t controlByte(bool bit2, uint8_t req = REQ_PROCEED) {
  uint8_t b = (uint8_t)(req << 4);
  if (g_master.debug) b |= 0x01;
  if (g_master.demo)  b |= 0x02;
  if (bit2)           b |= 0x04;
  if (g_master.idle)  b |= 0x08;
  return b;
}

uint8_t carrierInfoDispControl(uint8_t requestType) { return controlByte(g_panel.trim, requestType); }

uint16_t carrierModeFlags() {
  uint16_t f = 0;
  if (g_master.demo)        f |= 1u << MF_DEMO;
  if (g_vessel.twIndex > 0) f |= 1u << MF_WARP;
  if (g_panel.audio)        f |= 1u << MF_AUDIO;
  if (g_panel.apArm)        f |= 1u << MF_AP_ARM;
  if (g_panel.trim)         f |= 1u << MF_TRIM;
  if (g_panel.autopilot)    f |= 1u << MF_AUTOPILOT;
  if (g_master.debug)       f |= 1u << MF_DEBUG;
  if (g_panel.switchErr)    f |= 1u << MF_SWITCH_ERR;
  if (g_master.simpitLost)  f |= 1u << MF_SIMPIT_LOST;
  if (g_panel.thrPrec)      f |= 1u << MF_THRTL_PREC;
  if (g_panel.inputPrec)    f |= 1u << MF_INPUT_PREC;
  if (g_panel.engArm)       f |= 1u << MF_ENG_ARM;
  return f;
}

static void buildAnnunciator(uint8_t *cmd) {
  uint16_t mf = carrierModeFlags();
  uint8_t grp  = (g_panel.ctrlGrp >= 1 && g_panel.ctrlGrp <= 6) ? g_panel.ctrlGrp : 1;
  uint8_t mode = (g_panel.ctrlMode <= CTRL_MODE_SPC) ? g_panel.ctrlMode : CTRL_MODE_SPC;
  cmd[0] = controlByte(g_panel.audio);
  cmd[1] = mode;
  cmd[2] = grp;
  cmd[3] = (uint8_t)(mf & 0xFF);
  cmd[4] = (uint8_t)(mf >> 8);
  cmd[5] = 0;                                     // capValue: reserved
  cmd[6] = g_gpwsCfg;
  cmd[7] = (uint8_t)((uint16_t)g_gpwsThr >> 8);
  cmd[8] = (uint8_t)((uint16_t)g_gpwsThr & 0xFF);
}

static void push(bool force) {
  uint8_t ann[ANN_CMD_LEN];
  buildAnnunciator(ann);
  uint8_t ctrl[CAR_COUNT];
  // DISPLAY_RESET (MSTR RESET, DISP CLR) takes the place of PROCEED for one push: the
  // carriers are past their boot hold by then, and the next push repeats PROCEED.
  uint8_t req = g_resetPending ? REQ_DISPLAY_RESET : REQ_PROCEED;
  if (g_resetPending) { force = true; ann[0] = (uint8_t)((ann[0] & 0x0F) | (req << 4)); }
  g_resetPending = false;
  ctrl[CAR_ANNUNCIATOR] = ann[0];
  ctrl[CAR_INFODISP_1]  = controlByte(g_panel.trim, req);
  ctrl[CAR_INFODISP_2]  = controlByte(g_panel.trim, req);
  ctrl[CAR_RESDISP]     = controlByte(false, req);

  uint32_t now = millis();
  bool changed = !g_sent || memcmp(ann, g_lastAnn, ANN_CMD_LEN) != 0 || memcmp(ctrl, g_lastCtrl, CAR_COUNT) != 0;
  if (!force && !changed && now - g_lastPushMs < CARRIER_PUSH_MS) return;
  if (changed) dbg(DBG_DISPLAY, "commands: ann ctrl %02X mode %u grp %u flags %04X; info %02X; res %02X",
                   ann[0], ann[1], ann[2], carrierModeFlags(), ctrl[CAR_INFODISP_1], ctrl[CAR_RESDISP]);

  busWrite(ADDR_ANNUNCIATOR, ann, ANN_CMD_LEN);
  uint8_t two[2];
  two[0] = ctrl[CAR_INFODISP_1]; two[1] = 0; busWrite(ADDR_INFODISP_1, two, 2);
  two[0] = ctrl[CAR_INFODISP_2]; two[1] = 0; busWrite(ADDR_INFODISP_2, two, 2);
  two[0] = ctrl[CAR_RESDISP];    two[1] = 0; busWrite(ADDR_RESDISP, two, 2);

  memcpy(g_lastAnn, ann, ANN_CMD_LEN);
  memcpy(g_lastCtrl, ctrl, CAR_COUNT);
  g_sent = true;
  g_lastPushMs = now;
}

// Reads one display's status packet. Returns true and updates its link bit if it
// answered with the right sync byte.
static bool readStatus(CarrierId c) {
  uint8_t b[10];
  const CarrierDef &d = kCar[c];
  bool ok = busRead(d.addr, b, d.statusLen) == BUS_OK && b[0] == d.sync;
  if (ok) {
    g_master.displaysKnown |= d.linkBit;
    bool link = (b[1] & 0x01) != 0;
    bool was = (g_master.displayLinks & d.linkBit) != 0;
    if (link) g_master.displayLinks |= d.linkBit; else g_master.displayLinks &= ~d.linkBit;
    if (link != was) dbg(DBG_DISPLAY, "%s KSP link %s", d.name, link ? "up" : "down");
    if (c == CAR_INFODISP_1) g_id1Screen = b[2];
    memcpy(g_status[c], b, d.statusLen);
  } else {
    if (c == CAR_INFODISP_1) g_id1Screen = 0xFF;
    g_master.displaysKnown &= ~d.linkBit;
    g_master.displayLinks &= ~d.linkBit;
  }
  if (ok != g_answering[c]) {
    if (ok) dbg(DBG_DISPLAY, "%s answering", d.name);
    else    dbgErr(DBG_DISPLAY, "%s not answering", d.name);
  }
  g_answering[c] = ok;
  return ok;
}

bool carrierReadStatus(CarrierId c) { return c < CAR_COUNT && readStatus(c); }

// The same command the carrier gets every 100 ms, with requestType STATUS instead of
// PROCEED, so nothing else about it changes. Used to learn its INT line.
bool carrierRequestStatus(CarrierId c) {
  if (c >= CAR_COUNT) return false;
  if (c == CAR_ANNUNCIATOR) {
    uint8_t ann[ANN_CMD_LEN];
    buildAnnunciator(ann);
    ann[0] = (uint8_t)((ann[0] & 0x0F) | (REQ_STATUS << 4));
    return busWrite(ADDR_ANNUNCIATOR, ann, ANN_CMD_LEN) == BUS_OK;
  }
  uint8_t two[2] = { controlByte(c == CAR_RESDISP ? false : g_panel.trim, REQ_STATUS), 0 };
  return busWrite(kCar[c].addr, two, 2) == BUS_OK;
}

/***************************************************************************************
   Backlight (GitHub #47): the BACKLIGHT frame to the four 7" displays, at once on a
   change and every CARRIER_BACKLIGHT_MS (a display that restarted gets it again).
****************************************************************************************/
static uint8_t  g_blFlight, g_blStandby;
static bool     g_blChanged;
static uint32_t g_lastBlMs;

static void pushBacklight(uint32_t now) {
  if (!g_blChanged && now - g_lastBlMs < CARRIER_BACKLIGHT_MS) return;
  uint8_t f[BACKLIGHT_LEN] = { SYNC_BACKLIGHT, g_blFlight, g_blStandby, 0 };
  for (uint8_t c = 0; c < CAR_COUNT; c++) busWrite(kCar[c].addr, f, BACKLIGHT_LEN);
  siSendBacklight(f, BACKLIGHT_LEN);             // and the Sys Info Display (a no-op while absent)
  if (g_blChanged) dbg(DBG_DISPLAY, "backlight: flight %u %%, standby %u %%", g_blFlight, g_blStandby);
  g_blChanged = false;
  g_lastBlMs = now;
}

void carrierSetBacklight(uint8_t flightPct, uint8_t standbyPct) {
  if (flightPct > 100) flightPct = 100;
  if (standbyPct > 100) standbyPct = 100;
  if (flightPct == g_blFlight && standbyPct == g_blStandby) return;
  g_blFlight = flightPct; g_blStandby = standbyPct; g_blChanged = true;
}
uint8_t carrierBacklightFlight()  { return g_blFlight; }
uint8_t carrierInfoDisp1Screen()  { return g_id1Screen; }
bool    carrierAnswering(CarrierId c) { return c < CAR_COUNT && g_answering[c]; }
const uint8_t *carrierLastStatus(CarrierId c) { return c < CAR_COUNT ? g_status[c] : nullptr; }
uint8_t carrierBacklightStandby() { return g_blStandby; }

static void cmdBacklight(const char *a) {
  int f = -1, s = -1;
  int n = sscanf(a, "%d %d", &f, &s);
  if (n >= 1 && f >= 0 && f <= 100) carrierSetBacklight((uint8_t)f, (n >= 2 && s >= 0 && s <= 100) ? (uint8_t)s : g_blStandby);
  else if (n >= 1) { conPrintf("backlight: levels are 0..100 %%"); return; }
  conPrintf("backlight: flight %u %%, standby %u %% (displays clamp to their floor)", g_blFlight, g_blStandby);
}

void carriersInit() {
  g_blFlight = BL_FLIGHT_PCT;
  g_blStandby = BL_STANDBY_PCT;
  g_blChanged = true;
  g_lastBlMs = millis();
  consoleRegister("backlight", "backlight [flight %] [standby %] — the 7\" displays' levels", cmdBacklight);
  g_gpwsCfg = 0;
  g_gpwsThr = GPWS_DEFAULT_M;
  g_sent = false;
  g_resetPending = false;
  g_master.displayLinks = g_master.displaysKnown = 0;
  memset(g_answering, 0, sizeof g_answering);
  g_id1Screen = 0xFF;
  memset(g_status, 0, sizeof g_status);
  g_lastPushMs = g_lastStatusMs = millis();
}

void carriersBoot() {
  push(true);                                    // PROCEED to all four
  for (uint8_t c = 0; c < CAR_COUNT; c++) {
    uint32_t t0 = millis();
    bool ok = false;
    do {
      ok = readStatus((CarrierId)c);
      if (!ok) delay(50);
    } while (!ok && millis() - t0 < CARRIER_BOOT_WAIT_MS);
    static const char *const kShort[CAR_COUNT] = { "ANNUNCIATOR", "INFO DISP 1", "INFO DISP 2", "RES DISP" };
    const char *ksp = (g_master.displayLinks & kCar[c].linkBit) ? "UP" : "DOWN";
    if (ok) bootLineS(SEV_OK, bootFmt("%s OK KSP %s", kShort[c], ksp), "%-16s OK  KSP %s", kCar[c].name, ksp);
    else    bootLineS(SEV_ERROR, bootFmt("%s NO ANSWER", kShort[c]), "%-16s NO ANSWER", kCar[c].name);
  }
  g_lastStatusMs = millis();
}

void carrierService() {
  push(false);
  uint32_t now = millis();
  pushBacklight(now);
  if (now - g_lastStatusMs >= CARRIER_STATUS_MS) {
    g_lastStatusMs = now;
    for (uint8_t c = 0; c < CAR_COUNT; c++) readStatus((CarrierId)c);
  }
}
