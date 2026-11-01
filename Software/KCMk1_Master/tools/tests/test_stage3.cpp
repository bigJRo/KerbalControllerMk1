// Stage 3 selftest: switches and interlocks — Switch Groups 1 and 2 (first read, flips,
// what reaches KSP), SWITCH ERR and its exceptions, demo and debug, the mode selector,
// ENGINE SAFE / ARM with the lever, A/P INHIB, ABORT and STAGE, MSTR RESET, DISP CLR,
// LTG TEST, a watchdog restart in flight, the console.
#include "harness.h"
#include "switches.h"
#include "interlocks.h"
#include "panel_safe.h"

// Switch Group inputs on their modules (Inventory §7.2, §7.4).
static const uint32_t SG1_MSTR = 1u << 16, SG1_DISP = 1u << 17, SG1_SCE = 1u << 18, SG1_UPTLM = 1u << 19,
                      SG1_LTG = 1u << 20, SG1_AP = 1u << 21, SG1_ENGINE = 1u << 22, SG1_THRTL = 1u << 23;
static const uint32_t SG2_CHUTE = 1u << 16, SG2_GEAR = 1u << 17, SG2_BRAKE = 1u << 18, SG2_EXTLT = 1u << 19,
                      SG2_SAS = 1u << 20, SG2_RCS = 1u << 21, SG2_PREC = 1u << 22, SG2_AUDIO = 1u << 23;
static const uint32_t TIME_SPC = 1u << 12, TIME_RVR = 1u << 13;

static uint32_t g_presetSg1, g_presetSg2, g_presetTime;
static void preset(Bench &b) {
  b.m(MOD_FUNCTION).state = g_presetSg1;
  b.m(MOD_VEHICLE).state = g_presetSg2;
  b.m(MOD_TIME).state = g_presetTime;
}
static void boot3(Bench &b, uint32_t sg1, uint32_t sg2, uint32_t time = 0, uint32_t srsr = 1) {
  g_presetSg1 = sg1; g_presetSg2 = sg2; g_presetTime = time;
  powerOnBoot(b, srsr, preset);
}

static void flip(ModuleSim &m, uint32_t bit, bool on) { if (on) m.press(bit); else m.release(bit); }

static int sentAg(byte type, uint8_t action) {
  int n = 0;
  for (auto &s : stubSimpit().sent) if (s.type == type && s.b.size() == 1 && s.b[0] == action) n++;
  return n;
}
static int sentType(byte type) { int n = 0; for (auto &s : stubSimpit().sent) if (s.type == type) n++; return n; }
static int sentThrottleZero() {
  int n = 0;
  for (auto &s : stubSimpit().sent) if (s.type == THROTTLE_MESSAGE && s.b.size() == 2 && s.b[0] == 0 && s.b[1] == 0) n++;
  return n;
}
static void actions(uint8_t a) { byte m = a; inbound(ACTIONSTATUS_MESSAGE, &m, 1); }
static uint16_t annFlags(const Rig &r) {
  const auto &c = r.ann.cmds.back();
  return (uint16_t)(c[3] | c[4] << 8);
}

static std::vector<DropReason> g_drops;
static void recordDrop(DropReason why) { g_drops.push_back(why); }
static int drops(DropReason why) { int n = 0; for (auto d : g_drops) if (d == why) n++; return n; }

