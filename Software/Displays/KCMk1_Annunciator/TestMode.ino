/***************************************************************************************
   TestMode.ino -- Serial-driven test framework for KCMk1 Annunciator
   Activated when standaloneTest = true in AAA_Config.ino.
   Requires demoMode = false and provides its own state -- does not use Simpit.

   TWO TEST MODES (selected from Serial monitor):
   L -- Logic test runner
        Systematically sets AppState to known trigger and clear conditions for every
        C&W indicator, calls updateCautionWarningState(), and checks the resulting
        bitmask against expected values. Reports PASS/FAIL over Serial.
        Two-tier indicators (PE_LOW, PROP_LOW, LIFE_SUPPORT) are tested at both
        yellow (companion bool) and red (C&W bit) tiers separately.

   D -- Display walk-through
        Steps through every C&W indicator, situation button, regime tile, mode-grid
        tile and SPCFT state one at a time. Directly forces state to illuminate each
        indicator in isolation so rendering is verified independently of logic.
        Advance with ENTER or N. Go back with B. Quit with Q.

   SERIAL COMMANDS (available at any time):
   L   -- run logic tests
   D   -- enter display walk-through
   R   -- reprint menu
   Q   -- quit current test, return to menu

   LOGIC TEST DESIGN
   Each test case:
     1. Calls resetTestState() to put everything in a known neutral baseline
     2. Calls a setup function to set the specific condition
     3. Calls updateCautionWarningState()
     4. Checks that the expected bit IS set (or companion bool IS true for yellow tier)
     5. Checks that no unexpected WARNING bits are set (leakage check)
     6. Calls resetTestState() again for the clear test
     7. Calls updateCautionWarningState()
     8. Checks that the expected bit IS NOT set

   DISPLAY WALK-THROUGH DESIGN
   Each display step directly writes to state.cautionWarningState and companion bools
   without going through logic. This isolates rendering from logic so display bugs
   are visible even when logic is correct. The main screen is drawn and updated
   normally on each step so the full rendering pipeline is exercised.
****************************************************************************************/
#include "KCMk1_Annunciator.h"
#include <cfloat>

// Forward declarations for companion bools defined in CautionWarning.ino
extern bool peLowYellow;
extern bool propLowYellow;
extern bool lsYellow;

// Forward declarations for test functions defined below
static void printMenu();
static void runLogicTests();
static void runDisplayWalkthrough();
static void resetTestState();
static void setTestBody();
static bool checkBit(uint8_t bit, bool expected, const char *name, const char *tier);
static bool checkYellow(bool *companion, const char *name);
static void printResult(bool pass, const char *name, const char *detail);

// Test mode state
static bool _inDisplayWalk  = false;
static uint8_t _displayStep = 0;
static uint8_t _logicPassed = 0;
static uint8_t _logicFailed = 0;


/***************************************************************************************
   INIT TEST MODE
   Called from setup() when standaloneTest = true.
   Draws the main screen as a backdrop and prints the menu to Serial.
****************************************************************************************/
void initTestMode() {
  Serial.println(F("\n========================================"));
  Serial.println(F("  KCMk1 ANNUNCIATOR TEST MODE"));
  Serial.println(F("========================================"));

  // Set up a neutral display baseline
  resetTestState();
  currentBody = getBodyParams("Kerbin");
  state.gameSOI = "Kerbin";
  switchToScreen(screen_Main);

  // Draw the main screen chrome immediately so display is visible during tests
  drawStaticMain(infoDisp);
  updateCautionWarningState();
  updateScreenMain(infoDisp);

  printMenu();
}


/***************************************************************************************
   RUN TEST MODE
   Called from loop() when standaloneTest = true.
   Handles Serial input and dispatches to sub-modes.
****************************************************************************************/
void runTestMode() {
  // Keep display updated during normal test mode idle
  // During walk-through, runDisplayWalkthrough() draws directly so we skip here
  if (!_inDisplayWalk) {
    if (activeScreen != prevScreen) {
      if (activeScreen == screen_Main) drawStaticMain(infoDisp);
      prevScreen = activeScreen;
    }
    updateScreenMain(infoDisp);
  }

  if (!Serial.available()) return;

  char cmd = toupper(Serial.read());
  // Flush remainder of line
  while (Serial.available()) Serial.read();

  if (_inDisplayWalk) {
    switch (cmd) {
      case 'N': case '\r': case '\n':
        _displayStep++;
        runDisplayWalkthrough();
        break;
      case 'B':
        if (_displayStep > 0) _displayStep--;
        runDisplayWalkthrough();
        break;
      case 'Q':
        _inDisplayWalk = false;
        Serial.println(F("\nDisplay walk-through ended."));
        resetTestState();
        updateCautionWarningState();
        printMenu();
        break;
      default:
        Serial.println(F("  ENTER/N=next  B=back  Q=quit"));
        break;
    }
    return;
  }

  switch (cmd) {
    case 'L':
      runLogicTests();
      break;
    case 'D':
      _inDisplayWalk = true;
      _displayStep   = 0;
      runDisplayWalkthrough();
      break;
    case 'R':
      printMenu();
      break;
    default:
      Serial.println(F("Unknown command. Send R to reprint menu."));
      break;
  }
}


/***************************************************************************************
   PRINT MENU
****************************************************************************************/
static void printMenu() {
  Serial.println(F("\n--- TEST MENU ---"));
  Serial.println(F("  L  Run logic tests (automated pass/fail)"));
  Serial.println(F("  D  Display walk-through (visual check)"));
  Serial.println(F("  R  Reprint this menu"));
  Serial.println(F("-----------------"));
  Serial.println(F("Send command:"));
}


