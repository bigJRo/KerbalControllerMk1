/***************************************************************************************
   switches.cpp — the two switch groups, the mode selector, SWITCH ERR.
****************************************************************************************/
#include "switches.h"
#include "config.h"
#include "state.h"
#include "debug.h"
#include "boot.h"
#include "ksp_out.h"
#include "module_bus.h"
#include "interlocks.h"
#include "carriers.h"
#include "sysinfo.h"
#include <KerbalModuleCommon.h>

struct SwitchDef {
  const char *name, *off, *on;
  ModId    mod;
  uint8_t  bit;            // input index on its module
  bool     momentary;
  uint8_t  action;         // KSP *_ACTION bit it sets (Switch Group 2), 0 = none
};

// Inventory §7.2 (Switch Group 1) and §7.4 (Switch Group 2), index = SwitchId.
static const SwitchDef kSw[SW_COUNT] = {
  { "-",       "",      "",      MOD_FUNCTION, 0,  false, 0 },
  { "MSTR",    "OPER",  "RESET", MOD_FUNCTION, 16, true,  0 },
  { "DISP",    "OPER",  "CLR",   MOD_FUNCTION, 17, true,  0 },
  { "SCE",     "NORM",  "AUX",   MOD_FUNCTION, 18, false, 0 },
  { "UPTLM",   "INHIB", "XMIT",  MOD_FUNCTION, 19, false, 0 },
  { "LTG",     "NORM",  "TEST",  MOD_FUNCTION, 20, true,  0 },
  { "A/P",     "INHIB", "ARM",   MOD_FUNCTION, 21, false, 0 },
  { "ENGINE",  "SAFE",  "ARM",   MOD_FUNCTION, 22, false, 0 },
  { "THRTL",   "STD",   "FINE",  MOD_FUNCTION, 23, false, 0 },
  { "CHUTE",   "SAFE",  "ARM",   MOD_VEHICLE,  16, false, 0 },
  { "GEAR",    "UP",    "DOWN",  MOD_VEHICLE,  17, false, GEAR_ACTION },
  { "BRAKE",   "REL",   "LOCK",  MOD_VEHICLE,  18, false, BRAKES_ACTION },
  { "EXT LT",  "OFF",   "ILLUM", MOD_VEHICLE,  19, false, LIGHT_ACTION },
  { "SAS",     "OFF",   "ENAB",  MOD_VEHICLE,  20, false, SAS_ACTION },
  { "RCS",     "OFF",   "ENAB",  MOD_VEHICLE,  21, false, RCS_ACTION },
  { "THC/RHC", "NORM",  "PREC",  MOD_VEHICLE,  22, false, 0 },
  { "AUDIO",   "MUTE",  "LIVE",  MOD_VEHICLE,  23, false, 0 },
};

struct SwitchRt {
  bool     on, err;
  uint8_t  suppress;       // SUPP_* bits
  uint32_t misSinceMs;     // disagreeing with the vessel since (0 = agrees)
};

struct GroupRt { ModId mod; bool known; uint8_t epoch; };

static SwitchRt g_sw[SW_COUNT];
static GroupRt  g_grp[2] = { { MOD_FUNCTION, false, 0 }, { MOD_VEHICLE, false, 0 } };
static GroupRt  g_modeSel = { MOD_TIME, false, 0 };
static bool     g_modeFault;

static const uint8_t TIME_SPC_BIT = 12, TIME_RVR_BIT = 13;
static const char *const kModeName[] = { "RVR", "PLN", "SPC" };

static GroupRt &groupOf(SwitchId id) { return kSw[id].mod == MOD_FUNCTION ? g_grp[0] : g_grp[1]; }

bool switchKnown(SwitchId id) { return id > SW_NONE && id < SW_COUNT && groupOf(id).known; }
bool switchOn(SwitchId id)    { return id > SW_NONE && id < SW_COUNT && g_sw[id].on; }
bool switchErr(SwitchId id)   { return id > SW_NONE && id < SW_COUNT && g_sw[id].err; }
const char *switchName(SwitchId id) { return id < SW_COUNT ? kSw[id].name : "?"; }

void switchErrSuppress(SwitchId id, uint8_t source, bool on) {
  if (id <= SW_NONE || id >= SW_COUNT) return;
  if (on) g_sw[id].suppress |= source; else g_sw[id].suppress &= (uint8_t)~source;
}

// Switch Group 2's vessel switches act on KSP only here.
static bool canAct() { return !g_master.demo && g_master.kspConnected && g_vessel.flightScene; }

/***************************************************************************************
   Actions
****************************************************************************************/
static void masterReset() {
  dbg(DBG_STATE, "MSTR RESET: modules, displays, autopilots");
  moduleResetAll();
  carrierDisplayReset();
  interlockDrop(DROP_MSTR_RESET);
}