static void testFirstRead(Bench &b) {
  printf("switches: first read sets the master's modes and sends nothing\n");
  boot3(b, SG1_UPTLM | SG1_AP | SG1_ENGINE | SG1_THRTL, SG2_GEAR | SG2_SAS | SG2_AUDIO, TIME_RVR);
  runFor(300);
  CHECK(g_master.debug && g_panel.apArm && g_panel.engArm && g_panel.thrPrec, "debug, A/P ARM, ENGINE ARM, FINE");
  CHECK(interlockEngineState() == ENG_ARMED, "ENGINE already on ARM is armed straight away");
  CHECK(g_panel.audio && !g_panel.chuteArm && !g_master.demo, "AUDIO LIVE, CHUTE SAFE, demo off");
  CHECK(g_panel.ctrlMode == CTRL_MODE_RVR, "control mode RVR");
  CHECK(b.rig.si.shows("ENGINE ARM A/P ARM") && b.rig.si.shows("CTRL MODE RVR"), "boot lines");
  enterFlight();
  actions(0);                                    // gear up, SAS off: both disagree
  runFor(20);
  CHECK(sentType(AGACTIVATE_MESSAGE) + sentType(AGDEACTIVATE_MESSAGE) == 0, "entering flight enforces no switch");
  CHECK(!g_panel.switchErr, "SWITCH ERR waits for the settle time");
  runFor(1100);
  CHECK(g_panel.switchErr && switchErr(SW_GEAR) && switchErr(SW_SAS) && !switchErr(SW_BRAKE), "SWITCH ERR: GEAR and SAS");
  CHECK(annFlags(b.rig) & (1u << MF_SWITCH_ERR), "Annunciator SWITCH ERR");
  const auto *f = b.rig.si.last(SI_FRAME_SWITCH_ERR);
  CHECK(f && f->size() == 8 && (*f)[1] == 2 && (*f)[2] == SW_GEAR && (*f)[3] == 1 && (*f)[4] == 0 && (*f)[5] == SW_SAS,
        "Sys Info SWITCH_ERR names GEAR and SAS (switch 1, vessel 0)");
  actions(GEAR_ACTION | SAS_ACTION);
  runFor(20);
  f = b.rig.si.last(SI_FRAME_SWITCH_ERR);
  CHECK(!g_panel.switchErr && f && (*f)[1] == 0, "cleared when the vessel agrees; window closed");
}

static void testFlips(Bench &b) {
  printf("switches: Switch Group 2 flips in flight\n");
  ModuleSim &v = b.m(MOD_VEHICLE);
  stubSimpit().sent.clear();
  flip(v, SG2_GEAR, false); runFor(20);
  CHECK(sentAg(AGDEACTIVATE_MESSAGE, GEAR_ACTION) == 1, "GEAR UP: gear deactivate");
  runFor(500);
  CHECK(!g_panel.switchErr, "no SWITCH ERR while KSP catches up");
  actions(SAS_ACTION); runFor(20);
  flip(v, SG2_BRAKE, true); runFor(20);
  CHECK(sentAg(AGACTIVATE_MESSAGE, BRAKES_ACTION) == 1 && g_panel.brakeLock, "BRAKE LOCK: brakes on");
  flip(v, SG2_EXTLT, true); runFor(20);
  CHECK(sentAg(AGACTIVATE_MESSAGE, LIGHT_ACTION) == 1, "EXT LT ILLUM: lights on");
  flip(v, SG2_SAS, false); runFor(20);
  CHECK(sentAg(AGDEACTIVATE_MESSAGE, SAS_ACTION) == 1, "SAS OFF: SAS off");
  flip(v, SG2_RCS, true); runFor(20);
  CHECK(sentAg(AGACTIVATE_MESSAGE, RCS_ACTION) == 1, "RCS ENAB: RCS on");
  actions(BRAKES_ACTION | LIGHT_ACTION); runFor(1100);   // KSP took all but RCS
  CHECK(g_panel.switchErr && switchErr(SW_RCS) && !switchErr(SW_BRAKE), "a flip KSP did not follow: SWITCH ERR");
  switchErrSuppress(SW_RCS, SUPP_AUTOPILOT, true); runFor(20);
  CHECK(!g_panel.switchErr, "suppressed while an autopilot changes RCS");
  switchErrSuppress(SW_RCS, SUPP_AUTOPILOT, false); runFor(20);
  CHECK(!g_panel.switchErr, "settle time again after the exception ends");
  runFor(1100);
  CHECK(g_panel.switchErr, "and back after it");
  flightStatusMessage fs = {}; fs.flightStatusFlags = FLIGHT_IN_FLIGHT | FLIGHT_IS_EVA;
  inbound(FLIGHT_STATUS_MESSAGE, &fs, sizeof fs); runFor(20);
  CHECK(!g_panel.switchErr, "not checked on EVA");
  fs.flightStatusFlags = FLIGHT_IN_FLIGHT; inbound(FLIGHT_STATUS_MESSAGE, &fs, sizeof fs);
  actions(BRAKES_ACTION | LIGHT_ACTION | RCS_ACTION); runFor(20);
  CHECK(!g_panel.switchErr, "all agree");

  printf("switches: local switches\n");
  flip(v, SG2_CHUTE, true); flip(v, SG2_PREC, true); flip(v, SG2_AUDIO, false); runFor(150);
  CHECK(g_panel.chuteArm && g_panel.inputPrec && !g_panel.audio, "CHUTE ARM, THC/RHC PREC, AUDIO MUTE");
  CHECK((annFlags(b.rig) & (1u << MF_INPUT_PREC)) && !(annFlags(b.rig) & (1u << MF_AUDIO)) &&
        !(b.rig.ann.cmds.back()[0] & 0x04), "Annunciator INPUT PREC on, AUDIO off");
  CHECK(sentType(AGACTIVATE_MESSAGE) == 3, "local switches send nothing");

  printf("switches: a flip outside a flight scene is not acted on\n");
  leaveFlight();
  stubSimpit().sent.clear();
  flip(v, SG2_GEAR, true); runFor(150);
  CHECK(sentType(AGACTIVATE_MESSAGE) == 0, "GEAR DOWN in the menus: nothing sent");
  enterFlight();
  actions(BRAKES_ACTION | LIGHT_ACTION | RCS_ACTION); runFor(20);
  CHECK(sentType(AGACTIVATE_MESSAGE) == 0, "nor on the next flight");
  runFor(1100);
  CHECK(switchErr(SW_GEAR), "which shows as SWITCH ERR");
  flip(v, SG2_GEAR, false); runFor(20);
  actions(BRAKES_ACTION | LIGHT_ACTION | RCS_ACTION);
  runFor(1100);
  CHECK(!g_panel.switchErr, "flipped back to agree");
}

