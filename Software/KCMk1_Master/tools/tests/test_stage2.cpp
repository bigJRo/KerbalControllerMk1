// Stage 2 selftest: the module bus — discovery, INT line learning and the stored map,
// lifecycle, events (press / release / tap / re-press / hold), every packet kind, LED
// frames, packet checks, reboot recovery, re-probe, polling, the console.
#include "harness.h"

// Events seen by the test handler.
static std::vector<ModEvent> g_ev;
static void recordEvent(const ModEvent &e) { ModEvent c = e; c.pkt = nullptr; g_ev.push_back(c); }
static void hookAll() { for (uint8_t i = 0; i < MOD_COUNT; i++) moduleBusSetHandler((ModId)i, recordEvent); }

// powerOnBoot() with every module's events recorded, and the panel tabs out of the way
// (they would set the LEDs this file sets by hand).
static void boot2(Bench &b, uint32_t srsr = 1) { powerOnBoot(b, srsr); hookAll(); panelsSetEnabled(false); }

static void testDiscovery(Bench &b) {
  printf("modules: discovery and INT learning at power-on\n");
  stubEepromReset();
  boot2(b);
  CHECK(b.rig.si.shows("INT MAP DEFAULT"), "no stored map on a fresh EEPROM");
  CHECK(b.rig.si.shows("MODULES 13/13"), "all 13 found");
  CHECK(b.rig.si.sevOf("VEHICLE OK") == SEV_OK && serialHas("24 VEHICLE     B_INT_4  OK"), "vehicle OK on B_INT_4");
  bool allLearned = true, allDisabled = true, ledsOff = true, intsHigh = true;
  for (uint8_t i = 0; i < MOD_COUNT; i++) {
    uint8_t l = intLineOf(i);
    if (l != b.mods[i].line || intLineSource(l) != SRC_LEARNED) allLearned = false;
    if (b.mods[i].life != KMC_STATUS_DISABLED || !modulePresent((ModId)i)) allDisabled = false;
    if (moduleHasLeds((ModId)i) && b.mods[i].countCmd(KMC_CMD_SET_LED_STATE) != 1) ledsOff = false;
    if (b.mods[i].intLow) intsHigh = false;
  }
  CHECK(allLearned, "every module's line learned");
  CHECK(allDisabled, "every module DISABLED after boot");
  CHECK(ledsOff, "LED modules got one all-OFF frame");
  CHECK(intsHigh, "every boot INT released");
  CHECK(intLineOf(OWNER_ANNUNCIATOR) == L_INT(9) && intLineSource(L_INT(9)) == SRC_LEARNED, "annunciator line learned");
  CHECK(intLineOf(OWNER_INFODISP_2) == L_BINT(9) && intLineOf(OWNER_RESDISP) == L_BINT(12), "display lines learned");
  CHECK(b.rig.si.shows("DISP INT 4/4"), "display learning reported");
  CHECK(EEPROM.read(0) == 'K' && EEPROM.read(1) == 'I', "map stored in EEPROM");
}

static void testRecable(Bench &b) {
  printf("modules: re-cabled slots are learned and stored\n");
  b.m(MOD_VEHICLE).moveTo(L_BINT(1));      // Vehicle moved from P4 to P1
  b.m(MOD_TIME).moveTo(L_BINT(5));         // Time moved from P3 to P5
  boot2(b);
  CHECK(b.rig.si.shows("INT MAP STORED"), "stored map loaded");
  CHECK(intLineOf(MOD_VEHICLE) == L_BINT(1) && intLineOf(MOD_TIME) == L_BINT(5), "new lines learned");
  CHECK(intLineOwner(L_BINT(4)) == OWNER_NONE && intLineOwner(L_BINT(3)) == OWNER_NONE, "old lines freed");
  CHECK(b.rig.si.sevOf("VEHICLE INT LEARNED") == SEV_WARN, "re-cabling shown");
  // Master restarts on its own: modules running, INT high — the stored map is used.
  for (auto &m : b.mods) { m.cmds.clear(); m.life = KMC_STATUS_DISABLED; m.setInt(false); }
  b.attachAll(); b.rig.si.frames.clear();
  resetStubs(); stubSrsr() = 1u << 1;      // software reset
  setup(); hookAll(); panelsSetEnabled(false);
  CHECK(intLineOf(MOD_VEHICLE) == L_BINT(1) && intLineSource(L_BINT(1)) == SRC_STORED, "restart uses the stored map");
  CHECK(b.rig.si.sevOf("VEHICLE OK") == SEV_OK, "no re-learning needed");
  // Put the cabling back for the rest of the tests.
  b.m(MOD_VEHICLE).moveTo(L_BINT(4)); b.m(MOD_TIME).moveTo(L_BINT(3));
  boot2(b);
  CHECK(intLineOf(MOD_VEHICLE) == L_BINT(4), "cabling restored");
}

