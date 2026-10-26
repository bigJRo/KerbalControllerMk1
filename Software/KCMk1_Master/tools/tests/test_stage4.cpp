// Stage 4 selftest: the flight panels — Vehicle (BRAKES, toggles, CAG buttons with the
// control-group offset, the state machines), Stability (SAS modes, HOLD, INVERT),
// Action, AUX (EVA keys, UNDOCK, control points), Dual Encoder, demo, and keys released
// when a flight ends.
#include "harness.h"
#include "switches.h"
#include "interlocks.h"
#include "hold.h"
#include "keys.h"

static const uint32_t SG1_SCE = 1u << 18, SG1_MSTR = 1u << 16, SG1_AP = 1u << 21;
static const uint32_t SG2_CHUTE = 1u << 16, SG2_BRAKE = 1u << 18, SG2_SAS = 1u << 20;
static const uint32_t TIME_SPC = 1u << 12;

static uint32_t g_sg1, g_sg2, g_time;
static void preset(Bench &b) {
  b.m(MOD_FUNCTION).state = g_sg1; b.m(MOD_VEHICLE).state = g_sg2; b.m(MOD_TIME).state = g_time;
}
static void boot4(Bench &b, uint32_t sg1, uint32_t sg2, uint32_t time) {
  g_sg1 = sg1; g_sg2 = sg2; g_time = time;
  powerOnBoot(b, 1, preset);
}

static void tap(ModuleSim &m, uint32_t bit) { m.press(bit); m.release(bit); runFor(20); }
static void hold(ModuleSim &m, uint32_t bit, uint32_t ms) { m.press(bit); runFor(ms); m.release(bit); runFor(20); }

static int sent(byte type, int b0 = -1) {
  int n = 0;
  for (auto &s : stubSimpit().sent) if (s.type == type && (b0 < 0 || (!s.b.empty() && s.b[0] == b0))) n++;
  return n;
}
static int keys(int16_t vk, uint8_t mod) {
  int n = 0;
  for (auto &s : stubSimpit().sent)
    if (s.type == KEYBOARD_EMULATOR && s.b.size() == 3 && s.b[0] == mod && (int16_t)(s.b[1] | s.b[2] << 8) == vk) n++;
  return n;
}
static void actions(uint8_t a) { byte m = a; inbound(ACTIONSTATUS_MESSAGE, &m, 1); }
static void cags(std::initializer_list<int> on) {
  uint8_t st[32] = { 0 };
  for (int n : on) st[n / 8] |= (uint8_t)(1u << (n % 8));
  inbound(CAGSTATUS_MESSAGE, st, 32);
}
static void sasInfo(uint8_t mode, uint16_t avail) {
  SASInfoMessage m; m.currentSASMode = mode; m.SASModeAvailability = (int16_t)avail;
  inbound(SAS_MODE_INFO_MESSAGE, &m, sizeof m);
}
static void eva(bool on) {
  flightStatusMessage fs = {}; fs.flightStatusFlags = (byte)(FLIGHT_IN_FLIGHT | (on ? FLIGHT_IS_EVA : 0));
  inbound(FLIGHT_STATUS_MESSAGE, &fs, sizeof fs);
}

