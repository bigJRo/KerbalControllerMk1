// Stage 1 selftest: boot sequence and reporting, Sys Info Display link, display
// carriers, Simpit link, the KSP send gate, watchdog and the reboot-loop guard, the
// console. Runs setup() / loop() against simulated devices (tools/host/Arduino.h).
#include "harness.h"

/*------------------------------------------------------------------------------------
  Tests
------------------------------------------------------------------------------------*/
static size_t g_logAtFirstInit = 0;
static void captureFirstInit() { if (stubSimpit().initCalls == 1) g_logAtFirstInit = Wire.log.size(); }

static void testNormalBoot(Rig &r) {
  printf("boot: everything present, KSP not running\n");
  r.attachAll(); r.si.frames.clear();
  stubSimpit().kspUp = false;
  stubMillis() = 0; stubRetain()[0] = stubRetain()[1] = stubRetain()[2] = 0;
  resetStubs(); stubSimpit().onInit = captureFirstInit; stubSrsr() = 1;
  setup();
  CHECK(r.si.shows("I2C 400 kHz") && serialHas("CONFIG  I2C 400 kHz"), "CONFIG line (short on Sys Info, long on Serial)");
  CHECK(r.si.shows("MASTER v" KCM_MASTER_VERSION) && serialHas("KCMk1 MASTER v" KCM_MASTER_VERSION), "banner with version");
  CHECK(r.si.shows("RESET: POWER ON"), "reset cause power on");
  CHECK(r.si.sevOf("POWER MON OK") == SEV_OK && r.si.sevOf("FAN CTRL OK") == SEV_OK, "board devices OK");
  CHECK(serialHas("INA228   OK") && serialHas("EMC2101  OK"), "board devices on Serial, long form");
  CHECK(r.si.shows("ANNUNCIATOR OK KSP") && r.si.shows("INFO DISP 1 OK") && r.si.shows("INFO DISP 2 OK") &&
        r.si.shows("RES DISP OK"), "a line per display");
  CHECK(serialHas("RESOURCE DISPLAY"), "display names in full on Serial");
  CHECK(r.si.sevOf("ANNUNCIATOR") == SEV_OK, "annunciator OK");
  CHECK(r.si.shows("KSP NOT RUNNING"), "first KSP attempt reported");
  CHECK(r.si.shows("WATCHDOG 5 S"), "watchdog line");
  CHECK(r.si.shows("KSP WAIT 0 S"), "waiting line");
  CHECK(stubWdt().started && fabs(stubWdt().timeout - 5.0) < 1e-3 && fabs(stubWdt().trigger - 0.5) < 1e-3,
        "watchdog 5 s, warning 0.5 s before");
  CHECK(stubWdt().callback != nullptr, "watchdog warning callback set");
  CHECK(g_master.bootPhase == BOOT_PHASE_WAIT_KSP, "boot phase waiting for KSP");
  CHECK(serialHas("BOOT") && serialHas("WAITING FOR KSP"), "boot lines mirrored to Serial");
  // Displays released before the first KSP handshake attempt.
  bool annBefore = false, infoBefore = false;
  for (size_t i = 0; i < g_logAtFirstInit && i < Wire.log.size(); i++) {
    const WireRec &w = Wire.log[i];
    if (!w.read && w.addr == ADDR_ANNUNCIATOR && w.b.size() == 9 && (w.b[0] >> 4) == 2) annBefore = true;
    if (!w.read && w.addr == ADDR_INFODISP_2 && w.b.size() == 2 && (w.b[0] >> 4) == 2) infoBefore = true;
  }
  CHECK(annBefore && infoBefore, "PROCEED to the displays before KSP is tried");
  CHECK(r.si.count(SI_FRAME_BOOT_DONE) == 0, "boot screen held while KSP is down");
}

