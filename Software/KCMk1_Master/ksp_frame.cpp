/***************************************************************************************
   ksp_frame.cpp — per-frame control channels and the autopilot interface.
****************************************************************************************/
#include "ksp_frame.h"
#include "ksp_out.h"
#include "config.h"
#include "state.h"
#include "debug.h"
#include "interlocks.h"
#include "lever.h"

static inline int16_t axis(float v) {
  if (v > 1.0f) v = 1.0f;
  if (v < -1.0f) v = -1.0f;
  return (int16_t)(v * 32767.0f);
}

// Pilot inputs
static float g_pRot[3], g_pTrn[3], g_pWheel[2], g_cam[3], g_pThr;
static bool  g_camOn;

// Autopilot inputs
static float   g_aRot[3], g_aWheel[2], g_aTrn[3], g_aThr;
static uint8_t g_aRotMask, g_aWheelMask, g_aTrnMask, g_thrOwner;

// What was sent, per channel
struct Chan { int16_t v[4]; uint8_t mask; bool sent, dirty; uint32_t lastMs; };
enum { CH_ROT, CH_TRN, CH_WHEEL, CH_CAM, CH_THR, CH_COUNT };
static Chan g_ch[CH_COUNT];
static const char *const kChName[CH_COUNT] = { "rotation", "translation", "wheel", "camera", "throttle" };

// Edges that force a resend
static bool    g_wasReady, g_wasAuto, g_wasEva;
static uint8_t g_wasMode;
static uint32_t g_lastLeverMs;
static uint16_t g_lastLeverSent;          // the last lever target sent
static int8_t   g_holdSent;               // CMD_SET_LEVER_HOLD as last sent (-1 = not yet)
static uint32_t g_holdMs;

void frameSetPilotRotation(float p, float y, float r) { g_pRot[0] = p; g_pRot[1] = y; g_pRot[2] = r; }
void frameSetPilotTranslation(float x, float y, float z) { g_pTrn[0] = x; g_pTrn[1] = y; g_pTrn[2] = z; }
void frameSetPilotWheel(float s, float t) { g_pWheel[0] = s; g_pWheel[1] = t; }
void frameSetPilotThrottle(float t) { g_pThr = t < 0 ? 0 : t > 1 ? 1 : t; }
void frameSetCamera(bool on, float p, float y, float z) {
  if (on != g_camOn) { g_ch[CH_TRN].dirty = g_ch[CH_CAM].dirty = true; }
  g_camOn = on; g_cam[0] = p; g_cam[1] = y; g_cam[2] = z;
}

void frameForce() { for (auto &c : g_ch) c.dirty = true; }
void frameForceThrottle() { g_ch[CH_THR].dirty = true; }
uint16_t frameThrottleSent() { return g_ch[CH_THR].sent ? (uint16_t)g_ch[CH_THR].v[0] : 0; }

/*--------------------------------------------------------------------------------------
  Autopilot interface
--------------------------------------------------------------------------------------*/
void thrAutoThrottle(uint8_t owner, float t) {
  if (owner == THR_OWNER_NONE) return;
  if (owner != g_thrOwner) { dbg(DBG_OWNER, "throttle owner %u -> %u", g_thrOwner, owner); g_ch[CH_THR].dirty = true; }
  g_thrOwner = owner;
  g_aThr = t < 0 ? 0 : t > 1 ? 1 : t;
}
void thrAutoRelease(uint8_t owner) {
  if (owner != g_thrOwner || owner == THR_OWNER_NONE) return;
  dbg(DBG_OWNER, "throttle owner %u -> pilot", owner);
  g_thrOwner = THR_OWNER_NONE;
  g_ch[CH_THR].dirty = true;
}
uint8_t thrOwner()     { return g_thrOwner; }
bool    thrAutoOwned() { return g_thrOwner != THR_OWNER_NONE; }
float   thrCurrentThrottle() { return (float)frameThrottleSent() / 32767.0f; }
bool    thrTouched()   { LeverState l = leverState(); return l.known && l.touched; }
bool    thrPrecision() { LeverState l = leverState(); return l.known && l.precision; }
bool    thrLeverDriven() { LeverState l = leverState(); return l.known && l.active && !l.precision; }

void rotSetAutoAxes(float p, float y, float r, uint8_t mask) {
  g_aRot[0] = p; g_aRot[1] = y; g_aRot[2] = r;
  if (mask != g_aRotMask) g_ch[CH_ROT].dirty = true;
  g_aRotMask = mask & 0x07;
}
void rotClearAutoAxes() { if (g_aRotMask) g_ch[CH_ROT].dirty = true; g_aRotMask = 0; }
float rotPilotPitch() { return g_pRot[0]; }
float rotPilotYaw()   { return g_pRot[1]; }
float rotPilotRoll()  { return g_pRot[2]; }