/***************************************************************************************
   RESET TEST STATE
   Sets AppState and all telemetry flags to a known neutral baseline.
   After this call, updateCautionWarningState() should produce cw == 0
   (all indicators off) with default Kerbin body params.
****************************************************************************************/
static void resetTestState() {
  // Reset AppState to defaults
  state = AppState();

  // Override fields whose defaults would trigger C&W conditions:
  // (temperatures already default to 0; kept explicit so a future default change
  // cannot make HIGH_TEMP leak into every test)
  state.skinTemp  = 0;
  state.maxTemp   = 0;
  // periapsis defaults to 0 which is below reentryAlt(45000) -> triggers PE_LOW
  // Set well above Kerbin atmosphere (70000m) so Pe is safe
  state.periapsis = 150000.0f;
  state.apoapsis  = 200000.0f;

  // Reset telemetry flags
  inFlight        = false;
  inEVA           = false;
  hasO2           = false;
  inAtmo          = false;
  physTW          = false;
  simpitConnected = false;

  // Reset situation to ORBIT with inFlight=true as neutral baseline
  state.vesselSituationState = 0;
  bitSet(state.vesselSituationState, VSIT_ORBIT);
  inFlight = true;

  // Reset companion bools
  peLowYellow   = false;
  propLowYellow = false;
  lsYellow      = false;

  // Reset chute state
  chuteEnvState     = chute_Off;
  prevChuteEnvState = chute_Off;

  // Reset the mode grid and panel flags to a dark baseline so walk-through steps
  // don't bleed into each other.
  state.modeFlags = 0;
  demoMode        = false;
  debugMode       = false;
  audioEnabled    = false;
  simpitConnected = true;

  // Set body to Kerbin
  setTestBody();

  // Warm up the LOW_DV throttle holdoff static by running two update cycles
  // with throttle non-zero. Leave throttleCmd at 50 so there is no second
  // 0->nonzero transition when individual tests set throttleCmd = 50.
  // stageDV and stageBurnTime are set well above thresholds so LOW_DV does
  // not fire spuriously during other tests that run with throttleCmd=50.
  state.stageDV       = CW_LOW_DV_MS   + 500.0f;
  state.stageBurnTime = CW_LOW_BURN_S  + 120.0f;
  state.throttleCmd   = 50;
  updateCautionWarningState();
  updateCautionWarningState();
  // throttleCmd intentionally left at 50 -- see note above
}


/***************************************************************************************
   SET TEST BODY
   Populates currentBody with Kerbin values for consistent logic test results.
   Called by resetTestState() and can be called independently to change body context.
****************************************************************************************/
static void setTestBody() {
  currentBody = getBodyParams("Kerbin");
  state.gameSOI = "Kerbin";
}


/***************************************************************************************
   LOGIC TEST HELPERS
****************************************************************************************/
static bool checkBit(uint8_t bit, bool expected, const char *name, const char *tier) {
  bool actual = bitRead(state.cautionWarningState, bit);
  bool pass   = (actual == expected);
  char detail[48];
  snprintf(detail, sizeof(detail), "bit%d %s: expected %s got %s",
           bit, tier,
           expected ? "SET" : "CLR",
           actual   ? "SET" : "CLR");
  printResult(pass, name, detail);
  return pass;
}

static bool checkYellow(bool *companion, const char *name) {
  bool pass = (*companion == true) &&
              !bitRead(state.cautionWarningState,
                       // find which companion this is
                       (companion == &peLowYellow)   ? CW_PE_LOW :
                       (companion == &propLowYellow)  ? CW_PROP_LOW :
                                                        CW_LIFE_SUPPORT);
  char detail[48];
  snprintf(detail, sizeof(detail), "yellow: companion=%s bit=%s",
           *companion ? "true" : "false",
           bitRead(state.cautionWarningState,
                   (companion == &peLowYellow)  ? CW_PE_LOW :
                   (companion == &propLowYellow) ? CW_PROP_LOW :
                                                   CW_LIFE_SUPPORT) ? "SET(BAD)" : "CLR(good)");
  printResult(pass, name, detail);
  return pass;
}

static bool checkNoLeakage(uint8_t expectedBit, const char *name) {
  // Check no unexpected WARNING bits are set
  uint32_t unexpected = (state.cautionWarningState & masterAlarmMask) & ~(1ul << expectedBit);
  bool pass = (unexpected == 0);
  if (!pass) {
    char detail[48];
    snprintf(detail, sizeof(detail), "leakage: unexpected bits 0x%08lX", (unsigned long)unexpected);
    printResult(false, name, detail);
  }
  return pass;
}

static void printResult(bool pass, const char *name, const char *detail) {
  Serial.print(pass ? F("  PASS  ") : F("  FAIL  "));
  Serial.print(name);
  Serial.print(F(" -- "));
  Serial.println(detail);
  if (pass) _logicPassed++; else _logicFailed++;
}

// Convenience macro -- set up, check trigger, check clear
#define TEST_BIT(name, bit, setupOn, setupOff) \
  do { \
    resetTestState(); setupOn; \
    updateCautionWarningState(); \
    checkBit(bit, true,  name, "on"); \
    checkNoLeakage(bit, name); \
    resetTestState(); setupOff; \
    updateCautionWarningState(); \
    checkBit(bit, false, name, "off"); \
  } while(0)