static void testCarrierBytes(Rig &r) {
  printf("display commands\n");
  r.ann.cmds.clear(); r.id1.cmds.clear(); r.res.cmds.clear();
  runFor(1000);
  CHECK(r.ann.cmds.size() >= 9 && r.ann.cmds.size() <= 11, "annunciator every 100 ms (%zu in 1 s)", r.ann.cmds.size());
  const std::vector<uint8_t> &a = r.ann.cmds.back();
  CHECK(a.size() == 9, "annunciator 9 bytes");
  if (a.size() == 9) {
    CHECK(a[0] == 0x24, "ctrl PROCEED | audio (%02X)", a[0]);
    CHECK(a[1] == CTRL_MODE_SPC && a[2] == 1, "mode SPC, group 1");
    CHECK((a[3] | (a[4] << 8)) == (1 << MF_AUDIO), "only AUDIO flag (%02X%02X)", a[4], a[3]);
    CHECK(a[5] == 0 && a[6] == 0 && a[7] == 0 && a[8] == 200, "cap 0, GPWS off, threshold 200 m");
  }
  CHECK(!r.res.cmds.empty() && r.res.cmds.back() == std::vector<uint8_t>({ 0x20, 0 }), "resource [20 00]");
  CHECK(!r.id1.cmds.empty() && r.id1.cmds.back() == std::vector<uint8_t>({ 0x20, 0 }), "info display [20 00]");
}

static void testWaitingForKsp(Rig &r) {
  printf("waiting for KSP\n");
  int tries0 = stubSimpit().initCalls;
  runFor(4200);
  CHECK(stubSimpit().initCalls - tries0 == 2, "retry every 2 s (%d)", stubSimpit().initCalls - tries0);
  auto scr = r.si.screen();
  CHECK(!scr.empty() && scr.back().second.find("KSP WAIT ") == 0, "waiting line is the last line");
  CHECK(!scr.empty() && scr.back().second.find(" S") != std::string::npos && serialHas(" S  TRY"),
        "waiting line counts time (and tries, on Serial)");
  size_t lines = scr.size();
  runFor(2000);
  CHECK(r.si.screen().size() == lines, "waiting line replaced in place, not appended");
  runFor(60000, 20);
  CHECK(r.si.shows("CHECK KSP, SIMPIT, USB") && serialHas("KSP RUNNING?"), "hint after 60 s");
  CHECK(r.si.count(SI_FRAME_BOOT_DONE) == 0, "still holding");
  CHECK(r.si.count(SI_FRAME_CONTROL) > 100, "heartbeat while holding");
}

static void testKspConnects(Rig &r) {
  printf("KSP comes up\n");
  stubSimpit().kspUp = true;
  runFor(2100);
  CHECK(g_master.kspConnected, "connected on the next retry");
  CHECK(stubSimpit().handler != nullptr && stubSimpit().registered.size() >= 3, "handler and channels");
  CHECK(r.si.count(SI_FRAME_BOOT_DONE) == 1, "boot screen released");
  CHECK(g_master.bootPhase == BOOT_PHASE_RUN, "phase running");
  int u0 = stubSimpit().updateCalls;
  runFor(100);
  CHECK(stubSimpit().updateCalls - u0 == 20, "update() every pass");
  int st0 = r.si.count(SI_FRAME_STATUS);
  runFor(2000);
  CHECK(r.si.count(SI_FRAME_STATUS) - st0 == 4, "status every 500 ms");
  const std::vector<uint8_t> *s = r.si.last(SI_FRAME_STATUS);
  CHECK(s && s->size() == SI_STATUS_LEN, "status length");
  if (s && s->size() == SI_STATUS_LEN) {
    CHECK(((*s)[11] & SI_LINK_MASTER) && ((*s)[12] & 0x1F) == 0x1F, "master link up, all five known");
  }
}

