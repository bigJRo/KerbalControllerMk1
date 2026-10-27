// Stage 5 selftest: sticks and throttle — the per-frame channels (rotation, translation,
// wheel, camera, throttle), stick mapping by control mode, precision, trim, the stick
// buttons, camera mode, EVA keys, the lever and its buttons, the autopilot interface,
// neutral channels on entering a flight, demo.
#include "harness.h"
#include "switches.h"
#include "interlocks.h"
#include "hold.h"
#include "keys.h"
#include "ksp_frame.h"

static const uint32_t SG1_ENGINE = 1u << 22, SG1_AP = 1u << 21, SG1_THRTL = 1u << 23, SG1_SCE = 1u << 18;
static const uint32_t SG2_BRAKE = 1u << 18, SG2_PREC = 1u << 22;
static const uint32_t TIME_SPC = 1u << 12, TIME_RVR = 1u << 13;
static const uint32_t JB_STICK = 0x01, JB_BTN01 = 0x02, JB_BTN02 = 0x04;

static uint32_t g_sg1, g_sg2, g_time;
static void preset(Bench &b) { b.m(MOD_FUNCTION).state = g_sg1; b.m(MOD_VEHICLE).state = g_sg2; b.m(MOD_TIME).state = g_time; }
static void boot5(Bench &b, uint32_t sg1, uint32_t sg2, uint32_t time) { g_sg1 = sg1; g_sg2 = sg2; g_time = time; powerOnBoot(b, 1, preset); }

static const SimpitSent *last(byte type) {
  auto &v = stubSimpit().sent;
  for (auto it = v.rbegin(); it != v.rend(); ++it) if (it->type == type) return &*it;
  return nullptr;
}
static int16_t i16(const SimpitSent *s, int off) { return s ? (int16_t)(s->b[off] | s->b[off + 1] << 8) : -32768; }
static bool near(int a, int b, int tol = 3) { return a - b <= tol && b - a <= tol; }
// rotationMessage: pitch, roll, yaw, mask; translation: X, Y, Z, mask; wheel: steer, throttle, mask;
// camera: pitch, roll, yaw, zoom, mask.
static int rotP() { return i16(last(ROTATION_MESSAGE), 0); }
static int rotR() { return i16(last(ROTATION_MESSAGE), 2); }
static int rotY() { return i16(last(ROTATION_MESSAGE), 4); }
static int thr()  { return i16(last(THROTTLE_MESSAGE), 0); }
static int keyCount(int16_t vk, uint8_t mod) {
  int n = 0;
  for (auto &s : stubSimpit().sent)
    if (s.type == KEYBOARD_EMULATOR && s.b.size() == 3 && s.b[0] == mod && (int16_t)(s.b[1] | s.b[2] << 8) == vk) n++;
  return n;
}
static void stick(ModuleSim &m, int16_t x, int16_t y, int16_t z) { m.axes[0] = x; m.axes[1] = y; m.axes[2] = z; m.signal(); runFor(40); }
static void eva(bool on) {
  flightStatusMessage fs = {}; fs.flightStatusFlags = (byte)(FLIGHT_IN_FLIGHT | (on ? FLIGHT_IS_EVA : 0));
  inbound(FLIGHT_STATUS_MESSAGE, &fs, sizeof fs); runFor(20);
}

static void testEntry(Bench &b) {
  printf("sticks: every channel neutral on entering a flight\n");
  boot5(b, SG1_ENGINE | SG1_AP, 0, TIME_SPC);
  enterFlight();
  const SimpitSent *r = last(ROTATION_MESSAGE), *t = last(TRANSLATION_MESSAGE), *w = last(WHEEL_MESSAGE);
  CHECK(r && t && w && i16(r, 0) == 0 && i16(r, 2) == 0 && i16(r, 4) == 0 && i16(t, 0) == 0 && i16(w, 0) == 0,
        "rotation, translation and wheel sent at 0");
  CHECK(last(THROTTLE_MESSAGE) && thr() == 0, "throttle sent (the lever, at 0)");
  CHECK(!last(CAMERA_ROTATION_MESSAGE), "camera not sent while unused");
  stubSimpit().sent.clear(); runFor(200);
  CHECK(stubSimpit().sent.empty(), "nothing more while nothing moves");
  (void)b;
}