void wheelSetAuto(float s, float t, uint8_t mask) {
  g_aWheel[0] = s; g_aWheel[1] = t;
  if (mask != g_aWheelMask) g_ch[CH_WHEEL].dirty = true;
  g_aWheelMask = mask & 0x03;
}
void wheelClearAuto() { if (g_aWheelMask) g_ch[CH_WHEEL].dirty = true; g_aWheelMask = 0; }

void trnSetAuto(float x, float y, float z, uint8_t mask) {
  g_aTrn[0] = x; g_aTrn[1] = y; g_aTrn[2] = z;
  if (mask != g_aTrnMask) g_ch[CH_TRN].dirty = true;
  g_aTrnMask = mask & 0x07;
}
void trnClearAuto() { if (g_aTrnMask) g_ch[CH_TRN].dirty = true; g_aTrnMask = 0; }

/*--------------------------------------------------------------------------------------
  Sending
--------------------------------------------------------------------------------------*/
// True if the channel should go out now with these values.
static bool due(Chan &c, const int16_t *v, uint8_t n, uint8_t mask, int16_t noise, uint32_t now) {
  if (now - c.lastMs < FRAME_MIN_MS && c.sent) return false;
  bool changed = !c.sent || c.dirty || mask != c.mask;
  for (uint8_t i = 0; i < n && !changed; i++) {
    int32_t d = (int32_t)v[i] - c.v[i];
    if (d >= noise || d <= -noise || (v[i] == 0 && c.v[i] != 0)) changed = true;
  }
  return changed;
}

static void mark(Chan &c, const int16_t *v, uint8_t n, uint8_t mask, uint32_t now) {
  for (uint8_t i = 0; i < n; i++) c.v[i] = v[i];
  c.mask = mask; c.sent = true; c.dirty = false; c.lastMs = now;
}

static void sendRotation(uint32_t now, bool engaged, bool pilotHere) {
  int16_t v[3];
  for (uint8_t i = 0; i < 3; i++) {
    float f = (g_aRotMask & (1u << i)) ? g_aRot[i] : (engaged || !pilotHere) ? 0.0f : g_pRot[i];
    v[i] = axis(f);
  }
  if (!due(g_ch[CH_ROT], v, 3, 0x07, FRAME_AXIS_NOISE, now)) return;
  rotationMessage m;
  m.setPitch(v[0]); m.setYaw(v[1]); m.setRoll(v[2]);
  if (kspSend(ROTATION_MESSAGE, m)) mark(g_ch[CH_ROT], v, 3, 0x07, now);
}

static void sendTranslation(uint32_t now, bool engaged, bool eva) {
  int16_t v[3];
  bool pilot = !engaged && !eva && !g_camOn;
  for (uint8_t i = 0; i < 3; i++)
    v[i] = axis((g_aTrnMask & (1u << i)) ? g_aTrn[i] : pilot ? g_pTrn[i] : 0.0f);
  if (!due(g_ch[CH_TRN], v, 3, 0x07, FRAME_AXIS_NOISE, now)) return;
  translationMessage m;
  m.setX(v[0]); m.setY(v[1]); m.setZ(v[2]);
  if (kspSend(TRANSLATION_MESSAGE, m)) mark(g_ch[CH_TRN], v, 3, 0x07, now);
}

static void sendWheel(uint32_t now, bool engaged, bool pilotHere) {
  int16_t v[2];
  for (uint8_t i = 0; i < 2; i++) {
    float f = (g_aWheelMask & (1u << i)) ? g_aWheel[i] : (engaged || !pilotHere) ? 0.0f : g_pWheel[i];
    v[i] = axis(f);
  }
  if (!interlockThrottleLive()) v[1] = 0;            // ENGINE not ARMED: no wheel throttle
  if (!due(g_ch[CH_WHEEL], v, 2, 0x03, FRAME_AXIS_NOISE, now)) return;
  wheelMessage m;
  m.setSteer(v[0]); m.setThrottle(v[1]);
  if (kspSend(WHEEL_MESSAGE, m)) mark(g_ch[CH_WHEEL], v, 2, 0x03, now);
}

static void sendCamera(uint32_t now) {
  Chan &c = g_ch[CH_CAM];
  bool lastZero = !c.sent || (c.v[0] == 0 && c.v[1] == 0 && c.v[2] == 0);
  if (!g_camOn && lastZero) { c.dirty = false; return; }   // off, and KSP already has 0
  int16_t v[3];
  for (uint8_t i = 0; i < 3; i++) v[i] = axis(g_camOn ? g_cam[i] : 0.0f);
  if (!due(c, v, 3, 0x0D, FRAME_AXIS_NOISE, now)) return;
  cameraRotationMessage m;
  m.setPitch(v[0]); m.setYaw(v[1]); m.setZoom(v[2]);
  if (kspSend(CAMERA_ROTATION_MESSAGE, m)) mark(c, v, 3, 0x0D, now);
}