static void testInbound(Rig &r) {
  printf("inbound messages and SIMPIT LOST\n");
  byte scene = 0;
  inbound(SCENE_CHANGE_MESSAGE, &scene, 1);
  flightStatusMessage fs; memset(&fs, 0, sizeof fs);
  fs.flightStatusFlags = FLIGHT_IN_FLIGHT; fs.currentTWIndex = 3;
  inbound(FLIGHT_STATUS_MESSAGE, &fs, sizeof fs);
  runFor(20);
  CHECK(carrierModeFlags() & (1 << MF_WARP), "WARP from FLIGHT_STATUS");
  const std::vector<uint8_t> &a = r.ann.cmds.back();
  CHECK(a.size() == 9 && (a[3] & (1 << MF_WARP)), "WARP sent to the annunciator");
  byte junk[3] = { 1, 2, 3 };
  fs.currentTWIndex = 0;
  inbound(FLIGHT_STATUS_MESSAGE, junk, 3);
  CHECK(g_vessel.twIndex == 3, "wrong-size FLIGHT_STATUS dropped");
  inbound(FLIGHT_STATUS_MESSAGE, &fs, sizeof fs);
  CHECK(g_vessel.twIndex == 0, "right-size FLIGHT_STATUS used");

  // Silence in flight → SIMPIT LOST at 5 s, re-handshake at 10 s.
  r.ann.cmds.clear();
  runFor(5100);
  CHECK(g_master.simpitLost, "SIMPIT LOST after 5 s of silence in flight");
  CHECK(!r.ann.cmds.empty() && (r.ann.cmds.back()[4] & (1 << (MF_SIMPIT_LOST - 8))), "SIMPIT LOST flag sent");
  inbound(FLIGHT_STATUS_MESSAGE, &fs, sizeof fs);
  runFor(20);
  CHECK(!g_master.simpitLost, "traffic clears SIMPIT LOST");
  inbound(SCENE_CHANGE_MESSAGE, &scene, 1);
  int tries = stubSimpit().initCalls; size_t regs = stubSimpit().registered.size();
  runFor(10200);
  CHECK(stubSimpit().initCalls > tries, "re-handshake after 10 s of silence");
  CHECK(g_master.kspConnected && stubSimpit().registered.size() > regs, "channels registered again");
  scene = 1;
  inbound(SCENE_CHANGE_MESSAGE, &scene, 1);
  runFor(20);
  CHECK(!(carrierModeFlags() & (1 << MF_WARP)) && !g_vessel.flightScene, "leaving flight");
}

static void testDisplayLinks(Rig &r) {
  printf("display link state\n");
  r.ann.flags = 0x01; r.id2.flags = 0x01;
  runFor(1100);
  CHECK(g_master.displayLinks == (SI_LINK_ANNUNCIATOR | SI_LINK_INFODISP_2), "links from status flags (%02X)", g_master.displayLinks);
  const std::vector<uint8_t> *s = r.si.last(SI_FRAME_STATUS);
  CHECK(s && ((*s)[11] & 0x1E) == (SI_LINK_ANNUNCIATOR | SI_LINK_INFODISP_2), "links on the Sys Info status");
  Wire.detach(ADDR_RESDISP);
  runFor(1100);
  CHECK(!(g_master.displaysKnown & SI_LINK_RESDISP), "a display that stops answering is unknown");
  Wire.attach(ADDR_RESDISP, &r.res);
  r.ann.flags = r.id2.flags = 0;
}

static void testKspGate() {
  printf("KSP send gate\n");
  throttleMessage t; t.throttle = 100;
  size_t n0 = stubSimpit().sent.size();
  CHECK(kspSend(THROTTLE_MESSAGE, t) && stubSimpit().sent.size() == n0 + 1, "sent while connected");
  g_master.demo = true;
  CHECK(!kspSend(THROTTLE_MESSAGE, t) && stubSimpit().sent.size() == n0 + 1, "held back in demo");
  g_master.demo = false;
}

static void testConsole() {
  printf("console\n");
  Serial.out.clear();
  Serial.stubType("help\r\n"); runFor(10);
  CHECK(serialHas("status") && serialHas("simpit") && serialHas("log"), "help lists commands");
  Serial.out.clear(); Serial.stubType("status\n"); runFor(10);
  CHECK(serialHas("KCMk1 master v") && serialHas("I2C:"), "status");
  Serial.out.clear(); Serial.stubType("simpit\n"); runFor(10);
  CHECK(serialHas("FLIGHT_STATUS"), "simpit channel table");
  Serial.out.clear(); Serial.stubType("bogus\n"); runFor(10);
  CHECK(serialHas("unknown command"), "unknown command");
  Serial.out.clear(); Serial.stubType("log KSP on\n"); runFor(10);
  CHECK(dbgEnabled(DBG_KSP) && !dbgEnabled(DBG_I2C), "log KSP on forces one category");
  Serial.stubType("log KSP auto\n"); runFor(10);
  CHECK(!dbgEnabled(DBG_KSP), "log KSP auto follows debug again");
  Serial.out.clear(); Serial.stubType("debug on\n"); runFor(1100);
  CHECK(dbgEnabled(DBG_I2C) && serialHas("TIMING"), "debug on: categories print, loop timing line");
  Serial.stubType("debug off\n"); runFor(10);
  Serial.out.clear(); Serial.stubType("wdtest\n"); runFor(10);
  CHECK(serialHas("wdtest yes"), "wdtest needs confirming");
}

