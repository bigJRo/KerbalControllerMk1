// Render scenarios for the Resource Display, for tools/render_screen.py.
//
// Appended to the concatenated sketch by the tool, so it sees every tab's globals.
// Loads a slot set the way the Select screen does, feeds the slots the values the
// Simpit handler would, lets the panel's own sampling run long enough for its trend
// arrows, time-to-empty and Detail history to settle, then runs the firmware's own
// loop() for the screen. Live mode throughout (demoMode false), so presence, alert
// states, hysteresis and the alert strip come from the real logic.
//
// Scenarios (argv[1]):
//   Main      Main screen, SPCT set on a ship in orbit: LF below its reserve bug,
//             MP in caution, food on its time tier, Solid Fuel not aboard (collapsed).
//   MainTTE   The same with the TTE key engaged: counters show time to empty.
//   EVA       Main screen on EVA: the ring-gauge layout for the fixed EVA set.
//   Select    Select screen for the same ship (absent resources dimmed).
//   Detail    Detail screen on Liquid Fuel after ten minutes of a slow burn.
//   Standby   Standby splash.

HardwareSerial Serial, SerialUSB1, Serial1, Serial2, Serial3;
TwoWire Wire, Wire1, Wire2;
volatile uint32_t ARM_DWT_CYCCNT, ARM_DWT_CTRL, ARM_DEMCR;

static void kcmRunFrames(int n) { for (int i = 0; i < n; i++) { loop(); stubMillis() += 50; } }

// One resource's state: vessel total and active-stage figures (stage < 0: none).
struct KcmRes { ResourceType t; float cur, max, stg, stgMax, ratePerS; };

static const KcmRes KCM_SHIP[] = {
  { RES_ELEC_CHARGE, 760.0f,  1000.0f, -1, -1, -0.35f },
  { RES_LIQUID_FUEL, 324.0f,  1440.0f, 158.0f, 720.0f, -0.0f },   // 22 %, under a 25 % bug
  { RES_LIQUID_OX,   396.0f,  1760.0f, 193.0f, 880.0f, -0.0f },
  { RES_MONO_PROP,    27.0f,   180.0f, -1, -1, -0.01f },          // ~13 %: caution
  { RES_LS_OXYGEN,  1650.0f,  2200.0f, -1, -1, -0.10f },
  { RES_LS_FOOD,     290.0f,   350.0f, -1, -1, -0.0012f },
  { RES_LS_WATER,    190.0f,   230.0f, -1, -1, -0.0008f },
  { RES_ABLATOR,     800.0f,   800.0f, 800.0f, 800.0f, 0.0f },
};
static const uint8_t KCM_SHIP_N = sizeof(KCM_SHIP) / sizeof(KCM_SHIP[0]);

static const KcmRes KCM_EVA[] = {
  { RES_ELEC_CHARGE, 41.0f,  50.0f, -1, -1, -0.02f },
  { RES_EVA_PROP,     3.1f,   5.0f, -1, -1, -0.002f },
  { RES_LS_OXYGEN,   48.0f,  60.0f, -1, -1, -0.01f },
  { RES_LS_FOOD,      2.8f,   3.0f, -1, -1, 0.0f },
  { RES_LS_WATER,     1.7f,   2.0f, -1, -1, 0.0f },
};

static const KcmRes *kcmTable = KCM_SHIP;
static uint8_t       kcmTableN = KCM_SHIP_N;
static float         kcmElapsed = 0.0f;   // seconds since the values were seeded

static const KcmRes *kcmFind(ResourceType t) {
  for (uint8_t i = 0; i < kcmTableN; i++) if (kcmTable[i].t == t) return &kcmTable[i];
  return nullptr;
}

// What the Simpit handler does for each resource message: values, presence, timestamp.
static void kcmFeed() {
  uint32_t now = millis();
  for (uint8_t t = 1; t < (uint8_t)RES_COUNT; t++) {
    const KcmRes *r = kcmFind((ResourceType)t);
    if (r) noteResourcePresence((ResourceType)t, r->cur, r->max);
    else   noteResourcePresence((ResourceType)t, 0.0f, 0.0f);
  }
  for (uint8_t i = 0; i < slotCount; i++) {
    const KcmRes *r = kcmFind(slots[i].type);
    ResourceSlot &s = slots[i];
    if (!r) { s.current = s.maxVal = s.stageCurrent = s.stageMax = 0.0f; s.updatedMs = now; continue; }
    float d = r->ratePerS * kcmElapsed;
    // A slow LF / LOx burn for the Detail history: 9:11 by volume, from the stage.
    if (r->t == RES_LIQUID_FUEL) d = -0.090f * kcmElapsed;
    if (r->t == RES_LIQUID_OX)   d = -0.110f * kcmElapsed;
    s.current = r->cur + d;  s.maxVal = r->max;
    if (r->stg >= 0) { s.stageCurrent = r->stg + d; s.stageMax = r->stgMax; }
    else             { s.stageCurrent = s.current;  s.stageMax = s.maxVal; }
    s.updatedMs = now;
  }
  refreshPending = false;
}