static void testLifecycle(Bench &b) {
  printf("modules: lifecycle follows the flight scene and demo\n");
  boot2(b);
  runFor(100);
  CHECK(b.m(MOD_VEHICLE).life == KMC_STATUS_DISABLED, "disabled with KSP down");
  enterFlight();
  bool all = true;
  for (auto &m : b.mods) if (m.life != KMC_STATUS_ACTIVE) all = false;
  CHECK(all, "all ENABLEd in a flight scene");
  CHECK(b.m(MOD_UI).ledWrites >= 2, "LEDs resent after ENABLE");
  CHECK(kmcLedPackGet(b.m(MOD_UI).led, 5) == KMC_LED_ENABLED, "buttons backlit by default");
  leaveFlight();
  CHECK(b.m(MOD_VEHICLE).life == KMC_STATUS_DISABLED, "DISABLEd on leaving flight");
  g_master.demo = true; runFor(20);
  CHECK(b.m(MOD_VEHICLE).life == KMC_STATUS_ACTIVE, "ENABLEd in demo without a flight");
  g_master.demo = false; runFor(20);
  CHECK(b.m(MOD_VEHICLE).life == KMC_STATUS_DISABLED, "back to DISABLED after demo");
  enterFlight();
}

static int countEv(ModId id, uint32_t ModEvent::*field, uint32_t bit) {
  int n = 0; for (auto &e : g_ev) if (e.id == id && (e.*field & bit)) n++; return n;
}