/***************************************************************************************
   LOGIC TEST RUNNER
   Tests every C&W indicator in order.
   Two-tier indicators get separate yellow and red tests.
****************************************************************************************/
static void runLogicTests() {
  _logicPassed = 0;
  _logicFailed = 0;

  Serial.println(F("\n=== LOGIC TESTS ==="));
  Serial.println(F("Format: PASS/FAIL  INDICATOR -- detail"));
  Serial.println();

  // ---------------------------------------------------------------------------------
  // ROW 0 -- WARNING tier
  // ---------------------------------------------------------------------------------

  // CW_LOW_DV
  TEST_BIT("LOW_DV",
    CW_LOW_DV,
    { inFlight = true;
      bitClear(state.vesselSituationState, VSIT_PRELAUNCH);
      state.throttleCmd   = 50;    // nonzero -- not in coast, not in holdoff
      state.stageDV       = CW_LOW_DV_MS - 10.0f;
      state.stageBurnTime = CW_LOW_BURN_S - 10.0f; },
    { state.throttleCmd   = 50;
      state.stageDV       = CW_LOW_DV_MS + 100.0f;
      state.stageBurnTime = CW_LOW_BURN_S + 10.0f; });

  // CW_HIGH_G (positive)
  TEST_BIT("HIGH_G pos",
    CW_HIGH_G,
    { state.gForces = CW_HIGH_G_ALARM + 1.0f; },
    { state.gForces = 1.0f; });

  // CW_HIGH_G (negative)
  TEST_BIT("HIGH_G neg",
    CW_HIGH_G,
    { state.gForces = CW_HIGH_G_WARN - 1.0f; },
    { state.gForces = 1.0f; });

  // CW_HIGH_TEMP
  TEST_BIT("HIGH_TEMP",
    CW_HIGH_TEMP,
    { state.maxTemp = tempAlarm + 1; },
    { state.maxTemp = 0; state.skinTemp = 0; });

  // CW_BUS_VOLTAGE
  TEST_BIT("BUS_VOLTAGE",
    CW_BUS_VOLTAGE,
    { state.EC_total = 1000.0f;
      state.EC       = state.EC_total * (CW_EC_LOW_FRAC - 0.01f); },
    { state.EC_total = 1000.0f;
      state.EC       = state.EC_total * 0.5f; });

  // CW_ABORT
  TEST_BIT("ABORT",
    CW_ABORT,
    { state.abort_on = true; },
    { state.abort_on = false; });

  // ---------------------------------------------------------------------------------
  // ROW 1 -- Mixed
  // ---------------------------------------------------------------------------------

  // CW_GROUND_PROX
  TEST_BIT("GROUND_PROX",
    CW_GROUND_PROX,
    { state.vesselSituationState = 0;
      bitSet(state.vesselSituationState, VSIT_FLIGHT);
      state.vel_vert  = -50.0f;
      state.alt_surf  = CW_GROUND_PROX_S * 40.0f; },  // just under 10s to impact
    { state.alt_surf  = 5000.0f; });

  // CW_PE_LOW -- airless body red tier
  {
    resetTestState();
    currentBody = getBodyParams("Mun");  // airless
    state.periapsis = currentBody.minSafe - 100.0f;
    updateCautionWarningState();
    checkBit(CW_PE_LOW, true, "PE_LOW airless red", "on");
    checkNoLeakage(CW_PE_LOW, "PE_LOW airless red");

    resetTestState();
    currentBody = getBodyParams("Mun");
    state.periapsis = currentBody.minSafe + 1000.0f;
    updateCautionWarningState();
    checkBit(CW_PE_LOW, false, "PE_LOW airless red", "off");
    setTestBody();
  }

  // CW_PE_LOW -- atmospheric yellow tier
  {
    resetTestState();
    state.periapsis = currentBody.reentryAlt + 1000.0f;  // above reentry, inside atmo
    updateCautionWarningState();
    checkYellow(&peLowYellow, "PE_LOW atmo yellow");

    resetTestState();
    state.periapsis = currentBody.lowSpace + 1000.0f;  // above atmosphere
    updateCautionWarningState();
    bool pass = !peLowYellow && !bitRead(state.cautionWarningState, CW_PE_LOW);
    printResult(pass, "PE_LOW atmo yellow", "off: periapsis above atmo");
  }

  // CW_PE_LOW -- atmospheric red tier
  TEST_BIT("PE_LOW atmo red",
    CW_PE_LOW,
    { state.periapsis = currentBody.reentryAlt - 1000.0f; },
    { state.periapsis = currentBody.lowSpace + 1000.0f; });

  // CW_PROP_LOW -- yellow tier
  {
    resetTestState();
    state.LF_stage_tot = 1000.0f;
    state.OX_stage_tot = 1000.0f;
    state.LF_stage     = state.LF_stage_tot * (CW_PROP_LOW_WARN_FRAC - 0.02f);
    state.OX_stage     = state.OX_stage_tot * 0.5f;
    updateCautionWarningState();
    checkYellow(&propLowYellow, "PROP_LOW yellow");
  }

  // CW_PROP_LOW -- red tier
  TEST_BIT("PROP_LOW red",
    CW_PROP_LOW,
    { state.LF_stage_tot = 1000.0f;
      state.OX_stage_tot = 1000.0f;
      state.LF_stage     = state.LF_stage_tot * (CW_PROP_LOW_ALARM_FRAC - 0.01f);
      state.OX_stage     = state.OX_stage_tot * 0.5f; },
    { state.LF_stage_tot = 1000.0f;
      state.OX_stage_tot = 1000.0f;
      state.LF_stage     = state.LF_stage_tot * 0.8f;
      state.OX_stage     = state.OX_stage_tot * 0.8f; });

  // CW_LIFE_SUPPORT -- yellow tier (food between warn and alarm thresholds)
  // All other resources must be set to safe (abundant) levels so they don't
  // independently trigger the red alarm. tacOxygen=0 after reset looks like
  // zero oxygen remaining which fires the red tier immediately.
  {
    resetTestState();
    state.crewCount = 2;
    double crew = 2.0;

    // Set all consumables to 10x warn threshold -- safely in the green zone
    state.tacOxygen = (float)(TACLS_OXYGEN_RATE * crew * TACLS_OXYGEN_WARN_S * 10.0);
    state.tacWater  = (float)(TACLS_WATER_RATE  * crew * TACLS_WATER_WARN_S  * 10.0);
    // Set waste capacity to very large so waste fraction is near zero
    state.tacCO2_tot   = 1e6f; state.tacCO2   = 0.0f;
    state.tacWaste_tot = 1e6f; state.tacWaste  = 0.0f;
    state.tacWW_tot    = 1e6f; state.tacWW     = 0.0f;

    // Set food to midpoint between warn and alarm thresholds
    double foodRate = TACLS_FOOD_RATE * crew;
    float  midpoint = (TACLS_FOOD_WARN_S + TACLS_FOOD_ALARM_S) / 2.0f;
    state.tacFood   = (float)(foodRate * midpoint);

    updateCautionWarningState();
    checkYellow(&lsYellow, "LIFE_SUPPORT yellow");
  }

  // CW_LIFE_SUPPORT -- red tier (oxygen critical)
  TEST_BIT("LIFE_SUPPORT red",
    CW_LIFE_SUPPORT,
    { state.crewCount = 2;
      double crew = 2.0;
      // All resources safe except oxygen which is critically low
      state.tacFood   = (float)(TACLS_FOOD_RATE  * crew * TACLS_FOOD_WARN_S  * 10.0);
      state.tacWater  = (float)(TACLS_WATER_RATE  * crew * TACLS_WATER_WARN_S * 10.0);
      state.tacCO2_tot   = 1e6f; state.tacCO2   = 0.0f;
      state.tacWaste_tot = 1e6f; state.tacWaste  = 0.0f;
      state.tacWW_tot    = 1e6f; state.tacWW     = 0.0f;
      // Oxygen below alarm threshold
      double o2Rate   = TACLS_OXYGEN_RATE * crew;
      state.tacOxygen = (float)(o2Rate * (TACLS_OXYGEN_ALARM_S - 60.0f)); },
    { state.crewCount = 0; });

  // CW_O2_PRESENT
  TEST_BIT("O2_PRESENT",
    CW_O2_PRESENT,
    { inAtmo = true; hasO2 = true; },
    { inAtmo = false; hasO2 = false; });

  // ---------------------------------------------------------------------------------
  // ROW 2 -- CAUTION tier
  // ---------------------------------------------------------------------------------

  // CW_IMPACT_IMM
  TEST_BIT("IMPACT_IMM",
    CW_IMPACT_IMM,
    { state.vesselSituationState = 0;
      bitSet(state.vesselSituationState, VSIT_FLIGHT);
      state.vel_vert = -50.0f;
      state.alt_surf = CW_IMPACT_IMM_S * 40.0f; },  // ~30s to impact
    { state.alt_surf = 50000.0f; });

  // CW_ALT
  TEST_BIT("ALT",
    CW_ALT,
    { state.vesselSituationState = 0;
      bitSet(state.vesselSituationState, VSIT_FLIGHT);
      state.alt_surf = CW_ALT_THRESHOLD_M - 10.0f; },
    { state.alt_surf = CW_ALT_THRESHOLD_M + 100.0f; });

  // CW_DESCENT
  TEST_BIT("DESCENT",
    CW_DESCENT,
    { state.vesselSituationState = 0;
      bitSet(state.vesselSituationState, VSIT_FLIGHT);
      state.vel_vert = -10.0f; },
    { state.vel_vert = 10.0f; });

  // CW_GEAR_UP
  TEST_BIT("GEAR_UP",
    CW_GEAR_UP,
    { state.vesselSituationState = 0;
      bitSet(state.vesselSituationState, VSIT_FLIGHT);
      state.vel_vert  = -10.0f;
      state.alt_surf  = CW_GEAR_UP_THRESHOLD_M - 10.0f;
      state.gear_on   = false; },
    { state.gear_on   = true; });

  // CW_ATMO
  TEST_BIT("ATMO",
    CW_ATMO,
    { inAtmo = true; },
    { inAtmo = false; });

  // ---------------------------------------------------------------------------------
  // ROW 3 -- CAUTION tier
  // ---------------------------------------------------------------------------------

  // CW_RCS_LOW
  TEST_BIT("RCS_LOW",
    CW_RCS_LOW,
    { state.mono_tot = 100.0f;
      state.mono     = state.mono_tot * (CW_RCS_LOW_FRAC - 0.02f); },
    { state.mono_tot = 100.0f;
      state.mono     = state.mono_tot * 0.5f; });

  // CW_PROP_IMBAL
  TEST_BIT("PROP_IMBAL",
    CW_PROP_IMBAL,
    { state.LF_stage_tot = 1000.0f;
      state.OX_stage_tot = 1000.0f;
      state.LF_stage     = 200.0f;   // ratio = 200/800 = 0.25, far from nominal 0.818
      state.OX_stage     = 800.0f; },
    { state.LF_stage_tot = 1000.0f;
      state.OX_stage_tot = 1000.0f;
      state.LF_stage     = 450.0f;   // ratio ~0.818 nominal
      state.OX_stage     = 550.0f; });

  // CW_COMM_LOST
  TEST_BIT("COMM_LOST",
    CW_COMM_LOST,
    { inFlight = true;
      bitClear(state.vesselSituationState, VSIT_PRELAUNCH);
      state.commNet = 0; },
    { state.commNet = 80; });

  // CW_Ap_LOW -- atmospheric body
  TEST_BIT("Ap_LOW atmo",
    CW_Ap_LOW,
    { state.vesselSituationState = 0;
      bitSet(state.vesselSituationState, VSIT_SUBORBIT);
      state.apoapsis = currentBody.lowSpace - 5000.0f; },
    { state.apoapsis = currentBody.lowSpace + 10000.0f; });

  // CW_Ap_LOW -- airless body
  {
    resetTestState();
    currentBody = getBodyParams("Mun");
    state.vesselSituationState = 0;
    bitSet(state.vesselSituationState, VSIT_SUBORBIT);
    state.apoapsis = currentBody.minSafe - 500.0f;
    updateCautionWarningState();
    checkBit(CW_Ap_LOW, true, "Ap_LOW airless", "on");

    resetTestState();
    currentBody = getBodyParams("Mun");
    state.vesselSituationState = 0;
    bitSet(state.vesselSituationState, VSIT_ORBIT);
    state.apoapsis = currentBody.minSafe + 5000.0f;
    updateCautionWarningState();
    checkBit(CW_Ap_LOW, false, "Ap_LOW airless", "off");
    setTestBody();
  }

  // CW_HIGH_Q -- suppressed by default (highQThreshold == 0)
  {
    resetTestState();
    inAtmo = true;
    state.atmoPressure = 100.0f;
    state.vel_surf     = 500.0f;
    updateCautionWarningState();
    bool pass = !bitRead(state.cautionWarningState, CW_HIGH_Q);
    printResult(pass, "HIGH_Q suppressed", "off: highQThreshold==0 for Kerbin");

    // Now set a threshold and trigger it
    currentBody.highQThreshold = 1000.0f;  // low threshold
    updateCautionWarningState();
    pass = bitRead(state.cautionWarningState, CW_HIGH_Q);
    printResult(pass, "HIGH_Q triggered", "on: threshold set, pressure+vel high");
    setTestBody();
  }

  // ---------------------------------------------------------------------------------
  // ROW 4 -- POSITIVE and STATE indicators
  // ---------------------------------------------------------------------------------

  // CW_ORBIT_STABLE
  TEST_BIT("ORBIT_STABLE",
    CW_ORBIT_STABLE,
    { state.vesselSituationState = 0;
      bitSet(state.vesselSituationState, VSIT_ORBIT);
      state.periapsis = currentBody.lowSpace + 5000.0f;
      state.apoapsis  = currentBody.lowSpace + 50000.0f; },
    { state.periapsis = currentBody.reentryAlt - 1000.0f; }); // Pe inside atmo

  // CW_ELEC_GEN -- latched by a rising reading, released by a falling one. The
  // latch must survive recomputes with the reading unchanged (every other Simpit
  // message triggers one), which is what the middle call checks.
  {
    resetTestState();
    state.EC = 500.0f;
    updateCautionWarningState();  // initialises _prevEC
    state.EC = 505.0f;            // EC increased
    updateCautionWarningState();
    checkBit(CW_ELEC_GEN, true, "ELEC_GEN", "on: EC increasing");
    updateCautionWarningState();  // unchanged reading: must stay lit
    checkBit(CW_ELEC_GEN, true, "ELEC_GEN", "on: held across an unrelated recompute");

    state.EC = 504.0f;            // EC decreased
    updateCautionWarningState();
    checkBit(CW_ELEC_GEN, false, "ELEC_GEN", "off: EC decreasing");
  }

  // CW_CHUTE_ENV -- red (too fast for drogue)
  {
    resetTestState();
    inAtmo           = true;
    state.airDensity = 1.225f;                  // Kerbin sea-level density (q calibration)
    state.vel_surf   = 600.0f;                  // q > drogue limit
    updateCautionWarningState();
    bool pass = bitRead(state.cautionWarningState, CW_CHUTE_ENV) &&
                chuteEnvState == chute_Red;
    printResult(pass, "CHUTE_ENV red", pass ? "on" : "off");
    if (pass) _logicPassed++; else _logicFailed++;

    // yellow (safe for drogue, not main)
    state.vel_surf = 350.0f;                     // main limit < q < drogue limit
    updateCautionWarningState();
    pass = bitRead(state.cautionWarningState, CW_CHUTE_ENV) &&
           chuteEnvState == chute_Yellow;
    printResult(pass, "CHUTE_ENV yellow", pass ? "on" : "off");
    if (pass) _logicPassed++; else _logicFailed++;

    // green (safe for mains)
    state.vel_surf = 200.0f;                     // q < main limit
    updateCautionWarningState();
    pass = bitRead(state.cautionWarningState, CW_CHUTE_ENV) &&
           chuteEnvState == chute_Green;
    printResult(pass, "CHUTE_ENV green", pass ? "on" : "off");
    if (pass) _logicPassed++; else _logicFailed++;

    // off (not in atmo)
    inAtmo = false;
    updateCautionWarningState();
    pass = !bitRead(state.cautionWarningState, CW_CHUTE_ENV) &&
           chuteEnvState == chute_Off;
    printResult(pass, "CHUTE_ENV off", pass ? "off" : "still on");
    if (pass) _logicPassed++; else _logicFailed++;
  }

  // CW_SRB_ACTIVE -- latched by a falling reading, held across recomputes with the
  // reading unchanged, released by exhaustion or a full (new) stage.
  {
    resetTestState();
    state.SF_stage_tot = 1000.0f;
    state.SF_stage     = 900.0f;   // 90% -- within 99%/0.5% bounds
    updateCautionWarningState();   // initialises _prevSF = 900
    state.SF_stage     = 850.0f;   // decreasing -- SRB burning
    updateCautionWarningState();
    checkBit(CW_SRB_ACTIVE, true, "SRB_ACTIVE", "on: SF decreasing");
    updateCautionWarningState();   // unchanged reading: must stay lit
    checkBit(CW_SRB_ACTIVE, true, "SRB_ACTIVE", "on: held across an unrelated recompute");

    state.SF_stage     = 2.0f;     // 0.2% -- exhausted
    updateCautionWarningState();
    checkBit(CW_SRB_ACTIVE, false, "SRB_ACTIVE", "off: SF exhausted");

    state.SF_stage     = 1000.0f;  // a fresh full stage
    updateCautionWarningState();
    checkBit(CW_SRB_ACTIVE, false, "SRB_ACTIVE", "off: new full stage");
  }

  // CW_EVA_ACTIVE
  TEST_BIT("EVA_ACTIVE",
    CW_EVA_ACTIVE,
    { inEVA = true; },
    { inEVA = false; });

  // ---------------------------------------------------------------------------------
  // FLIGHT CONDITION INDICATORS
  // flightCondIndex() is static in ScreenMain.ino so we replicate its logic inline.
  // Returns: 0=FLYING LOW, 1=LOW SPACE, 2=FLYING HIGH, 3=HIGH SPACE
  // ---------------------------------------------------------------------------------
  Serial.println();
  Serial.println(F("--- Flight Condition Indicators ---"));

  {
    struct FCTest { const char *name; int8_t expected; bool atmo; float alt; float flyHigh; float highSpace; };
    FCTest fcTests[] = {
      { "FLYING LOW",         0, true,  10000.0f, 18000.0f, 250000.0f },
      { "FLYING HIGH",        2, true,  30000.0f, 18000.0f, 250000.0f },
      { "LOW SPACE",          1, false,100000.0f, 18000.0f, 250000.0f },
      { "HIGH SPACE",         3, false,300000.0f, 18000.0f, 250000.0f },
      { "LOW SPACE airless",  1, false, 30000.0f,     0.0f,  60000.0f },
      { "HIGH SPACE airless", 3, false, 80000.0f,     0.0f,  60000.0f },
    };
    for (uint8_t fi = 0; fi < sizeof(fcTests)/sizeof(fcTests[0]); fi++) {
      int8_t result;
      if (fcTests[fi].atmo)
        result = (fcTests[fi].flyHigh > 0 && fcTests[fi].alt > fcTests[fi].flyHigh) ? 2 : 0;
      else
        result = (fcTests[fi].highSpace > 0 && fcTests[fi].alt > fcTests[fi].highSpace) ? 3 : 1;
      bool pass = (result == fcTests[fi].expected);
      char detail[32];
      snprintf(detail, sizeof(detail), "expected %d got %d", (int)fcTests[fi].expected, (int)result);
      printResult(pass, fcTests[fi].name, detail);
    }
  }

  Serial.println();
  Serial.println(F("=== LOGIC TEST SUMMARY ==="));
  Serial.print(F("  PASSED: ")); Serial.println(_logicPassed);
  Serial.print(F("  FAILED: ")); Serial.println(_logicFailed);
  Serial.println(F("=========================="));

  resetTestState();
  updateCautionWarningState();
  printMenu();
}


