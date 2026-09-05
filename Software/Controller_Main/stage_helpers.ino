/***************************************************************************************
   stage_helpers.ino — auto-stage and the acceleration estimate.

   Auto-stage: fire STAGE_ACTION when the current throttle owner is above 10 % throttle,
   the stage's delta-V is spent, and the owner still has delta-V to deliver; 2 s lockout.

   Acceleration estimate (Mission_Autopilot.md §2): Simpit sends neither thrust nor mass.
     EST  — stage delta-V / stage burn time, the stage average, available before any burn.
     MEAS — felt g during a vacuum burn is the thrust acceleration; after 2 s of steady
            throttle above 50 % the measurement (scaled to full throttle) replaces EST
            for the rest of the stage. A staging event (stage delta-V jumping up) resets it.
     TWR  — a pilot-entered thrust-to-weight override bypasses both.
****************************************************************************************/
#include "control_links.h"

static bool     as_enabled     = true;
static uint32_t as_lastStageMs = 0;
static const uint32_t AS_LOCKOUT_MS   = 2000;
static const float    AS_STAGE_DV_MIN = 5.0f;

static float    ae_stageDv = 0.0f, ae_burnTime = 0.0f, ae_prevStageDv = 0.0f;
static float    ae_gForce = 0.0f;
static bool     ae_inAtmo = true;
static float    ae_throttle = 0.0f;
static uint32_t ae_steadySince = 0;
static float    ae_meas = 0.0f;              // m/s² at full throttle, 0 = none
static float    ae_twr = 0.0f, ae_g0 = 9.81f;
static const float AE_MEAS_MIN_THROTTLE = 0.5f;
static const uint32_t AE_MEAS_SETTLE_MS = 2000;
static const float AE_MEAS_FILTER = 0.2f;

void asSetEnabled(bool on) { as_enabled = on; }
bool asEnabled()           { return as_enabled; }

void asMaybeStage(bool enabled, float throttle, float remainingDv) {
  if (!enabled || throttle < 0.10f) return;
  if (ae_stageDv > AS_STAGE_DV_MIN || remainingDv < AS_STAGE_DV_MIN) return;
  uint32_t now = millis();
  if (now - as_lastStageMs < AS_LOCKOUT_MS) return;
  mySimpit.activateAction(STAGE_ACTION);
  as_lastStageMs = now;
}

void aeIngestStage(float stageDv, float stageBurnTime) {
  if (stageDv > ae_prevStageDv + 50.0f) ae_meas = 0.0f;   // new stage: the old measurement is void
  ae_prevStageDv = ae_stageDv;
  ae_stageDv = stageDv; ae_burnTime = stageBurnTime;
}
void aeIngestGForce(float gForce)     { ae_gForce = gForce; }
void aeIngestAtmo(bool inAtmosphere)  { ae_inAtmo = inAtmosphere; }

void aeNoteThrottle(float commanded) {
  ae_throttle = commanded;
  uint32_t now = millis();
  if (commanded < AE_MEAS_MIN_THROTTLE || ae_inAtmo) { ae_steadySince = 0; return; }
  if (ae_steadySince == 0) { ae_steadySince = now; return; }
  if (now - ae_steadySince < AE_MEAS_SETTLE_MS) return;
  float a = (ae_gForce * 9.81f) / commanded;       // felt g under thrust in vacuum, scaled to full throttle
  if (a < 0.5f) return;
  ae_meas = (ae_meas <= 0.0f) ? a : ae_meas + AE_MEAS_FILTER * (a - ae_meas);
}

void aeSetTwrOverride(float twr, float g0) { ae_twr = twr; if (g0 > 0.0f) ae_g0 = g0; }

uint8_t aeSource() {
  if (ae_twr > 0.0f) return AE_SRC_TWR;
  if (ae_meas > 0.0f) return AE_SRC_MEAS;
  return AE_SRC_EST;
}

float aeAccel() {
  switch (aeSource()) {
    case AE_SRC_TWR:  return ae_twr * ae_g0;
    case AE_SRC_MEAS: return ae_meas;
    default:          return (ae_burnTime > 0.5f) ? ae_stageDv / ae_burnTime : 0.0f;
  }
}

float aeBurnDuration(float dv) { float a = aeAccel(); return (a > 0.0f) ? fabsf(dv) / a : 0.0f; }
float aeStageDv()              { return ae_stageDv; }
