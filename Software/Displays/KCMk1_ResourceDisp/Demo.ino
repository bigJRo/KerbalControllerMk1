/***************************************************************************************
   Demo.ino -- Demo mode for Kerbal Controller Mk1 Resource Display
   Drives resource levels with slow sinusoidal sweeps so all display features are
   exercised without a live KSP connection. Each slot gets an independent phase offset
   so bars move independently. Only active when demoMode is true.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


static const uint32_t DEMO_UPDATE_MS = 50;   // update interval in ms
static uint32_t _demoLast = 0;
static float    _demoPhase = 0.0f;           // global phase counter (radians)


/***************************************************************************************
   INIT DEMO MODE
   Called once from setup(). Populates slots with defaults and sets initial levels.
****************************************************************************************/
void initDemoMode() {
  initAllSlots();
}


/***************************************************************************************
   STEP DEMO STATE
   Advances each slot's resource levels along slow sine waves with per-slot phase
   offsets. Total and stage values move independently so switching modes shows a clear
   difference. Stage is a faster, smaller sweep (simulating a partially-fuelled stage).
   Called every loop() pass.
****************************************************************************************/
void stepDemoState() {
  if (!demoMode) return;  // no-op when live Simpit data is active

  uint32_t now = millis();
  if (now - _demoLast < DEMO_UPDATE_MS) return;
  _demoLast = now;

  _demoPhase += 0.01f;  // ~0.6 rad/s at 50ms interval — full cycle ~10 seconds

  for (uint8_t i = 0; i < slotCount; i++) {
    if (slots[i].type == RES_NONE) continue;
    float offset = i * 0.785f;  // PI/4 per slot

    // Total: slow sweep across full range
    float totalPhase = _demoPhase + offset;
    slots[i].current = 0.5f + 0.48f * sinf(totalPhase);  // 0.02–0.98

    // Stage: faster sweep, smaller amplitude, lower average
    // Represents the current stage having less capacity than the whole vessel
    float stagePhase = _demoPhase * 2.3f + offset + 1.0f;
    slots[i].stageCurrent = 0.35f + 0.30f * sinf(stagePhase);  // 0.05–0.65

    // Stage max is a fixed fraction of total max (demo: ~40% of vessel)
    slots[i].stageMax = 0.4f;
    slots[i].maxVal   = 1.0f;
  }
}
