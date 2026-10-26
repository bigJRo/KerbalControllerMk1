// Stage 7 selftest: the ported autopilots in the master — the Info Display 2 console
// (command, ACK, status frames, summary), the hold autopilot flying through the control
// channels, the engage rules (A/P INHIB, ENGINE SAFE), the drop notices (INHIB, ENGINE
// SAFE, A/P disconnect, leaving the flight, telemetry loss), HOLD in PLN and RVR through
// the hold autopilot, the ascent autopilot's axes, the bench consoles.
#include "harness.h"
#include "switches.h"
#include "interlocks.h"
#include "hold.h"
#include "ksp_frame.h"
#include "autopilots.h"
#include "control_links.h"
#include "ascent_autopilot.h"
#include "hold_autopilot.h"
#include "burn_autopilot.h"
#include "landing_autopilot.h"

static const uint32_t SG1_AP = 1u << 21, SG1_ENGINE = 1u << 22;
static const uint32_t TIME_SPC = 1u << 12, TIME_RVR = 1u << 13;
static const uint8_t  SIT_LANDED = 1, SIT_FLYING = 8;
// Console opcodes (Hold_Mode_Autopilot.md §8.1, Ascent_Autopilot_Interface.md §4)
static const uint8_t OP_ARM = 0x10, OP_AP_OFF = 0x12, OP_ENG_ALT = 0x23, OP_ENG_HDG = 0x25, OP_ENG_IAS = 0x26;
static const uint8_t SCREEN_ACFTAP = 14;

static uint32_t g_sg1, g_time;
static void preset(Bench &b) { b.m(MOD_FUNCTION).state = g_sg1; b.m(MOD_TIME).state = g_time; }

// Telemetry for a plane in level flight at 1500 m, or a rover on the ground.
static bool g_rover;
static void telemetry() {
  flightStatusMessage fs = {};
  fs.flightStatusFlags = FLIGHT_IN_FLIGHT;
  fs.vesselSituation = g_rover ? SIT_LANDED : SIT_FLYING;
  fs.vesselType = g_rover ? 5 : 8;
  inbound(FLIGHT_STATUS_MESSAGE, &fs, sizeof fs);
  altitudeMessage alt = { 1500.0f, 1400.0f }; if (g_rover) alt = { 70.0f, 0.5f };
  inbound(ALTITUDE_MESSAGE, &alt, sizeof alt);
  velocityMessage vel = { 2300.0f, g_rover ? 8.0f : 120.0f, 0.0f };
  inbound(VELOCITY_MESSAGE, &vel, sizeof vel);
  airspeedMessage as = { 120.0f, 0.35f, 1.0f };
  inbound(AIRSPEED_MESSAGE, &as, sizeof as);
  vesselPointingMessage vp = { 90.0f, 2.0f, 0.0f, 90.0f, 0.0f, 90.0f, 0.0f };
  inbound(ROTATION_DATA_MESSAGE, &vp, sizeof vp);
  atmoConditionsMessage at = {}; at.atmoCharacteristics = HAS_ATMOSPHERE | IS_IN_ATMOSPHERE; at.airDensity = 1.0f;
  inbound(ATMO_CONDITIONS_MESSAGE, &at, sizeof at);
}
static void fly(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 50) { telemetry(); runFor(50); } }

static void console(Bench &b, uint8_t op, float v) { b.rig.id2.command(op, v); fly(150); }

static const SimpitSent *last(byte type) {
  auto &v = stubSimpit().sent;
  for (auto it = v.rbegin(); it != v.rend(); ++it) if (it->type == type) return &*it;
  return nullptr;
}
static int16_t i16(const SimpitSent *s, int off) { return s ? (int16_t)(s->b[off] | s->b[off + 1] << 8) : -32768; }