static void testVehicle(Bench &b) {
  printf("panels: Vehicle Control\n");
  boot4(b, SG1_AP, 0, TIME_SPC);
  enterFlight();
  actions(0); cags({}); runFor(100);
  ModuleSim &v = b.m(MOD_VEHICLE), &de = b.m(MOD_DUALENC);
  stubSimpit().sent.clear();
  tap(v, 1u << 2);
  CHECK(sent(CAGTOGGLE_MESSAGE, 13) == 1, "ANTENNA: CAG 13 toggle");
  de.turn(0, 2); runFor(20);
  CHECK(g_panel.ctrlGrp == 3, "ENC1: control group 3");
  tap(v, 1u << 2);
  CHECK(sent(CAGTOGGLE_MESSAGE, 13 + 80) == 1, "control group 3: CAG 93");
  de.edge(0x01); runFor(20);
  CHECK(g_panel.ctrlGrp == 1, "ENC1 button: back to 1");
  tap(v, 1u << 1); tap(v, 1u << 3);
  CHECK(sent(AGTOGGLE_MESSAGE, LIGHT_ACTION) == 1 && sent(AGTOGGLE_MESSAGE, GEAR_ACTION) == 1, "LIGHTS and LANDING GEAR toggle");
  actions(LIGHT_ACTION); cags({ 13 }); runFor(100);
  CHECK(v.ledOf(1) == KMC_LED_ACTIVE && v.ledOf(3) == KMC_LED_ENABLED && v.ledOf(2) == KMC_LED_ACTIVE,
        "LEDs: lit while KSP reports them on");

  printf("panels: BRAKES and the BRAKE switch\n");
  actions(0); stubSimpit().sent.clear();
  v.press(1u << 0); runFor(20);
  CHECK(sent(AGACTIVATE_MESSAGE, BRAKES_ACTION) == 1, "pressed: brakes on");
  actions(BRAKES_ACTION); runFor(1500);
  CHECK(!switchErr(SW_BRAKE), "no SWITCH ERR while the button holds them (switch on REL)");
  v.release(1u << 0); runFor(20);
  CHECK(sent(AGDEACTIVATE_MESSAGE, BRAKES_ACTION) == 1, "released, BRAKE on REL: brakes off");
  actions(0); runFor(20);
  v.press(SG2_BRAKE); runFor(20);                  // BRAKE to LOCK: brakes on
  actions(BRAKES_ACTION); runFor(20);
  stubSimpit().sent.clear();
  tap(v, 1u << 0);
  CHECK(sent(AGDEACTIVATE_MESSAGE, BRAKES_ACTION) == 0, "BRAKE on LOCK: they stay on after the button");
  v.release(SG2_BRAKE); runFor(20); actions(0); runFor(20);

  printf("panels: state machines\n");
  stubSimpit().sent.clear();
  tap(v, 1u << 8);
  CHECK(sent(CAGACTIVATE_MESSAGE, 19) == 1, "HEAT SHIELD READY: deploy (CAG 19)");
  cags({ 19 }); runFor(100);
  CHECK(v.ledOf(8) == KMC_LED_ACTIVE, "DEPLOYED: green");
  tap(v, 1u << 8);
  CHECK(sent(CAGACTIVATE_MESSAGE, 20) == 1, "DEPLOYED: release (CAG 20)");
  cags({ 19, 20 }); runFor(100);
  CHECK(v.ledOf(8) == KMC_LED_CUT, "RELEASED: red");
  stubSimpit().sent.clear();
  tap(v, 1u << 8);
  CHECK(stubSimpit().sent.empty(), "RELEASED: a press does nothing");
  cags({ 19 }); runFor(20);                        // DEPLOYED, e.g. after a repack mid-sequence
  hold(v, 1u << 8, 2100);
  CHECK(sent(CAGDEACTIVATE_MESSAGE, 19) == 1 && sent(CAGDEACTIVATE_MESSAGE, 20) == 1 && sent(CAGACTIVATE_MESSAGE) == 0,
        "2 s hold: both groups off, and the release fires nothing");
  cags({}); runFor(100);
  CHECK(v.ledOf(8) == KMC_LED_ENABLED, "READY again");
  CHECK(v.ledOf(10) == KMC_LED_OFF && v.ledOf(11) == KMC_LED_OFF, "chutes dark with CHUTE SAFE");
  stubSimpit().sent.clear();
  tap(v, 1u << 10);
  CHECK(stubSimpit().sent.empty(), "and ignored");
  v.press(SG2_CHUTE); runFor(150);
  CHECK(v.ledOf(10) == KMC_LED_ENABLED, "CHUTE ARM: backlit");
  tap(v, 1u << 11);
  CHECK(sent(CAGACTIVATE_MESSAGE, 23) == 1, "DROGUE: deploy (CAG 23)");
  de.turn(0, 1); runFor(20);
  tap(v, 1u << 10);
  CHECK(sent(CAGACTIVATE_MESSAGE, 21 + 40) == 1, "MAIN in control group 2: CAG 61");
  de.edge(0x01); runFor(20);
}

