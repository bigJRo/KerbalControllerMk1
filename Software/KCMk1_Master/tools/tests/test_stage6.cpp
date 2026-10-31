// Stage 6 selftest: the remaining panels — UI Control (keys, the screenshot macro,
// navball), Function Control (CAG buttons), Time Control (warp-to with the Pre-Warp
// lead and availability, warp, physics warp, LOAD / PAUSE / SAVE), the GPWS relay.
#include "harness.h"
#include "input_test.h"
#include "module_bus.h"
#include "keys.h"
#include "cag.h"

static const uint32_t SG1_AP = 1u << 21, SG1_ENGINE = 1u << 22;
static const uint32_t TIME_SPC = 1u << 12;

static uint32_t g_sg1, g_time;
static void preset(Bench &b) { b.m(MOD_FUNCTION).state = g_sg1; b.m(MOD_TIME).state = g_time; }

static void tap(ModuleSim &m, uint32_t bit) { m.press(bit); m.release(bit); runFor(20); }
static int keys(int16_t vk, uint8_t mod) {
  int n = 0;
  for (auto &s : stubSimpit().sent)
    if (s.type == KEYBOARD_EMULATOR && s.b.size() == 3 && s.b[0] == mod && (int16_t)(s.b[1] | s.b[2] << 8) == vk) n++;
  return n;
}
static int sent(byte type, int b0 = -1) {
  int n = 0;
  for (auto &s : stubSimpit().sent) if (s.type == type && (b0 < 0 || (!s.b.empty() && s.b[0] == b0))) n++;
  return n;
}
static const SimpitSent *last(byte type) {
  auto &v = stubSimpit().sent;
  for (auto it = v.rbegin(); it != v.rend(); ++it) if (it->type == type) return &*it;
  return nullptr;
}
static void situation(uint8_t sit) {
  flightStatusMessage fs = {}; fs.flightStatusFlags = FLIGHT_IN_FLIGHT; fs.vesselSituation = sit;
  inbound(FLIGHT_STATUS_MESSAGE, &fs, sizeof fs);
}
static void twIndex(uint8_t i, uint8_t sit) {
  flightStatusMessage fs = {}; fs.flightStatusFlags = FLIGHT_IN_FLIGHT; fs.vesselSituation = sit; fs.currentTWIndex = i;
  inbound(FLIGHT_STATUS_MESSAGE, &fs, sizeof fs);
}
static void orbit(float e) { orbitInfoMessage m = {}; m.eccentricity = e; inbound(ORBIT_MESSAGE, &m, sizeof m); }
static void node(float dv) { maneuverMessage m = {}; m.deltaVNextManeuver = dv; inbound(MANEUVER_MESSAGE, &m, sizeof m); }
static void atmo(bool in) { atmoConditionsMessage m = {}; m.atmoCharacteristics = in ? (HAS_ATMOSPHERE | IS_IN_ATMOSPHERE) : HAS_ATMOSPHERE; inbound(ATMO_CONDITIONS_MESSAGE, &m, sizeof m); }

static const uint8_t SIT_LANDED = 1, SIT_ORBITING = 32;

static void testUi(Bench &b) {
  printf("panels: UI Control\n");
  g_sg1 = SG1_AP | SG1_ENGINE; g_time = TIME_SPC;
  powerOnBoot(b, 1, preset);
  enterFlight();
  ModuleSim &u = b.m(MOD_UI);
  stubSimpit().sent.clear();
  tap(u, 1u << 2); tap(u, 1u << 1); tap(u, 1u << 8); tap(u, 1u << 7); tap(u, 1u << 11);
  CHECK(keys(VK_F2, 0) == 1 && keys(VK_F12, ALT_MOD) == 1 && keys(VK_TAB, SHIFT_MOD) == 1 && keys(VK_OEM_6, 0) == 1 &&
        keys(VK_C, 0) == 1, "UI F2, DEBUG Alt+F12, MAP < Shift+Tab, SHIP > ], IVA C");
  tap(u, 1u << 5);
  CHECK(sent(NAVBALLMODE_MESSAGE) == 1, "CYCLE NAV: navball mode");
  runFor(30);
  CHECK(u.ledOf(5) == KMC_LED_ACTIVE, "flashes on the press");
  runFor(200);
  CHECK(u.ledOf(5) == KMC_LED_ENABLED, "then backlit");
  stubSimpit().sent.clear();
  tap(u, 1u << 0);
  CHECK(keys(VK_F2, 0) == 1 && keys(VK_F1, 0) == 0, "SCREEN SHOT: F2 first");
  runFor(400);
  CHECK(keys(VK_F2, 0) == 2 && keys(VK_F1, 0) == 1, "then F1, then F2, spaced out");
}