static void testMapping(Bench &b) {
  printf("sticks: mapping by control mode, precision, trim\n");
  ModuleSim &r = b.m(MOD_ROTATION);
  stick(r, 16000, -8000, 4000);
  CHECK(near(rotY(), 16000) && near(rotP(), -8000) && near(rotR(), 4000), "SPC: X yaw, Y pitch, twist roll");
  b.m(MOD_TIME).release(TIME_SPC); runFor(40);
  stick(r, 16000, -8000, 4001);
  CHECK(near(rotR(), 16000) && near(rotP(), -8000) && near(rotY(), 4000), "PLN: X roll, Y pitch, twist yaw");
  b.m(MOD_VEHICLE).press(SG2_PREC); runFor(40);
  stick(r, 20000, 0, 0);
  CHECK(near(rotR(), 6000) && g_panel.inputPrec, "THC/RHC PREC: 0.3 scale");
  b.m(MOD_VEHICLE).release(SG2_PREC); runFor(40);
  stick(r, 10000, 0, 0);
  r.press(JB_BTN02); r.release(JB_BTN02); runFor(40);
  CHECK(g_panel.trim, "BTN02: trim set");
  stick(r, 0, 0, 0);
  CHECK(near(rotR(), 10000), "the stick back at centre: the trim holds roll");
  CHECK(r.ledOf(1) == KMC_LED_ACTIVE, "BTN02 lit while trimmed");
  uint16_t f = (uint16_t)(b.rig.ann.cmds.back()[3] | b.rig.ann.cmds.back()[4] << 8);
  CHECK(f & (1u << MF_TRIM), "Annunciator TRIM");
  r.press(JB_BTN01); r.release(JB_BTN01); runFor(40);
  CHECK(!g_panel.trim && rotR() == 0, "BTN01: trim cleared");
  stick(r, 10000, 0, 0); r.press(JB_BTN02); r.release(JB_BTN02); runFor(40); stick(r, 0, 0, 0);
  b.m(MOD_TIME).press(TIME_SPC); runFor(40);
  CHECK(!g_panel.trim && rotY() == 0, "a control-mode change clears it");
}

static void testRover(Bench &b) {
  printf("sticks: RVR and the stick button\n");
  ModuleSim &r = b.m(MOD_ROTATION), &f = b.m(MOD_FUNCTION);
  b.m(MOD_TIME).release(TIME_SPC); b.m(MOD_TIME).press(TIME_RVR); runFor(40);
  stick(r, 12000, 9000, 3000);
  const SimpitSent *w = last(WHEEL_MESSAGE);
  CHECK(near(i16(w, 0), 12000) && near(i16(w, 2), 9000), "RVR: X steering, Y wheel throttle");
  CHECK(rotP() == 0 && rotR() == 0 && rotY() == 0, "and no rotation");
  f.release(SG1_ENGINE); runFor(40);
  stick(r, 12001, 9001, 3000);
  w = last(WHEEL_MESSAGE);
  CHECK(near(i16(w, 0), 12000) && i16(w, 2) == 0, "ENGINE SAFE: steering still, wheel throttle 0");
  f.press(SG1_ENGINE); runFor(300);
  stick(r, 0, 0, 0);
  stubSimpit().sent.clear();
  r.press(JB_STICK); runFor(30);
  int on = 0; for (auto &s : stubSimpit().sent) if (s.type == AGACTIVATE_MESSAGE && s.b[0] == BRAKES_ACTION) on++;
  CHECK(on == 1, "RVR stick button: brakes while held");
  r.release(JB_STICK); runFor(30);
  int off = 0; for (auto &s : stubSimpit().sent) if (s.type == AGDEACTIVATE_MESSAGE && s.b[0] == BRAKES_ACTION) off++;
  CHECK(off == 1, "released, BRAKE on REL: off");
  b.m(MOD_TIME).release(TIME_RVR); b.m(MOD_TIME).press(TIME_SPC); runFor(40);
  const SimpitSent *w0 = last(WHEEL_MESSAGE);
  CHECK(w0 && i16(w0, 0) == 0 && i16(w0, 2) == 0, "leaving RVR: wheel back to 0");
}