/***************************************************************************************
   DISPLAY WALK-THROUGH
   Each step illuminates one indicator in isolation and describes it in the Serial monitor.
   Directly writes state.cautionWarningState rather than going through logic,
   so rendering is verified independently of logic correctness.
   Steps: 25 C&W buttons + 2-tier variants + situation column + regime tiles + the
   twelve mode-grid tiles (state.modeFlags, as the master drives them) + SPCFT states.
****************************************************************************************/
struct DisplayStep {
  const char   *name;
  uint32_t      cwBits;
  bool          peYellow;
  bool          propYellow;
  bool          lsYellow;
  ChuteEnvState chuteState;
  uint8_t       sitBits;
  // Flight condition fields
  bool          fcInAtmo;
  float         fcAlt;
  // Mode grid: the MF_* bits to light (state.modeFlags, the master's word)
  uint16_t      modeBits;
  // Vehicle control mode and type for the SPCFT/PLN/RVR tile
  // -1 = no override (use default ctrl_Spacecraft / type_Ship)
  int8_t        psVehCtrl;    // CtrlMode value: 0=ctrl_Rover, 1=ctrl_Plane, 2=ctrl_Spacecraft
  int8_t        psVesselType; // VesselType value (type_Ship=7, type_Plane=8, type_Rover=5)
  const char   *description;
};