static void testFunction(Bench &b) {
  printf("panels: Function Control\n");
  ModuleSim &f = b.m(MOD_FUNCTION), &de = b.m(MOD_DUALENC);
  stubSimpit().sent.clear();
  tap(f, 1u << 4); tap(f, 1u << 1);
  CHECK(sent(CAGTOGGLE_MESSAGE, 39) == 1 && sent(CAGTOGGLE_MESSAGE, 25) == 1, "FLAPS CAG 39, LES CAG 25");
  de.turn(0, 1); runFor(20);
  tap(f, 1u << 4);
  CHECK(sent(CAGTOGGLE_MESSAGE, 79) == 1, "control group 2: CAG 79");
  uint8_t st[32] = { 0 }; st[79 / 8] |= 1u << (79 % 8);
  inbound(CAGSTATUS_MESSAGE, st, 32); runFor(100);
  CHECK(f.ledOf(4) == KMC_LED_ACTIVE && f.ledOf(5) == KMC_LED_ENABLED, "lit while KSP reports it");
  de.edge(0x01); runFor(20);
}

static void testTime(Bench &b) {
  printf("panels: Time Control\n");
  ModuleSim &t = b.m(MOD_TIME), &pw = b.m(MOD_PREWARP);
  pw.dispValue = 5; pw.signal(); runFor(20);
  CHECK(timePreWarpMinutes() == 5, "Pre-Warp lead read (5 min)");
  situation(SIT_LANDED); runFor(100);
  CHECK(t.ledOf(0) == KMC_LED_OFF && t.ledOf(2) == KMC_LED_OFF && t.ledOf(6) == KMC_LED_OFF && t.ledOf(8) == KMC_LED_ENABLED,
        "landed: ApA, PeA, SOI dark; MORN available");
  CHECK(t.ledOf(4) == KMC_LED_OFF, "MNVR dark without a node");
  stubSimpit().sent.clear();
  tap(t, 1u << 0);
  CHECK(sent(TIMEWARP_TO_MESSAGE) == 0, "ApA ignored while landed");
  tap(t, 1u << 8);
  const SimpitSent *w = last(TIMEWARP_TO_MESSAGE);
  float d = 0; if (w) memcpy(&d, &w->b[1], 4);
  CHECK(w && w->b[0] == TIMEWARP_TO_NEXT_MORNING && d == -300.0f, "MORN: next morning, 5 min early");
  situation(SIT_ORBITING); orbit(0.1f); atmo(false); runFor(100);
  CHECK(t.ledOf(0) == KMC_LED_ENABLED && t.ledOf(2) == KMC_LED_ENABLED && t.ledOf(6) == KMC_LED_ENABLED && t.ledOf(8) == KMC_LED_OFF,
        "orbiting: ApA, PeA, SOI available; MORN dark");
  orbit(1.3f); runFor(100);
  CHECK(t.ledOf(0) == KMC_LED_OFF, "escape orbit: ApA dark");
  atmo(true); runFor(100);
  CHECK(t.ledOf(6) == KMC_LED_OFF, "in the atmosphere: SOI dark");
  node(120.0f); runFor(100);
  CHECK(t.ledOf(4) == KMC_LED_ENABLED, "MNVR available with a node");
  stubSimpit().sent.clear();
  tap(t, 1u << 4);
  w = last(TIMEWARP_TO_MESSAGE);
  CHECK(w && w->b[0] == TIMEWARP_TO_NEXT_BURN, "MNVR: start of the burn");
  tap(t, 1u << 1);
  CHECK(sent(TIMEWARP_MESSAGE, TIMEWARP_CANCEL_AUTOWARP) == 1 && sent(TIMEWARP_MESSAGE, TIMEWARP_X1) == 1, "STOP: cancel, 1x");
  tap(t, 1u << 5); tap(t, 1u << 7);
  CHECK(sent(TIMEWARP_MESSAGE, TIMEWARP_UP) == 1 && sent(TIMEWARP_MESSAGE, TIMEWARP_DOWN) == 1, "> up, < down");
  twIndex(1, SIT_ORBITING);
  t.press(1u << 3); runFor(20);
  tap(t, 1u << 5);
  CHECK(sent(TIMEWARP_MESSAGE, TIMEWARP_X3_PHYSICAL) == 1, "PHYS + >: physics warp up a step (2x to 3x)");
  twIndex(0, SIT_ORBITING);
  tap(t, 1u << 7);
  CHECK(sent(TIMEWARP_MESSAGE, TIMEWARP_X1_PHYSICAL) == 1, "PHYS + < at 1x: stays 1x");
  runFor(60);
  CHECK(t.ledOf(3) == KMC_LED_ACTIVE, "PHYS lit while held");
  t.release(1u << 3); runFor(20);
  t.press(1u << 9); runFor(20);
  CHECK(keys(VK_F9, KEY_DOWN_MOD) == 1 && keyHeld(VK_F9), "LOAD: F9 held");
  t.release(1u << 9); runFor(20);
  CHECK(keys(VK_F9, KEY_UP_MOD) == 1, "and let go");
  tap(t, 1u << 10); tap(t, 1u << 11);
  CHECK(keys(VK_ESCAPE, 0) == 1 && keys(VK_F5, 0) == 1, "PAUSE Esc, SAVE F5");
}