static void displayClear() {
  dbg(DBG_STATE, "DISP CLR: GPWS and Pre-Warp to 0, screens to default");
  uint8_t zero[2] = { 0, 0 };
  moduleCommand(MOD_GPWS, KMC_CMD_SET_VALUE, zero, 2);
  moduleCommand(MOD_PREWARP, KMC_CMD_SET_VALUE, zero, 2);
  carrierDisplayReset();
}

static void setDemo(bool on, bool firstRead) {
  if (on == g_master.demo) return;
  g_master.demo = on;
  dbg(DBG_STATE, "demo %s", on ? "on" : "off");
  if (on && !firstRead) interlockDrop(DROP_DEMO);
}

// A switch's new position (or its first-read position) applied to the master.
static void apply(SwitchId id, bool on, bool firstRead) {
  const SwitchDef &d = kSw[id];
  switch (id) {
    case SW_SCE:    setDemo(on, firstRead); break;
    case SW_UPTLM:  g_master.debug = on; break;
    case SW_LTG:    if (!firstRead) moduleBulbTest(on); break;
    case SW_AP:     interlockSetApArm(on, firstRead); break;
    case SW_ENGINE: interlockSetEngine(on, firstRead); break;
    case SW_THRTL:  g_panel.thrPrec = on; break;
    case SW_CHUTE:  g_panel.chuteArm = on; break;
    case SW_PREC:   g_panel.inputPrec = on; break;
    case SW_AUDIO:  g_panel.audio = on; break;
    default: break;
  }
  if (id == SW_BRAKE) g_panel.brakeLock = on;
  if (firstRead) return;
  dbg(DBG_STATE, "%s %s", d.name, on ? d.on : d.off);
  if (!d.action) return;
  if (canAct()) kspActionGroup(d.action, on);
  else dbg(DBG_SWITCH, "%s not acted on (%s)", d.name,
           g_master.demo ? "demo" : !g_master.kspConnected ? "KSP link down" : "not in a flight scene");
}

/***************************************************************************************
   Reading
****************************************************************************************/
static void readGroup(GroupRt &g) {
  uint32_t lv;
  uint8_t ep;
  if (!moduleInputs(g.mod, &lv, &ep)) {
    if (g.known) dbgErr(DBG_SWITCH, "%s switches not readable: last positions kept", moduleName(g.mod));
    g.known = false;
    return;
  }
  bool first = !g.known || ep != g.epoch;
  g.known = true;
  g.epoch = ep;
  uint32_t pressed = moduleTakePressed(g.mod, 0x00FF0000UL);
  if (first) dbg(DBG_SWITCH, "%s switches read: %02lX", moduleName(g.mod), (unsigned long)((lv >> 16) & 0xFF));
  for (uint8_t i = SW_NONE + 1; i < SW_COUNT; i++) {
    SwitchId id = (SwitchId)i;
    const SwitchDef &d = kSw[id];
    if (d.mod != g.mod) continue;
    uint32_t bit = 1UL << d.bit;
    bool on = (lv & bit) != 0;
    if (first) { g_sw[id].on = on; apply(id, on, true); continue; }
    bool flipped = on != g_sw[id].on;
    g_sw[id].on = on;
    if (flipped) g_sw[id].misSinceMs = 0;
    if (d.momentary) {
      bool press = (pressed & bit) != 0;          // includes a flick between two reads
      if (id == SW_MSTR && press) masterReset();
      if (id == SW_DISP && press) displayClear();
      if (id == SW_LTG) {
        if (flipped) apply(id, on, false);
        else if (press && !on) { moduleBulbTest(true); moduleBulbTest(false); }
      }
      if (id != SW_LTG && flipped) dbg(DBG_STATE, "%s %s", d.name, on ? d.on : d.off);
      continue;
    }
    if (flipped) apply(id, on, false);
  }
}

static void readModeSelector() {
  uint32_t lv;
  uint8_t ep;
  if (!moduleInputs(MOD_TIME, &lv, &ep)) { g_modeSel.known = false; return; }
  g_modeSel.known = true;
  g_modeSel.epoch = ep;
  bool spc = lv & (1UL << TIME_SPC_BIT), rvr = lv & (1UL << TIME_RVR_BIT);
  if (spc && rvr) {
    if (!g_modeFault) dbg(DBG_SWITCH, "mode selector fault: SPC and RVR both on, keeping %s", kModeName[g_panel.ctrlMode]);
    g_modeFault = true;
    return;
  }
  g_modeFault = false;
  uint8_t mode = spc ? CTRL_MODE_SPC : rvr ? CTRL_MODE_RVR : CTRL_MODE_PLN;
  if (mode != g_panel.ctrlMode) dbg(DBG_STATE, "control mode %s", kModeName[mode]);
  g_panel.ctrlMode = mode;
}