static void throttleFrame(uint32_t now) {
  LeverState l = leverState();
  float t;
  if (!interlockThrottleLive())  t = 0.0f;
  else if (g_thrOwner)           t = g_aThr;
  else                           t = (l.known && l.active) ? (float)l.pos / 32767.0f : g_pThr;
  int16_t v = axis(t);
  if (v < 0) v = 0;

  // Under an autopilot the lever is driven to its value and held there (design §11.3,
  // Inventory §8.10): CMD_SET_LEVER_HOLD tells the module to keep the target against a
  // hand (stall-limited) and to drive back on release. While a hand is on it, the lever
  // reads the hand, so a new target goes out only when the autopilot's value moves.
  bool hold = g_thrOwner && interlockThrottleLive() && l.known && l.active && !l.precision;
  if (l.known && l.active && ((int8_t)hold != g_holdSent || now - g_holdMs >= FRAME_LEVER_HOLD_MS)) {
    if (leverHold(hold)) { g_holdSent = (int8_t)hold; g_holdMs = now; }
  }
  if (hold && now - g_lastLeverMs >= FRAME_LEVER_MIN_MS) {
    int32_t d = (int32_t)v - (int32_t)(l.touched ? g_lastLeverSent : l.pos);
    if (d > (int32_t)FRAME_LEVER_BAND || d < -(int32_t)FRAME_LEVER_BAND) {
      if (leverCommand((uint16_t)v)) { g_lastLeverMs = now; g_lastLeverSent = (uint16_t)v; }
    }
  }
  if (!due(g_ch[CH_THR], &v, 1, 0x01, FRAME_THR_NOISE, now)) return;
  if (kspThrottle(v)) mark(g_ch[CH_THR], &v, 1, 0x01, now);
}

void frameService() {
  uint32_t now = millis();
  bool ready = !g_master.demo && g_master.kspConnected && g_vessel.flightScene;
  bool engaged = g_panel.autopilot;
  bool eva = g_vessel.eva;
  uint8_t mode = g_panel.ctrlMode;
  if (ready && (!g_wasReady || engaged != g_wasAuto || eva != g_wasEva || mode != g_wasMode)) {
    if (!g_wasReady) dbg(DBG_TOKSP, "flight: every channel sent again");
    frameForce();
  }
  if (!ready) for (auto &c : g_ch) c.sent = false;   // next flight starts from a clean slate
  g_wasReady = ready; g_wasAuto = engaged; g_wasEva = eva; g_wasMode = mode;
  if (!ready) return;
  bool stickFlies = !eva && mode != CTRL_MODE_RVR;
  bool stickDrives = !eva && mode == CTRL_MODE_RVR;
  sendRotation(now, engaged, stickFlies);
  sendTranslation(now, engaged, eva);
  sendWheel(now, engaged, stickDrives);
  sendCamera(now);
  throttleFrame(now);
}

static void cmdFrame(const char *) {
  for (uint8_t i = 0; i < CH_COUNT; i++) {
    const Chan &c = g_ch[i];
    if (!c.sent) { conPrintf("  %-11s not sent this flight", kChName[i]); continue; }
    conPrintf("  %-11s %6d %6d %6d  %lu ms ago", kChName[i], c.v[0], c.v[1], c.v[2], (unsigned long)(millis() - c.lastMs));
  }
  conPrintf("  throttle owner %u, autopilot axes rot %02X wheel %02X translation %02X", g_thrOwner, g_aRotMask,
            g_aWheelMask, g_aTrnMask);
}

void frameInit() {
  memset(g_ch, 0, sizeof g_ch);
  memset(g_pRot, 0, sizeof g_pRot); memset(g_pTrn, 0, sizeof g_pTrn); memset(g_pWheel, 0, sizeof g_pWheel);
  memset(g_cam, 0, sizeof g_cam); memset(g_aRot, 0, sizeof g_aRot); memset(g_aWheel, 0, sizeof g_aWheel);
  memset(g_aTrn, 0, sizeof g_aTrn); g_aTrnMask = 0;
  g_pThr = g_aThr = 0; g_camOn = false;
  g_aRotMask = g_aWheelMask = g_thrOwner = 0;
  g_wasReady = g_wasAuto = g_wasEva = false; g_wasMode = 0xFF;
  g_lastLeverMs = 0;
  g_lastLeverSent = 0;
  g_holdSent = -1;
  g_holdMs = 0;
  consoleRegister("frame", "control channels: last values sent to KSP", cmdFrame);
}