static void testStability(Bench &b) {
  printf("panels: Stability Control — SAS modes\n");
  ModuleSim &s = b.m(MOD_STABILITY), &v = b.m(MOD_VEHICLE);
  runFor(100);
  bool dark = true; for (int i = 0; i < 10; i++) if (s.ledOf(i) != KMC_LED_OFF) dark = false;
  CHECK(dark, "SAS switch OFF: all ten dark");
  stubSimpit().sent.clear();
  tap(s, 1u << 6);
  CHECK(sent(SAS_MODE_MESSAGE) == 0, "and ignored");
  v.press(SG2_SAS); runFor(20);
  actions(SAS_ACTION); sasInfo(AP_PROGRADE, (uint16_t)(0x3FF & ~(1u << AP_TARGET))); runFor(100);
  CHECK(s.ledOf(6) == KMC_LED_ACTIVE && s.ledOf(8) == KMC_LED_ENABLED && s.ledOf(0) == KMC_LED_OFF,
        "ENAB: current lit, available backlit, unavailable dark");
  stubSimpit().sent.clear();
  tap(s, 1u << 4); tap(s, 1u << 0);
  CHECK(sent(SAS_MODE_MESSAGE, AP_NORMAL) == 1 && sent(SAS_MODE_MESSAGE, AP_TARGET) == 0, "NORMAL sent, TARGET ignored");

  printf("panels: INVERT\n");
  stubSimpit().sent.clear();
  s.press(1u << 11); runFor(20);
  CHECK(keys(VK_F, KEY_DOWN_MOD) == 1 && keyHeld(VK_F), "F down while held");
  actions(0); runFor(1500);
  CHECK(!switchErr(SW_SAS), "no SWITCH ERR for SAS meanwhile");
  CHECK(s.ledOf(11) == KMC_LED_ACTIVE, "LED while held");
  s.release(1u << 11); runFor(20);
  CHECK(keys(VK_F, KEY_UP_MOD) == 1 && !keyHeld(VK_F), "F up on release");
  runFor(1100);
  CHECK(switchErr(SW_SAS), "SWITCH ERR checks SAS again after");
  actions(SAS_ACTION); runFor(20);

  printf("panels: HOLD\n");
  actions(0); sasInfo(255, 0x3FF); runFor(20);
  stubSimpit().sent.clear();
  tap(s, 1u << 10);
  CHECK(holdEngaged() && sent(AGACTIVATE_MESSAGE, SAS_ACTION) == 1 && sent(SAS_MODE_MESSAGE, AP_STABILITYASSIST) == 1,
        "SPC: SAS on, stability assist");
  runFor(100);
  uint16_t f = (uint16_t)(b.rig.ann.cmds.back()[3] | b.rig.ann.cmds.back()[4] << 8);
  CHECK((f & (1u << MF_AUTOPILOT)) && s.ledOf(10) == KMC_LED_ACTIVE, "Annunciator AUTOPILOT, HOLD lit");
  actions(SAS_ACTION); sasInfo(AP_STABILITYASSIST, 0x3FF); runFor(20);
  b.m(MOD_FUNCTION).release(SG1_AP); runFor(20);   // A/P INHIB does not drop HOLD
  CHECK(holdEngaged(), "A/P INHIB: HOLD stays");
  stubSimpit().sent.clear();
  tap(s, 1u << 10);
  CHECK(!holdEngaged() && sent(AGDEACTIVATE_MESSAGE, SAS_ACTION) == 1, "second press: released, SAS back off");
  actions(SAS_ACTION); sasInfo(AP_RETROGRADE, 0x3FF); runFor(20);
  tap(s, 1u << 10);
  stubSimpit().sent.clear();
  tap(s, 1u << 10);
  CHECK(sent(SAS_MODE_MESSAGE, AP_RETROGRADE) == 1 && sent(AGDEACTIVATE_MESSAGE) == 0, "release: SAS back to its mode");
  tap(s, 1u << 10);
  b.m(MOD_TIME).release(TIME_SPC); runFor(20);      // to PLN
  CHECK(!holdEngaged(), "control-mode change releases it");
  runFor(100);
  CHECK(s.ledOf(10) == KMC_LED_ENABLED, "PLN: HOLD available (the hold autopilot, stage 7)");
  tap(s, 1u << 10);
  CHECK(!holdEngaged(), "without flight telemetry the hold autopilot refuses, so HOLD does not engage");
  b.m(MOD_TIME).press(TIME_SPC); runFor(20);
  tap(s, 1u << 10);
  b.m(MOD_FUNCTION).press(SG1_MSTR); b.m(MOD_FUNCTION).release(SG1_MSTR); runFor(30);
  CHECK(!holdEngaged(), "MSTR RESET releases it");
}