// Field order: name, cwBits, peY, propY, lsY, chuteState, sitBits,
//              fcInAtmo, fcAlt, modeBits, psVehCtrl, psVesselType, description
#define DS_PLAIN(n,cw,pe,pr,ls,ch,sit,desc) \
  { n, cw, pe, pr, ls, ch, sit, false, 0.0f, 0, -1, -1, desc }
#define DS_MODE(n,bits,desc) \
  { n, 0, false, false, false, chute_Off, 0, false, 0.0f, bits, -1, -1, desc }
#define DS_SPCFT(n,ctrl,type,desc) \
  { n, 0, false, false, false, chute_Off, 0, false, 0.0f, 0, ctrl, type, desc }

static const DisplayStep _displaySteps[] = {
  // --- C&W buttons -- Row 0 ---
  DS_PLAIN("LOW_DV",         1ul<<CW_LOW_DV,       false,false,false, chute_Off, 0, "Row 0: RED -- master alarm"),
  DS_PLAIN("HIGH_G",         1ul<<CW_HIGH_G,       false,false,false, chute_Off, 0, "Row 0: RED -- master alarm"),
  DS_PLAIN("HIGH_TEMP",      1ul<<CW_HIGH_TEMP,    false,false,false, chute_Off, 0, "Row 0: RED -- master alarm"),
  DS_PLAIN("BUS_VOLTAGE",    1ul<<CW_BUS_VOLTAGE,  false,false,false, chute_Off, 0, "Row 0: RED -- master alarm"),
  DS_PLAIN("ABORT",          1ul<<CW_ABORT,        false,false,false, chute_Off, 0, "Row 0: RED -- master alarm"),
  // --- C&W buttons -- Row 1 ---
  DS_PLAIN("GROUND_PROX",    1ul<<CW_GROUND_PROX,  false,false,false, chute_Off, 0, "Row 1: RED -- master alarm"),
  DS_PLAIN("PE_LOW yellow",  0,                    true, false,false, chute_Off, 0, "Row 1: YELLOW -- aerobrake zone"),
  DS_PLAIN("PE_LOW red",     1ul<<CW_PE_LOW,       false,false,false, chute_Off, 0, "Row 1: RED -- committed reentry"),
  DS_PLAIN("PROP_LOW yellow",0,                    false,true, false, chute_Off, 0, "Row 1: YELLOW -- prop below 20%"),
  DS_PLAIN("PROP_LOW red",   1ul<<CW_PROP_LOW,     false,false,false, chute_Off, 0, "Row 1: RED -- prop below 5%"),
  DS_PLAIN("LIFE_SUPP yel",  0,                    false,false,true,  chute_Off, 0, "Row 1: YELLOW -- resource caution"),
  DS_PLAIN("LIFE_SUPP red",  1ul<<CW_LIFE_SUPPORT, false,false,false, chute_Off, 0, "Row 1: RED -- resource critical"),
  DS_PLAIN("O2_PRESENT",     1ul<<CW_O2_PRESENT,   false,false,false, chute_Off, 0, "Row 1: BLUE -- breathable atmosphere"),
  // --- C&W buttons -- Row 2 ---
  DS_PLAIN("IMPACT_IMM",     1ul<<CW_IMPACT_IMM,   false,false,false, chute_Off, 0, "Row 2: YELLOW -- <60s to impact"),
  DS_PLAIN("ALT",            1ul<<CW_ALT,          false,false,false, chute_Off, 0, "Row 2: YELLOW -- below 200m"),
  DS_PLAIN("DESCENT",        1ul<<CW_DESCENT,      false,false,false, chute_Off, 0, "Row 2: YELLOW -- descending"),
  DS_PLAIN("GEAR_UP",        1ul<<CW_GEAR_UP,      false,false,false, chute_Off, 0, "Row 2: YELLOW -- gear up below 200m"),
  DS_PLAIN("ATMO",           1ul<<CW_ATMO,         false,false,false, chute_Off, 0, "Row 2: YELLOW -- inside atmosphere"),
  // --- C&W buttons -- Row 3 ---
  DS_PLAIN("RCS_LOW",        1ul<<CW_RCS_LOW,      false,false,false, chute_Off, 0, "Row 3: YELLOW -- MonoProp below 20%"),
  DS_PLAIN("PROP_IMBAL",     1ul<<CW_PROP_IMBAL,   false,false,false, chute_Off, 0, "Row 3: YELLOW -- LF/OX ratio off"),
  DS_PLAIN("COMM_LOST",      1ul<<CW_COMM_LOST,    false,false,false, chute_Off, 0, "Row 3: YELLOW -- no CommNet signal"),
  DS_PLAIN("Ap_LOW",         1ul<<CW_Ap_LOW,       false,false,false, chute_Off, 0, "Row 3: YELLOW -- Ap inside atmosphere"),
  DS_PLAIN("HIGH_Q",         1ul<<CW_HIGH_Q,       false,false,false, chute_Off, 0, "Row 3: YELLOW -- dynamic pressure high"),
  // --- C&W buttons -- Row 4 ---
  DS_PLAIN("ORBIT_STABLE",   1ul<<CW_ORBIT_STABLE, false,false,false, chute_Off, 0, "Row 4: GREEN -- stable orbit confirmed"),
  DS_PLAIN("ELEC_GEN",       1ul<<CW_ELEC_GEN,     false,false,false, chute_Off, 0, "Row 4: GREEN -- EC charging"),
  DS_PLAIN("CHUTE_ENV red",  1ul<<CW_CHUTE_ENV,    false,false,false, chute_Red,    0, "Row 4: RED -- too fast for any chute"),
  DS_PLAIN("CHUTE_ENV yel",  1ul<<CW_CHUTE_ENV,    false,false,false, chute_Yellow, 0, "Row 4: YELLOW -- drogue safe, mains unsafe"),
  DS_PLAIN("CHUTE_ENV grn",  1ul<<CW_CHUTE_ENV,    false,false,false, chute_Green,  0, "Row 4: GREEN -- safe for main chutes"),
  DS_PLAIN("SRB_ACTIVE",     1ul<<CW_SRB_ACTIVE,   false,false,false, chute_Off, 0, "Row 4: ORANGE -- SRB burning"),
  DS_PLAIN("EVA_ACTIVE",     1ul<<CW_EVA_ACTIVE,   false,false,false, chute_Off, 0, "Row 4: ORANGE -- Kerbal on EVA"),
  // --- ALL OFF ---
  DS_PLAIN("ALL OFF",        0, false,false,false, chute_Off, 0, "All C&W dark -- verify no phantom illumination"),
  // --- Situation column ---
  // Note: CNTCT illuminates whenever SPLASH or LANDED is set -- no separate step needed.
  DS_PLAIN("SIT: PRE-LNCH", 0,false,false,false,chute_Off,(1<<VSIT_PRELAUNCH), "Situation: PRE- LNCH (green)"),
  DS_PLAIN("SIT: FLIGHT",   0,false,false,false,chute_Off,(1<<VSIT_FLIGHT),    "Situation: FLIGHT (green)"),
  DS_PLAIN("SIT: SUB-ORB",  0,false,false,false,chute_Off,(1<<VSIT_SUBORBIT),  "Situation: SUB- ORB (green)"),
  DS_PLAIN("SIT: ORBIT",    0,false,false,false,chute_Off,(1<<VSIT_ORBIT),     "Situation: ORBIT (green)"),
  DS_PLAIN("SIT: ESCAPE",   0,false,false,false,chute_Off,(1<<VSIT_ESCAPE),    "Situation: ESCAPE (green)"),
  DS_PLAIN("SIT: SPLASH",   0,false,false,false,chute_Off,(1<<VSIT_SPLASH),    "Situation: SPLASH (blue) + CNTCT (blue)"),
  DS_PLAIN("SIT: LANDED",   0,false,false,false,chute_Off,(1<<VSIT_LANDED),    "Situation: LANDED (green) + CNTCT (blue)"),
  DS_PLAIN("SIT: DOCK",     0,false,false,false,chute_Off,(1<<VSIT_DOCKED),    "DOCK indicator (green vertical text)"),
  // --- Regime column ---
  { "FC: FLYING LOW",  0,false,false,false,chute_Off,(1<<VSIT_FLIGHT), true,  10000.0f, 0,-1,-1, "Regime: FLYING LOW (green)" },
  { "FC: FLYING HIGH", 0,false,false,false,chute_Off,(1<<VSIT_FLIGHT), true,  30000.0f, 0,-1,-1, "Regime: FLYING HIGH (green)" },
  { "FC: LOW SPACE",   0,false,false,false,chute_Off,(1<<VSIT_ORBIT),  false,100000.0f, 0,-1,-1, "Regime: LOW SPACE (green)" },
  { "FC: HIGH SPACE",  0,false,false,false,chute_Off,(1<<VSIT_ORBIT),  false,300000.0f, 0,-1,-1, "Regime: HIGH SPACE (green)" },
  // --- Mode grid (state.modeFlags, one tile at a time, then all) ---
  DS_MODE("MODE: DEMO",        1u << MF_DEMO,        "Mode grid: DEMO (blue)"),
  DS_MODE("MODE: WARP",        1u << MF_WARP,        "Mode grid: WARP (yellow)"),
  DS_MODE("MODE: AUDIO",       1u << MF_AUDIO,       "Mode grid: AUDIO (green)"),
  DS_MODE("MODE: THRTL ENA",   1u << MF_THRTL_ENA,   "Mode grid: THRTL ENA (green)"),
  DS_MODE("MODE: TRIM",        1u << MF_TRIM,        "Mode grid: TRIM (aqua)"),
  DS_MODE("MODE: AUTOPILOT",   1u << MF_AUTOPILOT,   "Mode grid: AUTOPILOT (green)"),
  DS_MODE("MODE: DEBUG",       1u << MF_DEBUG,       "Mode grid: DEBUG (purple)"),
  DS_MODE("MODE: SWITCH ERR",  1u << MF_SWITCH_ERR,  "Mode grid: SWITCH ERR (red)"),
  DS_MODE("MODE: SIMPIT LOST", 1u << MF_SIMPIT_LOST, "Mode grid: SIMPIT LOST (red)"),
  DS_MODE("MODE: THRTL PREC",  1u << MF_THRTL_PREC,  "Mode grid: THRTL PREC (green)"),
  DS_MODE("MODE: INPUT PREC",  1u << MF_INPUT_PREC,  "Mode grid: INPUT PREC (green)"),
  DS_MODE("MODE: ENG ARM",     1u << MF_ENG_ARM,     "Mode grid: ENG ARM (green)"),
  DS_MODE("MODE: ALL ON",      (uint16_t)((1u << MF_COUNT) - 1), "Mode grid: every tile lit"),
  // --- SPCFT tile: control mode against vessel type ---
  DS_SPCFT("SPCFT match", ctrl_Spacecraft, type_Ship,  "SPCFT tile: green SPCFT (ctrl_Spacecraft, type_Ship) + ship icon"),
  DS_SPCFT("PLN match",   ctrl_Plane,      type_Plane, "SPCFT tile: green PLN (ctrl_Plane, type_Plane) + plane icon"),
  DS_SPCFT("RVR match",   ctrl_Rover,      type_Rover, "SPCFT tile: green RVR (ctrl_Rover, type_Rover) + rover icon"),
  DS_SPCFT("SPCFT err",   ctrl_Plane,      type_Ship,  "SPCFT tile: red PLN (ctrl_Plane but type_Ship -- mismatch)"),
  // --- ALL OFF ---
  DS_SPCFT("ALL OFF",     ctrl_Spacecraft, type_Ship,  "Everything dark; SPCFT green, mode grid unlit"),
};