static void testDebugFlood() {
  printf("debug rate limit\n");
  g_master.debug = true;
  Serial.out.clear();
  for (int i = 0; i < 1000; i++) dbg(DBG_STATE, "flood %d", i);
  size_t lines = 0; for (char c : Serial.out) if (c == '\n') lines++;
  CHECK(lines <= 200, "at most 200 lines a second (%zu)", lines);
  runFor(1100);
  CHECK(serialHas("lines suppressed"), "suppressed count reported");
  Serial.out.clear();
  Serial.writeRoom = 0;
  dbg(DBG_STATE, "no room");
  CHECK(Serial.out.empty() && dbgDropped() > 0, "a full USB buffer drops the line instead of blocking");
  Serial.writeRoom = 4096;
  g_master.debug = false;
}

static void testAbsentParts(Rig &r) {
  printf("boot: missing parts\n");
  r.attachAll(); r.si.frames.clear();
  Wire.detach(ADDR_ANNUNCIATOR); Wire.detach(ADDR_INA228);
  stubSimpit().kspUp = false;
  boot(1);
  CHECK(r.si.sevOf("ANNUNCIATOR") == SEV_ERROR && r.si.shows("NO ANSWER"), "missing display in red");
  CHECK(r.si.sevOf("POWER MON MISSING") == SEV_ERROR, "missing INA228 in red");
  CHECK(r.si.shows("WATCHDOG 5 S"), "boot carries on");
  r.emc.regs[0xFD] = { 0x99 };
  r.attachAll(); r.si.frames.clear();
  boot(1);
  CHECK(r.si.sevOf("FAN CTRL MISSING") == SEV_ERROR, "wrong part ID is not OK");
  r.emc.regs[0xFD] = { 0x16 };
}

static void testSysInfoLate(Rig &r) {
  printf("boot: Sys Info Display late\n");
  r.attachAll(); r.si.frames.clear();
  Wire.detach(ADDR_SYSINFO);
  stubSimpit().kspUp = false;
  boot(1);
  CHECK(r.si.frames.empty() && serialHas("WAITING FOR KSP"), "boot on Serial only");
  Wire.attach(ADDR_SYSINFO, &r.si);
  runFor(5200);
  CHECK(r.si.shows("MASTER v") && r.si.shows("KSP WAIT"), "boot lines replayed when it answers");
  { const std::vector<uint8_t> *first = nullptr;
    for (auto &f : r.si.frames) if (!f.empty() && f[0] == SI_FRAME_BOOT_LINE) { first = &f; break; }
    CHECK(first && ((*first)[2] & SI_ATTR_REPLAY) && !((*first)[2] & SI_ATTR_REPLACE), "the replay's first line says so");
    int replace = 0, n = 0;                       // the replay: the first unbroken run of lines
    bool in = false;
    for (auto &f : r.si.frames) {
      bool isLine = !f.empty() && f[0] == SI_FRAME_BOOT_LINE;
      if (isLine) { in = true; n++; if (f[2] & SI_ATTR_REPLACE) replace++; }
      else if (in) break;
    }
    CHECK(n > 5 && replace == 0, "replayed lines are plain lines (%d of %d with REPLACE)", replace, n); }
}

static void testDemoBoot(Rig &r) {
  printf("demo releases the boot screen, no handshake\n");
  r.attachAll(); r.si.frames.clear();
  stubSimpit().kspUp = true;
  resetStubs(); stubSrsr() = 1;
  g_master.demo = false;
  setup();                                   // KSP up: connected at boot step 8
  CHECK(g_master.kspConnected, "connected at boot");
  runFor(20);
  CHECK(r.si.count(SI_FRAME_BOOT_DONE) == 1, "released on connect");
  int connected = 0; for (auto &l : r.si.screen()) if (l.second == "KSP CONNECTED") connected++;
  CHECK(connected == 1, "KSP CONNECTED shown once (%d)", connected);
  // Demo before KSP: no handshake attempts at all.
  r.si.frames.clear(); stubSimpit().kspUp = false;
  resetStubs(); stubSrsr() = 1;
  setup();
  g_master.demo = true;                      // the SCE switch arrives with stage 3
  int tries = stubSimpit().initCalls;
  runFor(5000);
  CHECK(stubSimpit().initCalls == tries, "no handshake attempts in demo");
  CHECK(r.si.count(SI_FRAME_BOOT_DONE) == 1 && r.si.shows("DEMO MODE"), "demo releases the boot screen");
  g_master.demo = false;
}