static void testActionAux(Bench &b) {
  printf("panels: Action Control\n");
  ModuleSim &a = b.m(MOD_ACTION), &x = b.m(MOD_AUX), &de = b.m(MOD_DUALENC);
  de.turn(0, 3); runFor(20);
  stubSimpit().sent.clear();
  tap(a, 1u << 10); tap(a, 1u << 1);
  CHECK(sent(CAGTOGGLE_MESSAGE, 1) == 1 && sent(CAGTOGGLE_MESSAGE, 12) == 1, "AG 1 and AG 12: CAG 1, 12, whatever the group");
  cags({ 1 }); runFor(100);
  CHECK(a.ledOf(10) == KMC_LED_ACTIVE && a.ledOf(1) == KMC_LED_ENABLED, "lit while active");
  de.edge(0x01); runFor(20);

  printf("panels: AUX CTRL\n");
  runFor(100);
  CHECK(x.ledOf(4) == KMC_LED_OFF && x.ledOf(9) == KMC_LED_OFF && x.ledOf(8) == KMC_LED_ENABLED, "EVA group dark outside EVA");
  stubSimpit().sent.clear();
  tap(x, 1u << 4);
  CHECK(sent(KEYBOARD_EMULATOR) == 0, "and ignored");
  eva(true); runFor(100);
  CHECK(x.ledOf(4) == KMC_LED_ENABLED, "EVA: backlit");
  tap(x, 1u << 4); tap(x, 1u << 1);
  CHECK(keys(VK_SPACE, 0) == 1 && keys(VK_R, 0) == 1, "JUMP: Space, JET PACK: R");
  runFor(40);
  CHECK(x.ledOf(1) == KMC_LED_ACTIVE, "flashes on the press");
  runFor(200);
  CHECK(x.ledOf(1) == KMC_LED_ENABLED, "then backlit");
  eva(false); runFor(20);
  stubSimpit().sent.clear();
  tap(x, 1u << 8);
  CHECK(sent(CAGTOGGLE_MESSAGE, 37) == 1, "UNDOCK: CAG 37");
  cags({ 36 }); runFor(20);
  stubSimpit().sent.clear();
  tap(x, 1u << 10);
  CHECK(sent(CAGDEACTIVATE_MESSAGE, 36) == 1 && sent(CAGACTIVATE_MESSAGE, 35) == 1, "PRI: ALT off, PRI on");
  cags({ 35 }); runFor(100);
  CHECK(x.ledOf(10) == KMC_LED_ACTIVE && x.ledOf(11) == KMC_LED_ENABLED, "PRI lit");
  stubSimpit().sent.clear();
  tap(x, 1u << 10);
  CHECK(stubSimpit().sent.empty(), "pressing the active one does nothing");

  printf("panels: Dual Encoder\n");
  de.turn(0, -2); runFor(20);
  CHECK(g_panel.ctrlGrp == 5 && b.rig.ann.cmds.back()[2] == 5, "ENC1 back from 1 wraps to 5; Annunciator shows it");
  de.edge(0x01); runFor(20);
  stubSimpit().sent.clear();
  de.turn(1, 2); runFor(20); de.turn(1, -1); runFor(20); de.edge(0x02); runFor(20);
  CHECK(keys(VK_OEM_MINUS, 0) == 2 && keys(VK_OEM_PLUS, 0) == 1 && keys(VK_BACKSPACE, 0) == 1,
        "ENC2: next ('-') twice, previous ('=') once; button: Backspace");
}

static void testDemoAndKeys(Bench &b) {
  printf("panels: keys released when the flight ends\n");
  ModuleSim &s = b.m(MOD_STABILITY);
  s.press(1u << 11); runFor(20);
  stubSimpit().sent.clear();
  leaveFlight();
  CHECK(keys(VK_F, KEY_UP_MOD) == 1 && !keyHeld(VK_F), "F released on leaving the flight");
  s.release(1u << 11);

  printf("panels: demo\n");
  ModuleSim &v = b.m(MOD_VEHICLE), &x = b.m(MOD_AUX);
  v.release(SG2_CHUTE); runFor(150);
  b.m(MOD_FUNCTION).press(SG1_SCE); runFor(200);
  stubSimpit().sent.clear();
  v.press(1u << 4); runFor(100);
  CHECK(v.ledOf(4) == KMC_LED_ACTIVE && v.ledOf(5) == KMC_LED_ENABLED, "held: ACTIVE, others backlit");
  CHECK(v.ledOf(10) == KMC_LED_OFF, "CHUTE SAFE still darkens the chutes");
  CHECK(x.ledOf(4) == KMC_LED_ENABLED, "EVA buttons backlit");
  v.release(1u << 4); runFor(100);
  CHECK(v.ledOf(4) == KMC_LED_ENABLED && stubSimpit().sent.empty(), "released: backlit; nothing sent");
  b.m(MOD_FUNCTION).release(SG1_SCE); runFor(150);
}

void runStage4() {
  static Bench b;
  stubEepromReset();
  testVehicle(b);
  testStability(b);
  testActionAux(b);
  testDemoAndKeys(b);
  for (int a = 0; a < 128; a++) Wire.detach(a);
}