static void testConsole(Bench &b) {
  printf("autopilots: the Info Display 2 console\n");
  g_sg1 = SG1_AP | SG1_ENGINE; g_time = 0; g_rover = false;   // PLN
  powerOnBoot(b, 1, preset);
  enterFlight();
  b.rig.id2.screen = SCREEN_ACFTAP;
  fly(300);
  int acked = b.rig.id2.acked;
  console(b, OP_ENG_ALT, 1.0f);
  HoldStatus s = hpGetStatus();
  CHECK(s.pitchMode == HP_PITCH_ALT && s.alt == 1500.0f, "ENGAGE ALT: the hold autopilot holds 1500 m");
  CHECK(b.rig.id2.acked == acked + 1 && b.rig.id2.seq == 0, "the command was ACKed once");
  const auto *f = b.rig.id2.lastFrame(0xA6);
  CHECK(f && f->size() == 48 && (*f)[2] == HP_PITCH_ALT, "aircraft frame (48 bytes) pushed for the console on screen");
  CHECK(b.rig.id1.lastFrame(0xAA) && b.rig.id2.lastFrame(0xAA), "summary to both Info Displays");
  CHECK(g_panel.autopilot, "AUTOPILOT lit");
  console(b, OP_ENG_HDG, 1.0f);
  CHECK(hpGetStatus().latMode == HP_LAT_HDG, "ENGAGE HDG");

  printf("autopilots: authority — the sticks are ignored, nothing disconnects on stick input\n");
  ModuleSim &r = b.m(MOD_ROTATION);
  r.axes[0] = 20000; r.axes[1] = 20000; r.axes[2] = 20000; r.signal(); fly(500);
  CHECK(hpAnyEngaged(), "stick deflected: still engaged");
  const SimpitSent *rot = last(ROTATION_MESSAGE);
  CHECK(rot && i16(rot, 4) == 0, "yaw (not held) is 0, not the stick");
  r.axes[0] = r.axes[1] = r.axes[2] = 0; r.signal(); fly(100);

  printf("autopilots: ENGINE SAFE\n");
  ModuleSim &fn = b.m(MOD_FUNCTION);
  fn.release(SG1_ENGINE); fly(150);
  console(b, OP_ENG_IAS, 1.0f);
  CHECK(hpGetStatus().thrMode == HP_THR_OFF, "IAS refused while the ENGINE is SAFE");
  fn.press(SG1_ENGINE); fly(400);
  console(b, OP_ENG_IAS, 1.0f);
  CHECK(hpGetStatus().thrMode == HP_THR_IAS && thrOwner() == THR_OWNER_HOLD, "ARMED: IAS engages and owns the throttle");
  fn.release(SG1_ENGINE); fly(150);
  s = hpGetStatus();
  CHECK(s.thrMode == HP_THR_OFF && s.pitchMode == HP_PITCH_ALT && thrOwner() == THR_OWNER_NONE,
        "ENGINE SAFE drops IAS; ALT stays");
  fn.press(SG1_ENGINE); fly(400);

  printf("autopilots: A/P INHIB\n");
  fn.release(SG1_AP); fly(150);
  s = hpGetStatus();
  CHECK(!hpAnyEngaged() && s.reason == HP_REASON_AP_INHIB, "INHIB disconnects (reason A/P INHIB)");
  CHECK(!g_panel.autopilot, "AUTOPILOT off");
  console(b, OP_ENG_ALT, 1.0f);
  CHECK(!hpAnyEngaged(), "and nothing engages under INHIB");
  fn.press(SG1_AP); fly(150);

  printf("autopilots: A/P disconnect, A/P OFF, leaving the flight, telemetry loss\n");
  console(b, OP_ENG_ALT, 1.0f);
  g_time = 0; b.m(MOD_TIME).press(TIME_SPC); fly(100);        // the stick button is the A/P disconnect in SPC / PLN
  b.m(MOD_TIME).release(TIME_SPC); fly(100);
  r.press(0x01); r.release(0x01); fly(100);
  CHECK(!hpAnyEngaged() && hpGetStatus().reason == HP_REASON_AP_DISC, "stick button: A/P DISC");
  console(b, OP_ENG_ALT, 1.0f);
  console(b, OP_AP_OFF, 0.0f);
  CHECK(!hpAnyEngaged(), "the console's A/P OFF");
  console(b, OP_ENG_ALT, 1.0f);
  runFor(2500);                                               // no telemetry
  CHECK(!hpAnyEngaged() && hpGetStatus().reason == HP_REASON_TELEMETRY, "telemetry loss: TELEMETRY");
  console(b, OP_ENG_ALT, 1.0f);
  leaveFlight();
  CHECK(!hpAnyEngaged() && !g_panel.autopilot, "leaving the flight drops it");
}