static void testDemoDebug(Bench &b) {
  printf("switches: demo and debug\n");
  boot3(b, 0, 0);
  interlockOnDrop(recordDrop); g_drops.clear();
  ModuleSim &f = b.m(MOD_FUNCTION), &v = b.m(MOD_VEHICLE);
  runFor(200);
  CHECK(v.life == KMC_STATUS_DISABLED && !g_master.demo, "DISABLED, demo off, KSP down");
  int doneBefore = b.rig.si.count(SI_FRAME_BOOT_DONE);
  flip(f, SG1_SCE, true); runFor(150);
  CHECK(g_master.demo && v.life == KMC_STATUS_ACTIVE, "SCE AUX in the menus: demo, modules ENABLEd");
  CHECK(b.rig.si.count(SI_FRAME_BOOT_DONE) > doneBefore, "boot screen released");
  CHECK((b.rig.ann.cmds.back()[0] & 0x02) && (annFlags(b.rig) & (1u << MF_DEMO)), "displays get the demo flag");
  CHECK(drops(DROP_DEMO) == 1, "autopilots and HOLD dropped on entering demo");
  stubSimpit().sent.clear();
  flip(v, SG2_GEAR, true); runFor(20);
  CHECK(stubSimpit().sent.empty() && !g_panel.switchErr, "in demo: nothing to KSP, no SWITCH ERR");
  flip(f, SG1_SCE, false); runFor(150);
  CHECK(!g_master.demo && v.life == KMC_STATUS_DISABLED, "SCE NORM: demo off, back to DISABLED");
  enterFlight();
  actions(0); runFor(1100);
  CHECK(switchErr(SW_GEAR) && sentType(AGACTIVATE_MESSAGE) == 0, "a flip made in demo is not acted on: SWITCH ERR");
  flip(f, SG1_UPTLM, true); runFor(20);
  CHECK(g_master.debug && (b.rig.ann.cmds.back()[0] & 0x01), "UPTLM XMIT: debug, displays' debug flag");
  Serial.stubType("debug off\n"); runFor(10);
  CHECK(!g_master.debug, "the console can turn it off");
  flip(f, SG1_UPTLM, false); runFor(20); flip(f, SG1_UPTLM, true); runFor(20);
  CHECK(g_master.debug, "until the next flip");
  flip(v, SG2_GEAR, false); runFor(20);
}

static void testModeSelector(Bench &b) {
  printf("switches: control mode selector\n");
  ModuleSim &t = b.m(MOD_TIME);
  t.release(TIME_SPC | TIME_RVR); runFor(20);
  CHECK(g_panel.ctrlMode == CTRL_MODE_PLN && b.rig.ann.cmds.back()[1] == CTRL_MODE_PLN, "neither: PLN (centre)");
  t.press(TIME_SPC); runFor(20);
  CHECK(g_panel.ctrlMode == CTRL_MODE_SPC && b.rig.ann.cmds.back()[1] == CTRL_MODE_SPC, "B12: SPC");
  t.release(TIME_SPC); t.press(TIME_RVR); runFor(20);
  CHECK(g_panel.ctrlMode == CTRL_MODE_RVR, "B13: RVR");
  Serial.out.clear();
  t.press(TIME_SPC); runFor(20);
  CHECK(g_panel.ctrlMode == CTRL_MODE_RVR && !g_panel.switchErr, "both on: last good mode kept, no SWITCH ERR");
  CHECK(serialHas("mode selector fault"), "both on: debug message");
  t.release(TIME_SPC | TIME_RVR); runFor(20);
  CHECK(g_panel.ctrlMode == CTRL_MODE_PLN, "recovers");
  // Time Control off the bus: read while DISABLED, so the mode is right before a flight.
  leaveFlight();
  t.press(TIME_SPC); runFor(150);
  CHECK(g_panel.ctrlMode == CTRL_MODE_SPC, "read while not in flight");
}