static void testEvents(Bench &b) {
  printf("modules: button events\n");
  ModuleSim &v = b.m(MOD_VEHICLE);
  g_ev.clear();
  v.press(1u << 0); runFor(10);
  CHECK(countEv(MOD_VEHICLE, &ModEvent::pressed, 1) == 1, "press");
  v.release(1u << 0); runFor(10);
  CHECK(countEv(MOD_VEHICLE, &ModEvent::released, 1) == 1, "release");
  g_ev.clear();
  v.press(1u << 1); v.release(1u << 1); runFor(10);     // both before the next read
  CHECK(countEv(MOD_VEHICLE, &ModEvent::tapped, 2) == 1 && countEv(MOD_VEHICLE, &ModEvent::pressed, 2) == 0, "tap");
  g_ev.clear();
  v.press(1u << 2); runFor(10);
  v.release(1u << 2); v.press(1u << 2); runFor(10);     // release + re-press between reads
  CHECK(countEv(MOD_VEHICLE, &ModEvent::pressed, 4) == 2, "re-press counts as a press");
  g_ev.clear();
  runFor(2100);
  CHECK(countEv(MOD_VEHICLE, &ModEvent::held, 4) == 1, "held once after 2 s");
  runFor(2100);
  CHECK(countEv(MOD_VEHICLE, &ModEvent::held, 4) == 1, "held reported only once per press");
  v.release(1u << 2); runFor(10);
  g_ev.clear();
  v.press(1u << 20); runFor(10);                        // Switch Group 2 input (B20)
  CHECK(countEv(MOD_VEHICLE, &ModEvent::pressed, 1u << 20) == 1 && (g_ev.back().down & (1u << 20)), "24-input plane");
  v.release(1u << 20); runFor(10);
  ModuleSim &f = b.m(MOD_FUNCTION);
  g_ev.clear(); f.press(1u << 23); runFor(10);
  CHECK(countEv(MOD_FUNCTION, &ModEvent::pressed, 1u << 23) == 1, "Switch Group 1 B23");
  f.release(1u << 23); runFor(10);
  ModuleSim &u = b.m(MOD_UI);
  g_ev.clear(); u.press(1u << 13); runFor(10);
  CHECK(countEv(MOD_UI, &ModEvent::pressed, 1u << 13) == 1, "16-input plane, second byte (B13)");
  u.release(1u << 13); runFor(10);

  printf("modules: other packet kinds\n");
  ModuleSim &r = b.m(MOD_ROTATION);
  g_ev.clear(); r.axes[0] = 1234; r.press(1u << 0); runFor(10);
  CHECK(countEv(MOD_ROTATION, &ModEvent::pressed, 1) == 1, "joystick button from the state byte");
  CHECK(!g_ev.empty() && g_ev.back().len == KMC_JOYSTICK_PACKET_SIZE, "joystick packet length");
  r.release(1u << 0); runFor(10);
  ModuleSim &gp = b.m(MOD_GPWS);
  g_ev.clear(); gp.edge(1u << 1); runFor(10);
  CHECK(countEv(MOD_GPWS, &ModEvent::pressed, 2) == 1, "display module rising edge");
  ModuleSim &t = b.m(MOD_THROTTLE);
  g_ev.clear(); t.edge(1u << 3); runFor(10);
  CHECK(countEv(MOD_THROTTLE, &ModEvent::pressed, 8) == 1, "throttle button event");
  ModuleSim &de = b.m(MOD_DUALENC);
  g_ev.clear(); de.edge(1u << 0); runFor(10);
  CHECK(countEv(MOD_DUALENC, &ModEvent::pressed, 1) == 1, "dual encoder button");
}

static void testLeds(Bench &b) {
  printf("modules: LED frames\n");
  ModuleSim &a = b.m(MOD_AUX);
  int w0 = a.ledWrites;
  moduleSetLed(MOD_AUX, 0, KMC_LED_ACTIVE);
  moduleSetLed(MOD_AUX, 11, KMC_LED_ACTIVE_ALT);
  runFor(60);
  CHECK(a.ledWrites == w0 + 1, "one frame for the change");
  CHECK(kmcLedPackGet(a.led, 0) == KMC_LED_ACTIVE && kmcLedPackGet(a.led, 11) == KMC_LED_ACTIVE_ALT, "nibbles as the modules unpack them");
  CHECK((a.led[0] & 0x0F) == KMC_LED_ACTIVE, "button 0 in the low nibble (kmcLedPackSet)");
  w0 = a.ledWrites;
  for (int i = 0; i < 10; i++) { moduleSetLed(MOD_AUX, 1, (i & 1) ? KMC_LED_ACTIVE : KMC_LED_ENABLED); runFor(5); }
  CHECK(a.ledWrites - w0 <= 2, "at most one frame per 50 ms (%d)", a.ledWrites - w0);
  runFor(60);
  w0 = a.ledWrites; runFor(200);
  CHECK(a.ledWrites == w0, "no frame when nothing changed");
  leaveFlight();
  w0 = a.ledWrites; moduleSetLed(MOD_AUX, 2, KMC_LED_ACTIVE); runFor(100);
  CHECK(a.ledWrites == w0, "no LED frames while DISABLED");
  enterFlight();
  CHECK(a.ledWrites > w0 && kmcLedPackGet(a.led, 2) == KMC_LED_ACTIVE, "wanted LEDs sent on ENABLE");
  moduleBulbTest(true);
  bool all = true; for (auto &m : b.mods) if (m.countCmd(KMC_CMD_BULB_TEST) < 1) all = false;
  CHECK(all, "bulb test to every module");
  // The modules do not block: LED changes still go out during the test, and the stop
  // follows the release once the test has shown for MODBUS_BULB_MIN_MS.
  moduleSetLed(MOD_AUX, 3, KMC_LED_ACTIVE);
  runFor(100);
  CHECK(kmcLedPackGet(a.led, 3) == KMC_LED_ACTIVE, "LED changes sent during the test");
  moduleBulbTest(false);
  runFor(100);
  CHECK(a.countCmd(KMC_CMD_BULB_TEST) == 1, "a quick release: the test still shows for its minimum");
  runFor(MODBUS_BULB_MIN_MS);
  const std::vector<uint8_t> *lastBulb = nullptr;
  for (auto &c : a.cmds) if (c[0] == KMC_CMD_BULB_TEST) lastBulb = &c;
  CHECK(a.countCmd(KMC_CMD_BULB_TEST) == 2 && lastBulb && lastBulb->size() == 2 && (*lastBulb)[1] == 0,
        "then the stop (0x00)");
  moduleBulbTest(true); runFor(20); moduleBulbTest(false); moduleBulbTest(true); runFor(MODBUS_BULB_MIN_MS + 100);
  CHECK(a.countCmd(KMC_CMD_BULB_TEST) == 3, "on, off and on again before the stop: one start, no stop");
  moduleBulbTest(false); runFor(MODBUS_BULB_MIN_MS + 100);
  CHECK(a.countCmd(KMC_CMD_BULB_TEST) == 4, "and the stop on the final release");
}