static void testHoldAndDisconnect(Bench &b) {
  printf("sticks: ignored under HOLD; A/P disconnect\n");
  ModuleSim &r = b.m(MOD_ROTATION), &t = b.m(MOD_TRANSLATION), &s = b.m(MOD_STABILITY);
  s.press(1u << 10); s.release(1u << 10); runFor(40);
  CHECK(holdEngaged(), "HOLD engaged (SPC)");
  stick(r, 20000, 20000, 20000); stick(t, 15000, 0, 0);
  CHECK(rotP() == 0 && rotY() == 0 && rotR() == 0 && i16(last(TRANSLATION_MESSAGE), 0) == 0, "sticks ignored");
  t.press(JB_STICK); stick(t, 15000, 5000, 0);
  const SimpitSent *c = last(CAMERA_ROTATION_MESSAGE);
  CHECK(c && near(i16(c, 4), 15000) && near(i16(c, 0), 5000), "camera mode still works (X yaw, Y pitch)");
  t.release(JB_STICK); stick(t, 0, 0, 0);
  c = last(CAMERA_ROTATION_MESSAGE);
  CHECK(c && i16(c, 0) == 0 && i16(c, 4) == 0, "camera back to 0 when released");
  r.press(JB_STICK); r.release(JB_STICK); runFor(40);
  CHECK(!holdEngaged(), "SPC stick button: A/P disconnect releases HOLD");
  CHECK(near(rotY(), 20000), "the stick flies again");
  stick(r, 0, 0, 0);
  stubSimpit().sent.clear();
  t.press(JB_BTN01); t.release(JB_BTN01); runFor(20);
  t.press(JB_BTN02); t.release(JB_BTN02); runFor(20);
  int next = 0, reset = 0;
  for (auto &m : stubSimpit().sent) if (m.type == CAMERA_CONTROL_MODE) { if (m.b[0] == CAMERA_NEXT_MODE) next++; if (m.b[0] == FLIGHT_CAMERA_AUTO) reset++; }
  CHECK(next == 1 && reset == 1, "translation BTN01 next camera mode, BTN02 camera reset");
  stick(t, 9000, -9000, 3000);
  const SimpitSent *tr = last(TRANSLATION_MESSAGE);
  CHECK(near(i16(tr, 0), 9000) && near(i16(tr, 2), -9000) && near(i16(tr, 4), 3000), "translation X Y Z");
  stick(t, 0, 0, 0);
}

static void testEva(Bench &b) {
  printf("sticks: EVA keys\n");
  ModuleSim &r = b.m(MOD_ROTATION), &t = b.m(MOD_TRANSLATION);
  eva(true);
  stubSimpit().sent.clear();
  stick(r, 25000, 0, 0);
  CHECK(keyHeld(VK_D) && keyCount(VK_D, KEY_DOWN_MOD) == 1, "rotation X right: D held");
  CHECK(rotR() == 0 || !last(ROTATION_MESSAGE), "no rotation on EVA");
  stick(r, 12000, 0, 0);
  CHECK(keyHeld(VK_D), "hysteresis: still held at 37 %%");
  stick(r, 0, 0, 0);
  CHECK(!keyHeld(VK_D) && keyCount(VK_D, KEY_UP_MOD) == 1, "centre: D up");
  stick(t, 0, 0, 25000);
  CHECK(keyHeld(VK_LSHIFT), "translation Z up: Left Shift");
  stick(t, 0, 0, -25000);
  CHECK(!keyHeld(VK_LSHIFT) && keyHeld(VK_LCONTROL), "Z down: Left Ctrl");
  stick(t, 0, 0, 0);
  t.press(JB_STICK); stick(t, 25000, 5000, 25000);
  const SimpitSent *c = last(CAMERA_ROTATION_MESSAGE);
  CHECK(c && near(i16(c, 4), 25000) && near(i16(c, 0), 5000), "translation button held on EVA: camera mode");
  CHECK(!keyHeld(VK_D) && !keyHeld(VK_LSHIFT), "and the translation stick's EVA keys wait");
  t.release(JB_STICK); stick(t, 0, 0, 0);
  CHECK(!keyHeld(VK_D) && !keyHeld(VK_LSHIFT), "released at centre: no keys");
  r.press(JB_STICK); runFor(30);
  CHECK(keyHeld(VK_LSHIFT), "stick button on EVA: run (Left Shift)");
  stick(r, 0, -25000, 0);
  CHECK(keyHeld(VK_S), "Y back: S");
  eva(false);
  CHECK(!keyHeld(VK_S) && !keyHeld(VK_LSHIFT), "keys released when the EVA ends");
  r.release(JB_STICK); stick(r, 0, 0, 0);
}