static void kcmLoadSet(const KcmRes *tab, uint8_t n) {
  for (uint8_t i = 0; i < MAX_SLOTS; i++) slots[i] = ResourceSlot();
  slotCount = 0;
  // The SPCT preset, as the Select key loads it (Solid Fuel absent on this ship).
  const PresetGroup &pg = PRESETS[0];
  for (uint8_t i = 0; i < pg.count; i++) { slots[slotCount].type = pg.types[i]; slotCount++; }
  (void)tab; (void)n;
  sortSlotsByGroup();
}

// Advance the panel clock with sampling only (no drawing), so trends, TTE and the
// history ring hold what that much flight would have left in them.
static void kcmWarm(float seconds, uint32_t stepMs) {
  for (float t = 0; t < seconds; t += stepMs / 1000.0f) {
    stubMillis() += stepMs; kcmElapsed += stepMs / 1000.0f;
    kcmFeed();
    updateAllSampling();
  }
}

static void kcmShip() {
  flightScene = true; evaActive = evaFlag = false;
  currentVesselName = "Kerbal X"; layoutRecalled = true;
  resetResourcePresence();
  kcmTable = KCM_SHIP; kcmTableN = KCM_SHIP_N;
  kcmLoadSet(KCM_SHIP, KCM_SHIP_N);
  for (uint8_t i = 0; i < slotCount; i++) if (slots[i].type == RES_LIQUID_FUEL) slots[i].bug = 0.25f;
  kcmFeed();
  resetAllSampling();
}

int main(int argc, char **argv) {
  const char *scenario = argc > 1 ? argv[1] : "Main";
  const char *outPath  = argc > 2 ? argv[2] : "screen.ppm";
  stubMillis() = 60000;
  demoMode = false; debugMode = false;
  setupSD();

  // The first frame draws the screen's chrome, which (on Main) restarts the sampling
  // for a new slot sequence; the warm-up runs after it so trends, time to empty and
  // the Detail history are what that much flight would have left in them.
  String sc = scenario;
  float warm = 360.0f;
  if (sc == "Main" || sc == "MainTTE" || sc == "Select" || sc == "Detail") {
    kcmShip();
    tteMode = (sc == "MainTTE");
    if (sc == "Detail") {
      warm = 600.0f;
      for (uint8_t i = 0; i < slotCount; i++) if (slots[i].type == RES_LIQUID_FUEL) setDetailSlot(i);
      switchToScreen(screen_Detail);
    } else {
      switchToScreen(sc == "Select" ? screen_Select : screen_Main);
    }
  } else if (sc == "EVA") {
    kcmShip();
    currentVesselName = "Jebediah Kerman"; layoutRecalled = false;   // the fixed EVA set
    kcmTable = KCM_EVA; kcmTableN = sizeof(KCM_EVA) / sizeof(KCM_EVA[0]);
    evaFlag = evaActive = true;
    loadEvaSlots();
    resetResourcePresence();
    kcmElapsed = 0; kcmFeed(); resetAllSampling();
    switchToScreen(screen_Main);
  } else if (sc == "Standby") {
    flightScene = false; warm = 0.0f;
    switchToScreen(screen_Standby);
  } else {
    fprintf(stderr, "unknown scenario '%s'\n", scenario);
    return 2;
  }
  kcmFeed(); loop(); stubMillis() += 50;
  if (warm > 0.0f) kcmWarm(warm, 250);
  // Frames as the panel runs them; the feed keeps the values live between passes.
  for (int i = 0; i < 4; i++) { kcmFeed(); loop(); stubMillis() += 50; kcmElapsed += 0.05f; }
  if (!infoDisp.writePPM(outPath)) { fprintf(stderr, "cannot write %s\n", outPath); return 1; }
  fprintf(stderr, "%s: screen %d slots %d\n", scenario, (int)activeScreen, (int)slotCount);
  return 0;
}