static void testEngine(Bench &b) {
  printf("interlocks: ENGINE SAFE / ARM and the lever\n");
  boot3(b, SG1_ENGINE | SG1_AP | SG1_THRTL, 0);
  interlockOnDrop(recordDrop); g_drops.clear();
  ModuleSim &f = b.m(MOD_FUNCTION), &thr = b.m(MOD_THROTTLE);
  enterFlight();
  thr.thrPos = 20000; thr.signal(); runFor(300);
  CHECK(interlockThrottleLive() && thr.thrPrec, "ARMED: inputs taken, FINE passed to the module");
  stubSimpit().sent.clear();
  flip(f, SG1_ENGINE, false); runFor(30);
  CHECK(!interlockThrottleLive() && interlockEngineState() == ENG_SAFE, "SAFE: inputs ignored");
  CHECK(sentThrottleZero() == 1, "SAFE: 0 %% to KSP");
  CHECK(drops(DROP_ENG_SAFE) == 1, "SAFE: throttle autopilots dropped");
  CHECK(thr.thrPos == 0 && thr.countCmd(KMC_CMD_SET_THROTTLE) >= 1, "SAFE: lever driven to 0 %%");
  runFor(300);
  CHECK(!thr.thrPrec, "precision off while SAFE (lever = throttle)");
  CHECK(!(annFlags(b.rig) & (1u << MF_ENG_ARM)) && (annFlags(b.rig) & (1u << MF_THRTL_PREC)),
        "Annunciator: ENG ARM off, THRTL PREC still shows the switch");
  // The pilot moves the lever while SAFE: it goes back to 0 % on release.
  thr.touched = true; thr.thrPos = 15000; thr.signal();
  int c0 = thr.countCmd(KMC_CMD_SET_THROTTLE);
  runFor(500);
  CHECK(thr.thrPos == 15000 && thr.countCmd(KMC_CMD_SET_THROTTLE) == c0, "touched: no lever command (it would be refused)");
  CHECK(sentType(THROTTLE_MESSAGE) == 1, "and nothing but the 0 %% to KSP");
  thr.touched = false; thr.signal(); runFor(250);
  CHECK(thr.thrPos == 0, "released: back to 0 %%");
  // ARM with the lever held up: ARMING until the lever reports 0 %.
  thr.touched = true; thr.thrPos = 9000; thr.signal(); runFor(20);
  stubSimpit().sent.clear();
  flip(f, SG1_ENGINE, true); runFor(300);
  CHECK(interlockEngineState() == ENG_ARMING && !interlockThrottleLive(), "ARM: ARMING while the lever is held up");
  CHECK(sentThrottleZero() == 1, "ARM: 0 %% sent again");
  thr.touched = false; thr.signal(); runFor(300);
  CHECK(thr.thrPos == 0 && interlockEngineState() == ENG_ARMED, "lever at 0 %%: ARMED");
  runFor(300);
  CHECK(thr.thrPrec, "FINE restored once ARMED");
  CHECK(annFlags(b.rig) & (1u << MF_ENG_ARM), "Annunciator ENG ARM");

  printf("interlocks: A/P INHIB / ARM\n");
  flip(f, SG1_AP, false); runFor(20);
  CHECK(drops(DROP_AP_INHIB) == 1 && !g_panel.apArm && !(annFlags(b.rig) & (1u << MF_AP_ARM)), "INHIB: autopilots dropped");
  flip(f, SG1_AP, true); runFor(20);
  CHECK(drops(DROP_AP_INHIB) == 1 && (annFlags(b.rig) & (1u << MF_AP_ARM)), "ARM: no drop, A/P ARM lit");

  printf("interlocks: Switch Group 1 missing\n");
  b.attachAll(); Wire.detach(0x21);
  b.rig.si.frames.clear(); resetStubs(); b.powerOnAll();
  stubSimpit().kspUp = false; stubSrsr() = 1; setup();
  runFor(200);
  CHECK(!g_panel.engArm && !g_panel.apArm && interlockEngineState() == ENG_SAFE, "ENGINE SAFE and A/P INHIB until read");
  CHECK(b.rig.si.sevOf("SW GRP 1 NOT READ") == SEV_WARN, "boot line");
}