static void testBusRecovery(Rig &r) {
  printf("I2C bus held low at boot\n");
  r.attachAll(); r.si.frames.clear();
  resetStubs(); stubSrsr() = 1;
  stubPin(PIN_SDA_BUS) = LOW;                // a slave holding SDA; the stub "frees" it when clocked
  setup();
  CHECK(r.si.shows("I2C BUS WAS HELD LOW") || serialHas("bus held low"), "recovery reported");
}

static void testWatchdog(Rig &r) {
  printf("watchdog reset and reboot-loop guard\n");
  r.attachAll(); r.si.frames.clear();
  stubSimpit().kspUp = false;
  stubRetain()[0] = stubRetain()[1] = stubRetain()[2] = 0;
  stubRtc() = 5000;
  boot(1);
  runFor(100);
  CHECK(stubWdt().feeds > 0, "fed every pass");
  wdtCrumb(LS_CARRIERS, ADDR_INFODISP_2);    // where the "hang" happens
  if (stubWdt().callback) stubWdt().callback();
  CHECK(serialHas("watchdog: no feed"), "warning callback logs");

  // Reset #1
  r.si.frames.clear(); stubRtc() += 8;
  boot(1u << 4);
  CHECK(r.si.shows("RESET: WATCHDOG x1") && serialHas("RESET: WATCHDOG (1 IN A ROW)"), "watchdog reset reported");
  CHECK(r.si.shows("HUNG IN carrierService @13") && serialHas("HUNG IN carrierService @0x13"), "where it hung");
  CHECK(stubWdt().started && !boardResetInfo().guardTripped, "runs normally after one reset");
  // Reset #2 and #3 within 60 s
  wdtCrumb(LS_SIMPIT); r.si.frames.clear(); stubRtc() += 8;
  boot(1u << 4);
  CHECK(boardResetInfo().streak == 2 && !boardResetInfo().guardTripped, "second reset");
  CHECK(r.si.shows("HUNG IN unknown"), "no warning interrupt: stage unknown");
  wdtCrumb(LS_SIMPIT); r.si.frames.clear(); stubRtc() += 8;
  boot(1u << 4);
  int tries = stubSimpit().initCalls;
  CHECK(tries == 0, "no KSP handshake in the halted boot");
  CHECK(boardResetInfo().guardTripped, "third reset within 60 s trips the guard");
  CHECK(r.si.shows("HALTED") && r.si.sevOf("HALTED") == SEV_ERROR, "halt shown in red");
  CHECK(!stubWdt().started, "watchdog not started when halted");
  CHECK(bootHalted(), "halted");
  int c0 = r.si.count(SI_FRAME_CONTROL);
  runFor(3000);
  CHECK(stubSimpit().initCalls == tries, "no KSP handshake when halted");
  CHECK(r.si.count(SI_FRAME_CONTROL) > c0, "Sys Info heartbeat continues when halted");
  CHECK(r.si.count(SI_FRAME_BOOT_DONE) == 0, "stays on the boot screen");
  Serial.out.clear(); Serial.stubType("status\n"); runFor(10);
  CHECK(serialHas("HALTED"), "console still answers when halted");

  // A reset after the window starts a new streak.
  r.si.frames.clear(); stubRtc() += 120;
  boot(1u << 4);
  CHECK(boardResetInfo().streak == 1 && stubWdt().started, "a reset after 60 s starts a new streak");
  // A clean minute clears the streak; a power-on does too.
  runFor(61000, 50);
  CHECK(((stubRetain()[0] >> 8) & 0xFF) == 0, "a clean 60 s run clears the streak");
  boot(1u << 4);
  CHECK(boardResetInfo().streak == 1, "next watchdog reset counts from 1");
  boot(1);
  CHECK(boardResetInfo().streak == 0 && !boardResetInfo().watchdog, "power-on clears the streak");
}

void runStage1() {
  Rig r;
  testNormalBoot(r);
  testCarrierBytes(r);
  testWaitingForKsp(r);
  testKspConnects(r);
  testInbound(r);
  testDisplayLinks(r);
  testKspGate();
  testConsole();
  testDebugFlood();
  testAbsentParts(r);
  testSysInfoLate(r);
  testDemoBoot(r);
  testBusRecovery(r);
  testWatchdog(r);
}