static void testGpws(Bench &b) {
  printf("panels: GPWS relay\n");
  ModuleSim &g = b.m(MOD_GPWS);
  g.dispState = 0x05; g.dispValue = 300; g.signal(); runFor(150);
  const auto &c = b.rig.ann.cmds.back();
  CHECK(c[6] == 0x05 && c[7] == 0x01 && c[8] == 0x2C, "Annunciator gets the mode, prox alarm and 300 m");
  g.dispState = 0x09; g.dispValue = 1500; g.signal(); runFor(150);
  const auto &c2 = b.rig.ann.cmds.back();
  CHECK(c2[6] == 0x09 && (c2[7] << 8 | c2[8]) == 1500, "and follows it");
}

static void testInputTest(Bench &b) {
  printf("input test screen (Info Display 1)\n");
  b.rig.id1.cmds.clear(); runFor(600);
  CHECK(!b.rig.id1.lastFrame(SYNC_INPUT_TEST), "nothing while the screen is closed");
  b.rig.id1.screen = INPUT_TEST_SCREEN; runFor(600);
  CHECK(inputTestOpen(), "the master sees it open (status byte 2)");
  int n0 = 0, n1 = 0;
  for (auto &c : b.rig.id1.cmds) if (c.size() >= 2 && c[0] == SYNC_INPUT_TEST) (c[1] == 0 ? n0 : n1)++;
  CHECK(n0 >= 3 && n1 >= 1, "page 0 every 100 ms, page 1 every 500 ms (%d, %d)", n0, n1);
  b.m(MOD_ACTION).press(1u << 3); b.m(MOD_VEHICLE).press(1u << 20);
  ModuleSim &r = b.m(MOD_ROTATION); r.axes[0] = 16384; r.axes[1] = -32767; r.press(0x02);
  b.m(MOD_THROTTLE).thrPos = 16383; b.m(MOD_THROTTLE).touched = true; b.m(MOD_THROTTLE).signal();
  b.m(MOD_DUALENC).enc[1] = 3; b.m(MOD_DUALENC).signal();
  stubPin(PIN_SAFE_STAGE) = HIGH;
  b.m(MOD_UI).fault = true; b.m(MOD_UI).signal();
  b.rig.ann.flags = 0x05; b.rig.res.flags = 0x0B; b.rig.id2.screen = 12;
  runFor(1100);
  std::vector<uint8_t> p0, p1, p2;
  for (auto &c : b.rig.id1.cmds) if (c.size() >= 2 && c[0] == SYNC_INPUT_TEST) (c[1] == 0 ? p0 : c[1] == 1 ? p1 : p2) = c;
  CHECK(p0.size() == INPUT_TEST_INPUTS_LEN && p1.size() == INPUT_TEST_MODULES_LEN && p2.size() == INPUT_TEST_DISPLAYS_LEN,
        "sizes 40, 24 and 24");
  uint16_t present = (uint16_t)(p0[2] | p0[3] << 8);
  CHECK(present == (1u << MOD_COUNT) - 1, "every module present (%04X)", present);
  CHECK(p0[13] == 0x08 && p0[19] == 0x10, "the raw state words: a button (ACTION B3) and a switch (VEHICLE B20)");
  CHECK((p0[4] & (1u << MOD_UI)) && !(p0[4] & (1u << MOD_ACTION)), "the UI module's FAULT bit, as sent");
  CHECK(p0[24] == 0x02 && (int8_t)p0[25] == 50 && (int8_t)p0[26] == -100, "rotation BTN01, axes 50 %% and -100 %%");
  CHECK(p0[34] == 49 || p0[34] == 50, "lever 50 %% (%u)", p0[34]);
  CHECK(p0[35] & 0x01, "lever touched");
  CHECK((int8_t)p0[37] == 3, "ENC2 count from its deltas");
  CHECK(p0[39] == 0x02, "STAGE line closed, ABORT open");
  CHECK(p1[2 + MOD_ACTION] == intLineOf(MOD_ACTION) && p1[2 + MOD_ACTION] != 0xFF, "page 1: INT line per module");
  uint32_t life = (uint32_t)(p1[19] | p1[20] << 8 | p1[21] << 16 | (uint32_t)p1[22] << 24);
  bool lifeOk = true;
  for (uint8_t m = 0; m < MOD_COUNT; m++) if (((life >> (2 * m)) & 3) != moduleLifecycle((ModId)m)) lifeOk = false;
  CHECK(lifeOk, "page 1: each module's lifecycle (%08X)", (unsigned)life);
  CHECK(p2[2] == 0x0F, "page 2: all four displays answering");
  CHECK(p2[3] == 0x05 && p2[12] == 0x0B && p2[11] == 12, "page 2: Annunciator and Resource flags, Info Display 2's screen");
  b.m(MOD_UI).fault = false; b.m(MOD_UI).signal();
  b.rig.ann.flags = b.rig.res.flags = 0; b.rig.id2.screen = 0xFF;
  stubPin(PIN_SAFE_STAGE) = LOW;
  b.m(MOD_ACTION).release(1u << 3); b.m(MOD_VEHICLE).release(1u << 20); r.release(0x02);
  r.axes[0] = r.axes[1] = 0; r.signal(); b.m(MOD_THROTTLE).touched = false; b.m(MOD_THROTTLE).signal();
  b.rig.id1.screen = 0; runFor(600);
  b.rig.id1.cmds.clear(); runFor(600);
  CHECK(!b.rig.id1.lastFrame(SYNC_INPUT_TEST) && !inputTestOpen(), "closed: the frames stop");
}

void runStage6() {
  static Bench b;
  stubEepromReset();
  testUi(b);
  testFunction(b);
  testTime(b);
  testGpws(b);
  testInputTest(b);
  for (int a = 0; a < 128; a++) Wire.detach(a);
}
