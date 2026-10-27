/***************************************************************************************
   panel_sticks.cpp — Rotation (0x28) and Translation (0x29) joysticks (design §8.7,
   Inventory §10.8). Packet (I2C spec §9.2): [3] button events, [4] change, [5] button
   state (bit 0 stick button, bit 1 BTN01, bit 2 BTN02), [6..11] AXIS1..3, int16 BE.
   LEDs: BTN01 is position 0, BTN02 position 1.

   Rotation stick, by control mode (AXIS1 = X, AXIS2 = Y, AXIS3 = twist):
     SPC  yaw / pitch / roll       PLN  roll / pitch / yaw
     RVR  steering / wheel throttle (AXIS1 / AXIS2)
     EVA  W A S D Q E held while deflected
   Translation stick: X / Y / Z translation; while its button is held, the camera
   (yaw / pitch / zoom), on EVA too; on EVA, otherwise, W A S D and Left Shift / Left Ctrl.
   THC / RHC PREC scales both sticks by STICK_PREC_SCALE (not the trim).
   While any autopilot or HOLD is engaged the sticks are ignored (ksp_frame.cpp), except
   camera mode and the buttons.

   Rotation buttons: BTN02 sets the master's trim from the current output (trim +
   stick), BTN01 clears it; the trim clears on a control-mode change; TRIM on the
   Annunciator while set. Stick button: SPC / PLN — A/P disconnect (every autopilot and
   HOLD); RVR — brakes while held (vehicleBrakeHold); EVA — Left Shift (run) while held.
   Translation buttons: BTN01 next camera mode, BTN02 camera reset (auto).

   Axis signs are as the module reports them (STICK_SIGN_* to change); the bench check
   confirms each direction (README).
****************************************************************************************/
#include "panels.h"
#include "config.h"
#include "state.h"
#include "debug.h"
#include "ksp_out.h"
#include "ksp_frame.h"
#include "keys.h"
#include "interlocks.h"
#include <KerbalModuleCommon.h>
#include <math.h>

static const float STICK_PREC_SCALE = 0.3f;
static const float STICK_SIGN[2][3] = { { 1, 1, 1 }, { 1, 1, 1 } };   // [rotation / translation][axis]
static const float EVA_KEY_ON = 0.5f, EVA_KEY_OFF = 0.3f;            // key down / up thresholds
static const uint32_t STICK_LOG_MS = 200;

enum : uint8_t { JB_STICK = 0x01, JB_BTN01 = 0x02, JB_BTN02 = 0x04 };
enum { STK_ROT = 0, STK_TRN = 1 };

static float   g_raw[2][3];
static float   g_trim[3];
static bool    g_trimSet;
static uint8_t g_btn[2];                 // button state from the last packet
static bool    g_stickHeld;              // rotation stick button, as acted on
static uint8_t g_stickMode;              // what the held stick button is doing: 0 none, 1 brake, 2 shift
static uint8_t g_lastMode;
static uint32_t g_lastLogMs;
static float   g_logged[2][3];

static float axisOf(const uint8_t *p) { return (float)(int16_t)(p[0] << 8 | p[1]) / 32767.0f; }

static void readAxes(int s, const ModEvent &e) {
  if (!e.pkt) return;
  for (int i = 0; i < 3; i++) g_raw[s][i] = STICK_SIGN[s][i] * axisOf(e.pkt + 6 + 2 * i);
  g_btn[s] = e.pkt[5];
}

static float scale() { return g_panel.inputPrec ? STICK_PREC_SCALE : 1.0f; }

static void clearTrim(const char *why) {
  if (!g_trimSet) return;
  g_trimSet = false;
  g_trim[0] = g_trim[1] = g_trim[2] = 0;
  g_panel.trim = false;
  dbg(DBG_STATE, "trim cleared (%s)", why);
}

static void setTrim() {
  for (int i = 0; i < 3; i++) {
    float v = g_trim[i] + g_raw[STK_ROT][i] * scale();
    g_trim[i] = v > 1 ? 1 : v < -1 ? -1 : v;
  }
  g_trimSet = g_trim[0] != 0 || g_trim[1] != 0 || g_trim[2] != 0;
  g_panel.trim = g_trimSet;
  dbg(DBG_STATE, "trim set %d %d %d %%", (int)(g_trim[0] * 100), (int)(g_trim[1] * 100), (int)(g_trim[2] * 100));
}