static void testThrottle(Bench &b) {
  printf("throttle: lever, buttons, ENGINE\n");
  ModuleSim &th = b.m(MOD_THROTTLE), &f = b.m(MOD_FUNCTION);
  th.thrPos = 16000; th.signal(); runFor(40);
  CHECK(near(thr(), 16000), "the lever to KSP");
  th.edge(0x01); runFor(40);
  CHECK(th.thrPos == 32767 && near(thr(), 32767), "100 %%: lever to full");
  th.edge(0x04); runFor(40);
  CHECK(near(th.thrPos, 32767 - 3277) && near(thr(), 32767 - 3277), "DOWN: 10 %% down");
  th.edge(0x08); runFor(40);
  CHECK(th.thrPos == 0 && thr() == 0, "0 %%: lever to 0");
  th.edge(0x02); runFor(40);
  CHECK(near(th.thrPos, 3277), "UP: 10 %% up");
  f.press(SG1_THRTL); runFor(300);
  CHECK(th.thrPrec, "FINE on the module");
  uint16_t p0 = th.thrPos; th.edge(0x01); runFor(40);
  CHECK(th.thrPos == p0, "buttons ignored in FINE");
  f.release(SG1_THRTL); runFor(300);
  f.release(SG1_ENGINE); runFor(300);
  th.edge(0x01); runFor(40);
  CHECK(th.thrPos == 0 && thr() == 0, "ENGINE SAFE: buttons ignored, throttle 0");
  f.press(SG1_ENGINE); runFor(300);

  printf("throttle: an autopilot owns it\n");
  th.thrPos = 8000; th.signal(); runFor(40);
  f.press(SG1_THRTL); runFor(300);
  thrAutoThrottle(THR_OWNER_HOLD, 0.5f); runFor(40);
  CHECK(near(thr(), 16383) && thrOwner() == THR_OWNER_HOLD, "the owner's value to KSP");
  runFor(500);
  CHECK(!th.thrPrec, "FINE dropped while it owns the throttle");
  CHECK(near(th.thrPos, 16383, 40), "the lever driven to match (%u)", th.thrPos);
  th.touched = true; th.thrPos = 30000; th.signal(); runFor(300);
  CHECK(near(thr(), 16383), "the pilot's lever is ignored");
  th.touched = false; th.signal(); runFor(300);
  CHECK(near(th.thrPos, 16383, 40), "and put back once released");
  th.edge(0x08); runFor(40);
  CHECK(near(thr(), 16383), "so are the buttons");
  thrAutoThrottle(THR_OWNER_HOLD, 0.6f); runFor(300);
  CHECK(near(thr(), 19660), "follows the owner");
  thrAutoRelease(THR_OWNER_HOLD); runFor(300);
  CHECK(thrOwner() == THR_OWNER_NONE && near(thr(), th.thrPos, 70) && th.thrPrec, "released: the lever again, FINE back");
  f.press(SG1_ENGINE); f.release(SG1_THRTL); runFor(300);
  thrAutoThrottle(THR_OWNER_HOLD, 0.9f);
  f.release(SG1_ENGINE); runFor(40);
  CHECK(thr() == 0, "ENGINE SAFE beats the owner");
  thrAutoRelease(THR_OWNER_HOLD); f.press(SG1_ENGINE); runFor(300);

  printf("rotation: autopilot axes\n");
  rotSetAutoAxes(0.25f, 0, 0, ROT_AXIS_PITCH);
  stick(b.m(MOD_ROTATION), 10000, 0, 0);
  CHECK(near(rotP(), 8192) && near(rotY(), 10000), "held pitch from the autopilot, the rest from the pilot");
  rotClearAutoAxes(); stick(b.m(MOD_ROTATION), 0, 0, 0);
  CHECK(rotP() == 0, "cleared");
}

static void testDemo(Bench &b) {
  printf("sticks: demo\n");
  b.m(MOD_FUNCTION).press(SG1_SCE); runFor(200);
  stubSimpit().sent.clear();
  stick(b.m(MOD_ROTATION), 20000, 0, 0);
  b.m(MOD_THROTTLE).thrPos = 20000; b.m(MOD_THROTTLE).signal(); runFor(40);
  CHECK(stubSimpit().sent.empty(), "nothing to KSP");
  b.m(MOD_FUNCTION).release(SG1_SCE); stick(b.m(MOD_ROTATION), 0, 0, 0); runFor(200);
  Serial.out.clear(); Serial.stubType("frame\n"); runFor(10);
  CHECK(serialHas("rotation") && serialHas("throttle owner"), "console: frame");
}

void runStage5() {
  static Bench b;
  stubEepromReset();
  testEntry(b);
  testMapping(b);
  testRover(b);
  testHoldAndDisconnect(b);
  testEva(b);
  testThrottle(b);
  testDemo(b);
  for (int a = 0; a < 128; a++) Wire.detach(a);
}