static void testHealth(Bench &b) {
  printf("modules: packet checks and recovery\n");
  ModuleSim &s = b.m(MOD_STABILITY);
  s.cnt += 3; s.press(1u << 4); runFor(10);
  s.release(1u << 4); runFor(10);
  Serial.out.clear(); Serial.stubType("modules\n"); runFor(10);
  CHECK(Serial.out.find("missed 3") != std::string::npos, "transaction-counter gap counted");
  s.fault = true; s.press(1u << 5); runFor(10);
  CHECK(s.countCmd(KMC_CMD_ACK_FAULT) == 1 && !s.fault, "fault acknowledged");
  s.release(1u << 5); runFor(10);
  // The module reboots mid-flight.
  int en = s.countCmd(KMC_CMD_ENABLE);
  s.powerOn(); runFor(20);
  CHECK(s.life == KMC_STATUS_ACTIVE && s.countCmd(KMC_CMD_ENABLE) == en + 1, "rebooted module brought back to ACTIVE");
  // Wrong part at an address.
  ModuleSim wrong(0x25, KMC_TYPE_UI_CONTROL, KMC_BUTTON_PACKET_SIZE, L_BINT(3), KIND_BUTTONS);
  b.attachAll(); Wire.attach(0x25, &wrong);
  b.rig.si.frames.clear(); resetStubs(); b.powerOnAll(); wrong.powerOn(); stubSrsr() = 1;
  setup(); hookAll(); panelsSetEnabled(false);
  CHECK(b.rig.si.sevOf("TIME WRONG TYPE") == SEV_ERROR, "wrong module type reported");
  CHECK(!modulePresent(MOD_TIME), "wrong module not used");
}