// The rotation stick button: what it does depends on the mode when it is pressed.
static void stickButton(bool held) {
  if (held == g_stickHeld) return;
  g_stickHeld = held;
  if (held) {
    if (g_vessel.eva)                         { g_stickMode = 2; }
    else if (g_panel.ctrlMode == CTRL_MODE_RVR) { g_stickMode = 1; vehicleBrakeHold(BRAKE_SRC_STICK, true); }
    else                                      { g_stickMode = 0; interlockDrop(DROP_AP_DISC); }
  } else {
    if (g_stickMode == 1) vehicleBrakeHold(BRAKE_SRC_STICK, false);
    g_stickMode = 0;
  }
}

static void onRotation(const ModEvent &e) {
  panelNoteEvent(e);
  if (!panelsEnabled()) return;
  readAxes(STK_ROT, e);
  uint32_t act = e.pressed | e.tapped;            // a quick press between two reads counts
  if (act & JB_BTN02) setTrim();
  if (act & JB_BTN01) clearTrim("BTN01");
  if (!panelLive()) return;                       // the stick button acts on KSP
  if (e.pressed & JB_STICK) stickButton(true);
  if ((e.released | e.tapped) & JB_STICK) { if (e.tapped & JB_STICK) stickButton(true); stickButton(false); }
}

static void onTranslation(const ModEvent &e) {
  panelNoteEvent(e);
  if (!panelsEnabled()) return;
  readAxes(STK_TRN, e);
  if (!panelLive()) return;
  if ((e.pressed | e.tapped) & JB_BTN01) {
    byte m = CAMERA_NEXT_MODE;
    kspSend(CAMERA_CONTROL_MODE, m);
    panelFlash(MOD_TRANSLATION, 0);
  }
  if ((e.pressed | e.tapped) & JB_BTN02) {
    byte m = FLIGHT_CAMERA_AUTO;
    kspSend(CAMERA_CONTROL_MODE, m);
    panelFlash(MOD_TRANSLATION, 1);
  }
}

/*--------------------------------------------------------------------------------------
  EVA keys: a key is held while either stick asks for it (with hysteresis)
--------------------------------------------------------------------------------------*/
struct EvaKey { int16_t vk; int8_t stick, axis, sign; };
static const EvaKey kEva[] = {
  { VK_D, STK_ROT, 0, +1 }, { VK_A, STK_ROT, 0, -1 }, { VK_W, STK_ROT, 1, +1 }, { VK_S, STK_ROT, 1, -1 },
  { VK_E, STK_ROT, 2, +1 }, { VK_Q, STK_ROT, 2, -1 },
  { VK_D, STK_TRN, 0, +1 }, { VK_A, STK_TRN, 0, -1 }, { VK_W, STK_TRN, 1, +1 }, { VK_S, STK_TRN, 1, -1 },
  { VK_LSHIFT, STK_TRN, 2, +1 }, { VK_LCONTROL, STK_TRN, 2, -1 },
};
static const int16_t kEvaKeys[] = { VK_W, VK_A, VK_S, VK_D, VK_Q, VK_E, VK_LSHIFT, VK_LCONTROL };

static void evaKeys(bool on, bool trnCamera) {   // trnCamera: the translation stick is on the camera
  for (int16_t vk : kEvaKeys) {
    bool want = false;
    if (on) {
      for (const EvaKey &k : kEva) {
        if (k.vk != vk || (trnCamera && k.stick == STK_TRN)) continue;
        float v = g_raw[k.stick][k.axis] * k.sign;
        if (v > EVA_KEY_ON || (keyHeld(vk) && v > EVA_KEY_OFF)) want = true;
      }
      if (vk == VK_LSHIFT && g_stickHeld && g_stickMode == 2) want = true;
    }
    if (want != keyHeld(vk)) keyHold(vk, want);
  }
}