/***************************************************************************************
   SWITCH ERR
****************************************************************************************/
static void evalSwitchErr(uint32_t now) {
  bool eval = !g_master.demo && g_master.kspConnected && g_vessel.flightScene && g_vessel.actionsValid &&
              !g_vessel.eva && g_grp[1].known;
  SiSwitchErr list[SI_SWERR_MAX];
  uint8_t n = 0;
  bool any = false;
  for (uint8_t i = SW_NONE + 1; i < SW_COUNT; i++) {
    SwitchId id = (SwitchId)i;
    const SwitchDef &d = kSw[id];
    SwitchRt &s = g_sw[id];
    if (!d.action) continue;
    bool vessel = (g_vessel.actions & d.action) != 0;
    bool mis = eval && s.on != vessel && !s.suppress;
    if (!mis) {
      if (s.err) dbg(DBG_SWITCH, "SWITCH ERR cleared: %s", d.name);
      s.err = false;
      s.misSinceMs = 0;
      continue;
    }
    if (!s.misSinceMs) s.misSinceMs = now | 1;
    if (!s.err && now - s.misSinceMs >= SWITCH_ERR_SETTLE_MS) {
      s.err = true;
      dbg(DBG_SWITCH, "SWITCH ERR: %s is %s, vessel %s", d.name, s.on ? d.on : d.off, vessel ? d.on : d.off);
    }
    if (s.err) {
      any = true;
      if (n < SI_SWERR_MAX) list[n++] = { (uint8_t)id, (uint8_t)s.on, (uint8_t)vessel };
    }
  }
  if (any != g_panel.switchErr) dbg(DBG_STATE, "SWITCH ERR %s", any ? "on" : "off");
  g_panel.switchErr = any;
  siSetSwitchErrors(list, n);
}

void switchesService() {
  uint32_t now = millis();
  readGroup(g_grp[0]);
  readGroup(g_grp[1]);
  readModeSelector();
  evalSwitchErr(now);
}

/***************************************************************************************
   Boot and console
****************************************************************************************/
void switchesBoot() {
  switchesService();
  if (g_grp[0].known)
    bootLineS(SEV_OK, bootFmt("ENGINE %s A/P %s", g_panel.engArm ? "ARM" : "SAFE", g_panel.apArm ? "ARM" : "INHIB"),
              "SW GRP 1  ENGINE %s  A/P %s", g_panel.engArm ? "ARM" : "SAFE", g_panel.apArm ? "ARM" : "INHIB");
  else
    bootLineS(SEV_WARN, "SW GRP 1 NOT READ", "SW GRP 1  NOT READ: ENGINE SAFE");
  if (!g_grp[1].known) bootLineS(SEV_WARN, "SW GRP 2 NOT READ", "SW GRP 2  NOT READ");
  if (g_modeSel.known) bootLineS(SEV_OK, bootFmt("CTRL MODE %s", kModeName[g_panel.ctrlMode]), "CTRL MODE  %s", kModeName[g_panel.ctrlMode]);
  else                 bootLineS(SEV_WARN, bootFmt("MODE NOT READ: %s", kModeName[g_panel.ctrlMode]),
                                 "CTRL MODE  NOT READ: %s", kModeName[g_panel.ctrlMode]);
}

static void cmdSwitches(const char *) {
  for (uint8_t gi = 0; gi < 2; gi++) {
    conPrintf("Switch Group %u (%s)%s", gi + 1, moduleName(g_grp[gi].mod), g_grp[gi].known ? "" : ": not read");
    for (uint8_t i = SW_NONE + 1; i < SW_COUNT; i++) {
      SwitchId id = (SwitchId)i;
      const SwitchDef &d = kSw[id];
      if (d.mod != g_grp[gi].mod) continue;
      const SwitchRt &s = g_sw[id];
      char vessel[32] = "";
      if (d.action) {
        if (g_vessel.actionsValid) snprintf(vessel, sizeof vessel, "vessel %s", (g_vessel.actions & d.action) ? d.on : d.off);
        else snprintf(vessel, sizeof vessel, "vessel ?");
      }
      conPrintf("  B%-2u %-7s %-5s %-13s%s%s", d.bit, d.name, s.on ? d.on : d.off, vessel,
                s.err ? " SWITCH ERR" : "", s.suppress ? " (not checked)" : "");
    }
  }
  conPrintf("Mode selector (TIME B12/B13): %s%s%s", kModeName[g_panel.ctrlMode], g_modeSel.known ? "" : " (not read)",
            g_modeFault ? " FAULT: both on" : "");
  conPrintf("SWITCH ERR %s", g_panel.switchErr ? "ON" : "off");
}

void switchesInit() {
  memset(g_sw, 0, sizeof g_sw);
  g_grp[0].known = g_grp[1].known = g_modeSel.known = false;
  g_modeFault = false;
  consoleRegister("switches", "every switch: position, the vessel, SWITCH ERR", cmdSwitches);
}