static const uint8_t _displayStepCount =
    sizeof(_displaySteps) / sizeof(_displaySteps[0]);


static void runDisplayWalkthrough() {
  if (_displayStep >= _displayStepCount) {
    Serial.println(F("\nDisplay walk-through complete."));
    _inDisplayWalk = false;
    resetTestState();
    updateCautionWarningState();
    printMenu();
    return;
  }

  const DisplayStep &step = _displaySteps[_displayStep];

  // Reset to clean baseline first
  resetTestState();

  // Force full redraw -- must happen before applying step state
  invalidateAllState();
  resetSitAndPanelState();  // resets _prevContact and prevModeFlags sentinels

  // Apply step state after invalidation so it isn't overwritten
  state.cautionWarningState  = step.cwBits;
  peLowYellow                = step.peYellow;
  propLowYellow              = step.propYellow;
  lsYellow                   = step.lsYellow;
  chuteEnvState              = step.chuteState;
  state.vesselSituationState = step.sitBits;
  state.masterAlarmOn        = (step.cwBits & masterAlarmMask) != 0;
  inAtmo                     = step.fcInAtmo;
  state.alt_sl               = step.fcAlt;
  state.modeFlags            = step.modeBits;

  // Vehicle control mode and vessel type for the SPCFT tile
  if (step.psVehCtrl >= 0)    state.vehCtrlMode = (CtrlMode)step.psVehCtrl;
  if (step.psVesselType >= 0) state.vesselType  = (VesselType)step.psVesselType;

  // Draw each zone directly without blanking the screen (no flicker).
  // Force prev to differ from current state so each update function redraws.
  prev.masterAlarmOn        = !state.masterAlarmOn;
  prev.cautionWarningState  = ~state.cautionWarningState;
  prev.vesselSituationState = ~state.vesselSituationState;
  prevChuteEnvState         = (chuteEnvState == chute_Off) ? chute_Red : chute_Off;
  // Force CNTCT to redraw correctly regardless of previous step state
  bool newContact = bitRead(state.vesselSituationState, VSIT_LANDED) ||
                    bitRead(state.vesselSituationState, VSIT_SPLASH);
  forceContactState(newContact);

  // Force DOCK to redraw correctly
  forceDockState(bitRead(state.vesselSituationState, VSIT_DOCKED));

  // Draw all zones (same fonts as the live chrome, so a step looks like flight)
  drawButton(infoDisp, MASTER_X, MASTER_Y, MASTER_W, MASTER_H,
             masterAlarmLabel, &Roboto_Black_48, state.masterAlarmOn);
  prev.masterAlarmOn = state.masterAlarmOn;
  updateCautWarnPanel(infoDisp, ~state.cautionWarningState, state.cautionWarningState);
  prev.cautionWarningState = state.cautionWarningState;
  updateVesselSitPanel(infoDisp, ~state.vesselSituationState, state.vesselSituationState);
  prev.vesselSituationState = state.vesselSituationState;
  updateDockedIndicator(infoDisp);
  updateRegimeColumn(infoDisp);
  updateModeGrid(infoDisp);
  updateSpcftTile(infoDisp);
  drawBottomZonePerimeter(infoDisp);

  // Serial output
  Serial.println();
  Serial.print(F("Step "));
  Serial.print(_displayStep + 1);
  Serial.print(F("/"));
  Serial.print(_displayStepCount);
  Serial.print(F("  ["));
  Serial.print(step.name);
  Serial.print(F("]  "));
  Serial.println(step.description);
  Serial.println(F("  ENTER/N=next  B=back  Q=quit"));
}
