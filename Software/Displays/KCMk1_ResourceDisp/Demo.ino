/***************************************************************************************
   Demo.ino -- Demo mode for Kerbal Controller Mk1 Resource Display
   Drives resource levels with slow sinusoidal sweeps so all display features are
   exercised without a live KSP connection. Each slot gets an independent phase offset
   so bars move independently. Only active when demoMode is true.

   Presence scenario: every DEMO_ABSENT_PERIOD_MS the demo moves through three
   phases of "not aboard" -- nothing absent, then Solid Fuel and Ablator absent (a
   craft past staging with no heat shield), then those plus MonoPropellant and Xenon
   -- so the Main screen's collapse, the Select screen's dimming and the ORDER list
   can all be checked without a vessel to switch to. An absent resource's values
   are held at zero so Detail reads as it would live.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


static const uint32_t DEMO_UPDATE_MS       = 50;      // update interval in ms
static const uint32_t DEMO_ABSENT_PERIOD_MS = 20000;  // time in each presence phase
static uint32_t _demoLast = 0;
static float    _demoPhase = 0.0f;           // global phase counter (radians)


/***************************************************************************************
   PRESENCE SCENARIO
****************************************************************************************/
static uint8_t demoAbsentPhase() {
  return (uint8_t)((millis() / DEMO_ABSENT_PERIOD_MS) % 3);
}

bool demoResourceAbsent(ResourceType t) {
  uint8_t phase = demoAbsentPhase();
  if (phase == 0) return false;
  if (t == RES_SOLID_FUEL || t == RES_ABLATOR) return true;
  if (phase == 2 && (t == RES_MONO_PROP || t == RES_XENON)) return true;
  return false;
}


/***************************************************************************************
   INIT DEMO MODE
   Called once from setup(). Populates slots with defaults and sets initial levels.
****************************************************************************************/
void initDemoMode() {
  initAllSlots();
  // DEMO_EVA: latch the EVA flag as FLIGHT_STATUS would. loop() reconciles it on
  // its first pass, loads the EVA slot set and shows the ring-gauge layout.
  if (DEMO_EVA) evaFlag = true;
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
    if (demoResourceAbsent(slots[i].type)) {
      slots[i].current = slots[i].maxVal = slots[i].stageCurrent = slots[i].stageMax = 0.0f;
      continue;
    }
    float offset = i * 0.785f;  // PI/4 per slot

    // Total: slow sweep across the FULL range, so every meter visits 0% and 100%
    // and the counters, limit bands and alarm cell can be checked at both ends.
    float totalPhase = _demoPhase + offset;
    slots[i].current = 0.5f + 0.5f * sinf(totalPhase);   // 0.00-1.00

    // Stage: faster, independent sweep of the stage FRACTION across its full
    // range, so the secondary column visibly moves against the primary rather
    // than pinning at 100%. Stage max is a fixed fraction of total max (~40% of
    // the vessel) so the units counters differ between the two modes.
    float stagePhase = _demoPhase * 2.3f + offset + 1.0f;
    slots[i].stageMax     = 0.4f;
    slots[i].stageCurrent = slots[i].stageMax * (0.5f + 0.5f * sinf(stagePhase));
    slots[i].maxVal       = 1.0f;
  }
}