static void testReprobe(Bench &b) {
  printf("modules: late module and polling\n");
  b.attachAll(); Wire.detach(0x2B);
  b.rig.si.frames.clear(); resetStubs(); b.powerOnAll(); stubSrsr() = 1;
  stubSimpit().kspUp = false;
  setup(); hookAll(); panelsSetEnabled(false);
  CHECK(b.rig.si.sevOf("PRE-WARP MISSING") == SEV_ERROR && !modulePresent(MOD_PREWARP), "missing at boot");
  Wire.attach(0x2B, &b.m(MOD_PREWARP));
  runFor(5100);
  CHECK(modulePresent(MOD_PREWARP) && b.m(MOD_PREWARP).life == KMC_STATUS_DISABLED, "found by the re-probe");
  enterFlight();
  CHECK(b.m(MOD_PREWARP).life == KMC_STATUS_ACTIVE, "and enabled with the rest");
  // A module that stops answering is retired, then found again.
  Wire.detach(0x26);
  b.m(MOD_AUX).press(1); runFor(100);
  CHECK(!modulePresent(MOD_AUX), "a module that stops answering is retired");
  b.m(MOD_AUX).release(1);
  Wire.attach(0x26, &b.m(MOD_AUX));
  runFor(5100);
  CHECK(modulePresent(MOD_AUX) && b.m(MOD_AUX).life == KMC_STATUS_ACTIVE, "and comes back by the re-probe");

  // A module with no known line is polled.
  stubEeprom()[3 + L_BINT(6)] = OWNER_NONE;    // remove Stability from the stored map
  uint8_t o[INT_LINE_COUNT]; for (int i = 0; i < INT_LINE_COUNT; i++) o[i] = stubEeprom()[3 + i];
  uint8_t sum = 0x5A; for (int i = 0; i < INT_LINE_COUNT; i++) sum = (uint8_t)((sum << 1 | sum >> 7) ^ o[i]);
  stubEeprom()[3 + INT_LINE_COUNT] = sum;
  ModuleSim &s = b.m(MOD_STABILITY);
  for (auto &m : b.mods) { m.cmds.clear(); m.life = KMC_STATUS_DISABLED; m.setInt(false); }
  b.attachAll(); b.rig.si.frames.clear(); resetStubs(); stubSrsr() = 1;
  setup(); hookAll(); panelsSetEnabled(false);
  CHECK(intLineOf(MOD_STABILITY) == 0xFF && b.rig.si.shows("STABILITY OK") && serialHas("POLLED"), "no line: polled");
  enterFlight();
  s.setInt(false);
  g_ev.clear(); s.state |= 1; s.change |= 1; s.cnt++;    // a press with no INT
  runFor(30);
  CHECK(countEv(MOD_STABILITY, &ModEvent::pressed, 1) == 1, "polled press seen");
  boot2(b);                                         // relearn for the next tests
}

static void testWatchdogRestart(Bench &b) {
  printf("modules: watchdog restart in flight does not reset the modules\n");
  boot2(b);
  enterFlight();
  for (auto &m : b.mods) m.cmds.clear();
  b.attachAll(); b.rig.si.frames.clear(); resetStubs(); stubSrsr() = 1u << 4;
  stubRetain()[0] = stubRetain()[1] = stubRetain()[2] = 0;
  setup(); hookAll(); panelsSetEnabled(false);
  bool noDisable = true; for (auto &m : b.mods) if (m.countCmd(KMC_CMD_DISABLE) || m.life != KMC_STATUS_ACTIVE) noDisable = false;
  CHECK(noDisable, "modules left ACTIVE");
}

static void testModuleConsole() {
  printf("modules: console\n");
  Serial.out.clear(); Serial.stubType("modules\n"); runFor(10);
  CHECK(Serial.out.find("VEHICLE") != std::string::npos && Serial.out.find("B_INT_4") != std::string::npos, "modules table");
  Serial.out.clear(); Serial.stubType("intmap\n"); runFor(10);
  CHECK(Serial.out.find("B_INT_12") != std::string::npos && Serial.out.find("RESOURCE") != std::string::npos, "intmap table");
  Serial.stubType("intmap clear\n"); runFor(10);
  CHECK(EEPROM.read(0) != 'K', "intmap clear erases the stored map");
}

void runStage2() {
  static Bench b;
  testDiscovery(b);
  testRecable(b);
  testLifecycle(b);
  testEvents(b);
  testLeds(b);
  testHealth(b);
  testReprobe(b);
  testWatchdogRestart(b);
  testModuleConsole();
  for (int a = 0; a < 128; a++) Wire.detach(a);
}