static void logSticks(uint32_t now) {
  if (!(g_master.demo || g_master.debug) || now - g_lastLogMs < STICK_LOG_MS) return;
  if (memcmp(g_logged, g_raw, sizeof g_raw) == 0) return;
  memcpy(g_logged, g_raw, sizeof g_raw);
  g_lastLogMs = now;
  dbg(DBG_STICKS, "rot %+4d %+4d %+4d  trn %+4d %+4d %+4d %%",
      (int)(g_raw[0][0] * 100), (int)(g_raw[0][1] * 100), (int)(g_raw[0][2] * 100),
      (int)(g_raw[1][0] * 100), (int)(g_raw[1][1] * 100), (int)(g_raw[1][2] * 100));
}

void panelSticksService() {
  uint32_t now = millis();
  if (g_panel.ctrlMode != g_lastMode) {
    clearTrim("control mode changed");
    if (g_stickHeld) stickButton(false);          // the button's mode-specific action ends
    g_lastMode = g_panel.ctrlMode;
  }
  if (!panelLive() && g_stickHeld) stickButton(false);
  if (moduleLifecycle(MOD_ROTATION) != KMC_STATUS_ACTIVE) memset(g_raw[STK_ROT], 0, sizeof g_raw[STK_ROT]);
  if (moduleLifecycle(MOD_TRANSLATION) != KMC_STATUS_ACTIVE) memset(g_raw[STK_TRN], 0, sizeof g_raw[STK_TRN]);

  float k = scale(), s[3];
  for (int i = 0; i < 3; i++) s[i] = g_raw[STK_ROT][i] * k + g_trim[i];
  if (g_panel.ctrlMode == CTRL_MODE_SPC) frameSetPilotRotation(s[1], s[0], s[2]);
  else                                  frameSetPilotRotation(s[1], s[2], s[0]);   // PLN (and RVR: unused)
  frameSetPilotWheel(s[0], s[1]);
  frameSetPilotTranslation(g_raw[STK_TRN][0] * k, g_raw[STK_TRN][1] * k, g_raw[STK_TRN][2] * k);
  bool cam = panelLive() && (g_btn[STK_TRN] & JB_STICK);
  frameSetCamera(cam, g_raw[STK_TRN][1], g_raw[STK_TRN][0], g_raw[STK_TRN][2]);

  evaKeys(panelLive() && g_vessel.eva, cam);
  logSticks(now);

  // LEDs (BTN01 = 0, BTN02 = 1): rotation BTN02 lit while the trim is set.
  uint32_t dr = panelDown(MOD_ROTATION), dt = panelDown(MOD_TRANSLATION);
  bool demo = panelDemo();
  moduleSetLed(MOD_ROTATION, 0, demo && (dr & JB_BTN01) ? KMC_LED_ACTIVE : KMC_LED_ENABLED);
  moduleSetLed(MOD_ROTATION, 1, (demo ? (dr & JB_BTN02) : g_trimSet) ? KMC_LED_ACTIVE : KMC_LED_ENABLED);
  moduleSetLed(MOD_TRANSLATION, 0, (demo ? (dt & JB_BTN01) : panelFlashing(MOD_TRANSLATION, 0)) ? KMC_LED_ACTIVE : KMC_LED_ENABLED);
  moduleSetLed(MOD_TRANSLATION, 1, (demo ? (dt & JB_BTN02) : panelFlashing(MOD_TRANSLATION, 1)) ? KMC_LED_ACTIVE : KMC_LED_ENABLED);
}

void panelSticksInit() {
  memset(g_raw, 0, sizeof g_raw);
  memset(g_trim, 0, sizeof g_trim);
  memset(g_logged, 0, sizeof g_logged);
  g_trimSet = false;
  g_panel.trim = false;
  g_btn[0] = g_btn[1] = 0;
  g_stickHeld = false;
  g_stickMode = 0;
  g_lastMode = g_panel.ctrlMode;
  g_lastLogMs = 0;
  moduleBusSetHandler(MOD_ROTATION, onRotation);
  moduleBusSetHandler(MOD_TRANSLATION, onTranslation);
}