static void testSafeLines(Bench &b) {
  printf("safe lines: ABORT and STAGE\n");
  boot3(b, 0, 0);
  enterFlight();
  stubSimpit().sent.clear();
  for (int i = 0; i < 3; i++) {                  // bounce shorter than the debounce
    stubPin(PIN_SAFE_ABORT) = HIGH; runFor(6, 1);
    stubPin(PIN_SAFE_ABORT) = LOW;  runFor(6, 1);
  }
  CHECK(sentAg(AGACTIVATE_MESSAGE, ABORT_ACTION) == 0, "a bounce is not a press");
  stubPin(PIN_SAFE_ABORT) = HIGH; runFor(20, 1);
  CHECK(sentAg(AGACTIVATE_MESSAGE, ABORT_ACTION) == 1, "ABORT");
  runFor(500);
  stubPin(PIN_SAFE_ABORT) = LOW; runFor(20, 1);
  CHECK(sentAg(AGACTIVATE_MESSAGE, ABORT_ACTION) == 1, "once per press");
  stubPin(PIN_SAFE_STAGE) = HIGH; runFor(20, 1); stubPin(PIN_SAFE_STAGE) = LOW; runFor(20, 1);
  CHECK(sentAg(AGACTIVATE_MESSAGE, STAGE_ACTION) == 1, "STAGE");
  stubPin(PIN_SAFE_STAGE) = HIGH; runFor(20, 1); stubPin(PIN_SAFE_STAGE) = LOW; runFor(20, 1);
  CHECK(sentAg(AGACTIVATE_MESSAGE, STAGE_ACTION) == 1, "a second press within 1 s is ignored");
  runFor(1000);
  stubPin(PIN_SAFE_STAGE) = HIGH; runFor(20, 1); stubPin(PIN_SAFE_STAGE) = LOW; runFor(20, 1);
  CHECK(sentAg(AGACTIVATE_MESSAGE, STAGE_ACTION) == 2, "after the lockout it stages again");
  leaveFlight();
  stubPin(PIN_SAFE_STAGE) = HIGH; runFor(20, 1); stubPin(PIN_SAFE_STAGE) = LOW; runFor(20, 1);
  CHECK(sentAg(AGACTIVATE_MESSAGE, STAGE_ACTION) == 2, "not outside a flight scene");
  // A line already high at start-up is not a press.
  b.attachAll(); b.rig.si.frames.clear(); resetStubs(); b.powerOnAll();
  stubSimpit().kspUp = true; stubSrsr() = 1;
  stubPin(PIN_SAFE_ABORT) = HIGH;
  setup();
  byte scene = 0; inbound(SCENE_CHANGE_MESSAGE, &scene, 1);
  runFor(100, 1);
  CHECK(sentAg(AGACTIVATE_MESSAGE, ABORT_ACTION) == 0, "high at start-up: no ABORT");
  stubPin(PIN_SAFE_ABORT) = LOW; runFor(20, 1);
}

static int carrierResets(const CarrierSim &c) {
  int n = 0; for (auto &v : c.cmds) if (!v.empty() && (v[0] >> 4) == 0x4) n++; return n;
}

