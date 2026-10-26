// Stage 6 selftest: the remaining panels — UI Control (keys, the screenshot macro,
// navball), Function Control (CAG buttons), Time Control (warp-to with the Pre-Warp
// lead and availability, warp, physics warp, LOAD / PAUSE / SAVE), the GPWS relay.
#include "harness.h"
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

void runStage6() {
  static Bench b;
  stubEepromReset();
  testUi(b);
  testFunction(b);
  testTime(b);
  testGpws(b);
  for (int a = 0; a < 128; a++) Wire.detach(a);
}