static void testHold(Bench &b) {
  printf("HOLD: PLN through the hold autopilot\n");
  enterFlight();
  fly(300);
  ModuleSim &st = b.m(MOD_STABILITY), &fn = b.m(MOD_FUNCTION);
  st.press(1u << 10); st.release(1u << 10); fly(100);
  HoldStatus s = hpGetStatus();
  CHECK(holdEngaged() && s.pitchMode == HP_PITCH_ALT && s.latMode == HP_LAT_HDG, "PLN HOLD: ALT + HDG at the current values");
  CHECK(st.ledOf(10) == KMC_LED_ACTIVE && g_panel.autopilot, "HOLD lit, AUTOPILOT lit");
  fn.release(SG1_AP); fly(150);
  CHECK(holdEngaged() && hpAnyEngaged(), "A/P INHIB leaves HOLD alone");
  fn.press(SG1_AP); fly(150);
  console(b, OP_ENG_IAS, 1.0f);
  CHECK(!holdEngaged(), "a console autopilot engaging releases HOLD");
  console(b, OP_AP_OFF, 0.0f);
  st.press(1u << 10); st.release(1u << 10); fly(100);
  st.press(1u << 10); st.release(1u << 10); fly(100);
  CHECK(!holdEngaged() && !hpAnyEngaged(), "second press: HOLD and its modes off");

  printf("HOLD: RVR through the hold autopilot\n");
  g_rover = true;
  b.m(MOD_TIME).press(TIME_RVR); fly(300);
  st.press(1u << 10); st.release(1u << 10); fly(100);
  s = hpGetStatus();
  CHECK(holdEngaged() && s.rhdg && s.cruise, "RVR HOLD: heading + speed");
  CHECK(last(WHEEL_MESSAGE) != nullptr, "the wheels come from the hold autopilot");
  fn.release(SG1_ENGINE); fly(150);
  s = hpGetStatus();
  CHECK(holdEngaged() && s.rhdg && !s.cruise, "ENGINE SAFE: heading only, HOLD stays");
  st.press(1u << 10); st.release(1u << 10); fly(100);
  CHECK(!holdEngaged() && !hpRoverEngaged(), "released");
  fn.press(SG1_ENGINE); b.m(MOD_TIME).release(TIME_RVR); g_rover = false; fly(400);
}

static void testAscent(Bench &b) {
  printf("autopilots: ascent arms, flies its axes, drops on ENGINE SAFE\n");
  b.m(MOD_TIME).press(TIME_SPC); fly(200);
  stubSimpit().sent.clear();
  console(b, OP_ARM, 0.0f);
  CHECK(apIsArmed() && g_panel.autopilot && thrOwner() == THR_OWNER_ASCENT, "armed: AUTOPILOT, owns the throttle");
  fly(300);
  bool rot = false;
  for (auto &m : stubSimpit().sent) if (m.type == ROTATION_MESSAGE && (i16(&m, 0) != 0 || i16(&m, 4) != 0)) rot = true;
  CHECK(rot, "its steering goes out on the rotation channel");
  b.m(MOD_FUNCTION).release(SG1_ENGINE); fly(150);
  CHECK(!apIsArmed(), "ENGINE SAFE disarms it");
  b.m(MOD_FUNCTION).press(SG1_ENGINE); fly(400);
  console(b, OP_ARM, 0.0f);
  CHECK(apIsArmed(), "armed again");
  leaveFlight();
  CHECK(!apIsArmed() && !g_panel.autopilot, "leaving the flight disarms it (no flight, no autopilot)");
  enterFlight();
}

static void testConsoles() {
  printf("autopilots: bench consoles\n");
  Serial.out.clear(); Serial.stubType("hp STATUS\n"); runFor(10);
  CHECK(serialHas("HP pitch="), "hp STATUS");
  Serial.out.clear(); Serial.stubType("ap STATUS\n"); runFor(10);
  CHECK(serialHas("AP armed="), "ap STATUS");
  Serial.out.clear(); Serial.stubType("bp STATUS\n"); runFor(10);
  CHECK(serialHas("BP mode="), "bp STATUS");
  Serial.out.clear(); Serial.stubType("lp STATUS\n"); runFor(10);
  CHECK(serialHas("LP mode="), "lp STATUS");
  Serial.out.clear(); Serial.stubType("hp NONSENSE\n"); runFor(10);
  CHECK(serialHas("not understood"), "an unknown line is reported");
}

void runStage7() {
  static Bench b;
  stubEepromReset();
  testConsole(b);
  testHold(b);
  testAscent(b);
  testConsoles();
  for (int a = 0; a < 128; a++) Wire.detach(a);
}