static void testMomentary(Bench &b) {
  printf("switches: MSTR RESET, DISP CLR, LTG TEST\n");
  boot3(b, 0, 0);
  interlockOnDrop(recordDrop); g_drops.clear();
  ModuleSim &f = b.m(MOD_FUNCTION);
  enterFlight();
  for (auto &m : b.mods) m.cmds.clear();
  f.press(SG1_MSTR); f.release(SG1_MSTR); runFor(30);   // a flick between two reads
  bool all = true, active = true;
  for (auto &m : b.mods) { if (m.countCmd(KMC_CMD_RESET) != 1) all = false; if (m.life != KMC_STATUS_ACTIVE) active = false; }
  CHECK(all && active, "MSTR RESET: CMD_RESET to every module, which stay ACTIVE in flight");
  CHECK(carrierResets(b.rig.ann) == 1 && carrierResets(b.rig.id1) == 1 && carrierResets(b.rig.id2) == 1 &&
        carrierResets(b.rig.res) == 1, "DISPLAY_RESET to the four displays, once");
  CHECK(drops(DROP_MSTR_RESET) == 1, "autopilots and HOLD off");
  CHECK(b.m(MOD_UI).ledWrites >= 1, "LEDs re-sent after the reset");
  leaveFlight();
  for (auto &m : b.mods) m.cmds.clear();
  f.press(SG1_MSTR); runFor(150); f.release(SG1_MSTR); runFor(150);
  CHECK(b.m(MOD_VEHICLE).countCmd(KMC_CMD_RESET) == 1 && b.m(MOD_VEHICLE).life == KMC_STATUS_DISABLED,
        "outside flight: reset, then back to DISABLED");
  b.m(MOD_GPWS).value = b.m(MOD_PREWARP).value = 77;
  f.press(SG1_DISP); runFor(150); f.release(SG1_DISP); runFor(150);
  CHECK(b.m(MOD_GPWS).value == 0 && b.m(MOD_PREWARP).value == 0, "DISP CLR: GPWS and Pre-Warp to 0");
  CHECK(carrierResets(b.rig.ann) == 3 && carrierResets(b.rig.res) == 3, "DISP CLR: displays reset (third reset)");
  for (auto &m : b.mods) m.cmds.clear();
  f.press(SG1_LTG); runFor(150);
  all = true; for (auto &m : b.mods) if (m.countCmd(KMC_CMD_BULB_TEST) != 1 || m.cmds[0][1] != 1) all = false;
  CHECK(all, "LTG TEST: bulb test on every module");
  f.release(SG1_LTG); runFor(MODBUS_BULB_MIN_MS + 100);
  all = true; for (auto &m : b.mods) if (m.countCmd(KMC_CMD_BULB_TEST) != 2) all = false;
  CHECK(all, "LTG NORM: bulb test stopped");
}

static void testWatchdogRestart(Bench &b) {
  printf("switches: watchdog restart in flight\n");
  boot3(b, SG1_ENGINE | SG1_AP, SG2_GEAR | SG2_BRAKE);
  enterFlight();
  actions(GEAR_ACTION | BRAKES_ACTION); runFor(20);
  b.m(MOD_THROTTLE).thrPos = 12000;                 // the pilot's lever, mid-flight
  b.attachAll(); b.rig.si.frames.clear(); resetStubs(); stubSrsr() = 1u << 4;
  stubRetain()[0] = stubRetain()[1] = stubRetain()[2] = 0;
  stubSimpit().kspUp = true;
  for (auto &m : b.mods) m.cmds.clear();
  setup();
  byte scene = 0; inbound(SCENE_CHANGE_MESSAGE, &scene, 1);
  actions(GEAR_ACTION | BRAKES_ACTION);
  runFor(300);
  CHECK(sentType(AGACTIVATE_MESSAGE) + sentType(AGDEACTIVATE_MESSAGE) == 0, "no switch enforced");
  bool lever = false;
  for (auto &s : stubSimpit().sent) if (s.type == THROTTLE_MESSAGE && s.b.size() == 2 && (s.b[0] | s.b[1] << 8) == 12000) lever = true;
  CHECK(interlockEngineState() == ENG_ARMED && sentThrottleZero() == 0 && lever, "ENGINE ARM: armed, the throttle follows the lever");
  CHECK(!g_panel.switchErr && g_panel.apArm, "switches read back");
}

static void testConsole() {
  printf("switches: console\n");
  Serial.out.clear(); Serial.stubType("switches\n"); runFor(10);
  CHECK(serialHas("GEAR") && serialHas("vessel DOWN") && serialHas("Mode selector"), "switches table");
  Serial.out.clear(); Serial.stubType("interlocks\n"); runFor(10);
  CHECK(serialHas("ENGINE ARMED") && serialHas("A/P ARM"), "interlocks");
  Serial.out.clear(); Serial.stubType("status\n"); runFor(10);
  CHECK(serialHas("ENGINE ARMED"), "status line");
}

void runStage3() {
  static Bench b;
  stubEepromReset();
  testFirstRead(b);
  testFlips(b);
  testDemoDebug(b);
  testModeSelector(b);
  testEngine(b);
  testSafeLines(b);
  testMomentary(b);
  testWatchdogRestart(b);
  testConsole();
  for (int a = 0; a < 128; a++) Wire.detach(a);
}
