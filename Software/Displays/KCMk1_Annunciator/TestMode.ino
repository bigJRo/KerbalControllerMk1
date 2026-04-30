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
        Steps through every C&W indicator, situation button, panel status button,
        and flight condition indicator one at a time. Directly forces state to
        illuminate each indicator in isolation so rendering is verified independently
        of logic. Advance with ENTER or N. Go back with B. Quit with Q.

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

// Force-on mask for panel status buttons during display walk-through.
// Read by updatePanelStatus() in ScreenMain.ino to override not-yet-wired items.
// Declared here, externed in ScreenMain.ino.
uint16_t testPsForceOn = 0;


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
      if (activeScreen == screen_Main) {
        drawStaticMain(infoDisp);
        firstPassOnMain = true;
      }
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
  // skinTemp defaults to 100 which exceeds tempAlarm(90) -> triggers HIGH_TEMP
  state.skinTemp  = 0;
  state.maxTemp   = 0;
  // periapsis defaults to 0 which is below reentryAlt(45000) -> triggers PE_LOW
  // Set well above Kerbin atmosphere (70000m) so Pe is safe
  state.periapsis = 150000.0f;
  state.apoapsis  = 200000.0f;

  // Reset telemetry flags
  inFlight        = false;
  inEVA           = false;
  hasTarget       = false;
  docked          = false;
  isRecoverable   = false;
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

  // Reset panel mode flags to safe display baseline
  // These must be reset here so display walk-through steps don't bleed into each other
  demoMode        = false;
  debugMode       = false;
  audioEnabled    = false;
  simpitConnected = true;   // default connected so SIMPIT LOST is off

  // Reset panel status force-on override
  testPsForceOn = 0;

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

  // CW_ELEC_GEN -- tested via delta tracking (needs two calls)
  {
    resetTestState();
    state.EC = 500.0f;
    updateCautionWarningState();  // initialises _prevEC
    state.EC = 505.0f;            // EC increased
    updateCautionWarningState();
    checkBit(CW_ELEC_GEN, true, "ELEC_GEN", "on: EC increasing");

    state.EC = 504.0f;            // EC decreased
    updateCautionWarningState();
    checkBit(CW_ELEC_GEN, false, "ELEC_GEN", "off: EC decreasing");
  }

  // CW_CHUTE_ENV -- red (too fast for drogue)
  {
    resetTestState();
    inAtmo         = true;
    state.vel_surf = CW_CHUTE_DROGUE_MAX_SPEED + 100.0f;
    updateCautionWarningState();
    bool pass = bitRead(state.cautionWarningState, CW_CHUTE_ENV) &&
                chuteEnvState == chute_Red;
    printResult(pass, "CHUTE_ENV red", pass ? "on" : "off");
    if (pass) _logicPassed++; else _logicFailed++;

    // yellow (safe for drogue, not main)
    state.vel_surf = CW_CHUTE_MAIN_MAX_SPEED + 10.0f;
    updateCautionWarningState();
    pass = bitRead(state.cautionWarningState, CW_CHUTE_ENV) &&
           chuteEnvState == chute_Yellow;
    printResult(pass, "CHUTE_ENV yellow", pass ? "on" : "off");
    if (pass) _logicPassed++; else _logicFailed++;

    // green (safe for mains)
    state.vel_surf = CW_CHUTE_MAIN_MAX_SPEED - 10.0f;
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

  // CW_SRB_ACTIVE -- needs delta tracking
  {
    resetTestState();
    state.SF_stage_tot = 1000.0f;
    state.SF_stage     = 900.0f;   // 90% -- within 99%/0.5% bounds
    updateCautionWarningState();   // initialises _prevSF = 900
    state.SF_stage     = 850.0f;   // decreasing -- SRB burning
    updateCautionWarningState();
    checkBit(CW_SRB_ACTIVE, true, "SRB_ACTIVE", "on: SF decreasing");

    state.SF_stage     = 849.9f;   // still decreasing
    updateCautionWarningState();
    state.SF_stage     = 849.9f;   // stable -- no longer decreasing
    updateCautionWarningState();
    checkBit(CW_SRB_ACTIVE, false, "SRB_ACTIVE", "off: SF stable");
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

  // ---------------------------------------------------------------------------------
  // PANEL STATUS INDICATORS
  // Tests the boolean state flags that drive each panel status button.
  // ---------------------------------------------------------------------------------
  Serial.println();
  Serial.println(F("--- Panel Status Indicators ---"));

  // Save current flags
  bool saveDemoMode       = demoMode;
  bool saveDebugMode      = debugMode;
  bool saveAudioEnabled   = audioEnabled;
  bool saveSimpitConn     = simpitConnected;
  bool saveStandaloneMode = standaloneMode;
  bool saveStandaloneTest = standaloneTest;
  uint8_t saveTwIndex     = state.twIndex;

  // DEMO on/off
  demoMode = true; debugMode = false;
  printResult(demoMode && !debugMode,  "DEMO on",       "demoMode=true debugMode=false");
  demoMode = false;
  printResult(!demoMode && !debugMode, "DEMO off",      "demoMode=false");

  // DEBUG on/off (same DEMO button, purple)
  debugMode = true; demoMode = false;
  printResult(debugMode,               "DEBUG on",      "debugMode=true");
  debugMode = false;
  printResult(!debugMode,              "DEBUG off",     "debugMode=false");

  // WARP on/off
  state.twIndex = 3;
  printResult(state.twIndex > 0,       "WARP on",       "twIndex=3");
  state.twIndex = 0;
  printResult(!(state.twIndex > 0),    "WARP off",      "twIndex=0");

  // AUDIO on/off
  audioEnabled = true;
  printResult(audioEnabled,            "AUDIO on",      "audioEnabled=true");
  audioEnabled = false;
  printResult(!audioEnabled,           "AUDIO off",     "audioEnabled=false");

  // SIMPIT LOST on/off
  simpitConnected = false;
  printResult(!simpitConnected,        "SIMPIT LOST on",  "simpitConnected=false");
  simpitConnected = true;
  printResult(simpitConnected,         "SIMPIT LOST off", "simpitConnected=true");

  // Restore flags
  demoMode        = saveDemoMode;
  debugMode       = saveDebugMode;
  audioEnabled    = saveAudioEnabled;
  simpitConnected = saveSimpitConn;
  standaloneMode  = saveStandaloneMode;
  standaloneTest  = saveStandaloneTest;
  state.twIndex   = saveTwIndex;

  Serial.println(F("  NOTE: THRTL ENA, THRTL PREC, PREC INPUT, TRIM SET,"));
  Serial.println(F("        SWITCH ERR, SPCFT -- not yet wired to I2C,"));
  Serial.println(F("        tested in display walk-through only."));
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
   Steps: 25 C&W buttons + 2-tier variants + situation column + panel status + flight cond.
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
  // Panel status overrides (-1=no override, 0=off, 1=on/demoMode, 2=debugMode)
  int8_t        psDemo;
  int8_t        psWarp;
  int8_t        psAudio;
  int8_t        psSimpit;
  // Force specific panel status buttons ON regardless of wiring state
  // Bit positions match panel status array indices 0-9
  uint16_t      psForceOn;
  // Vehicle control mode and type for SPCFT/PLN/RVR button testing
  // -1 = no override (use default ctrl_Spacecraft / type_Ship)
  int8_t        psVehCtrl;    // 0=ctrl_Rover, 1=ctrl_Plane, 2=ctrl_Spacecraft
  int8_t        psVesselType; // 0=type_Ship, 1=type_Plane, 2=type_Rover
  const char   *description;
};

// Field order: name, cwBits, peY, propY, lsY, chuteState, sitBits,
//              fcInAtmo, fcAlt, psDemo, psWarp, psAudio, psSimpit, psForceOn, description
// psDemo: -1=no override, 0=CTRL(green), 1=demoMode(blue), 2=debugMode(purple)
// psWarp/psAudio/psSimpit: -1=no override, 0=off, 1=on
// psForceOn: bitmask of panel status indices to force ON (for not-yet-wired items)
// psVehCtrl: -1=no override, 0=ctrl_Rover, 1=ctrl_Plane, 2=ctrl_Spacecraft
// psVesselType: -1=no override, 0=type_Ship, 1=type_Plane, 2=type_Rover
#define DS_PLAIN(n,cw,pe,pr,ls,ch,sit,desc) \
  { n, cw, pe, pr, ls, ch, sit, false, 0.0f, -1, -1, -1, -1, 0, -1, -1, desc }

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
  // --- Flight condition block ---
  { "FC: FLYING LOW",  0,false,false,false,chute_Off,(1<<VSIT_FLIGHT), true,  10000.0f,-1,-1,-1,-1,0,-1,-1, "Flight cond: FLYING LOW (green)" },
  { "FC: FLYING HIGH", 0,false,false,false,chute_Off,(1<<VSIT_FLIGHT), true,  30000.0f,-1,-1,-1,-1,0,-1,-1, "Flight cond: FLYING HIGH (green)" },
  { "FC: LOW SPACE",   0,false,false,false,chute_Off,(1<<VSIT_ORBIT),  false,100000.0f,-1,-1,-1,-1,0,-1,-1, "Flight cond: LOW SPACE (green)" },
  { "FC: HIGH SPACE",  0,false,false,false,chute_Off,(1<<VSIT_ORBIT),  false,300000.0f,-1,-1,-1,-1,0,-1,-1, "Flight cond: HIGH SPACE (green)" },
  // --- Panel status strip ---
  { "PS: DEMO",        0,false,false,false,chute_Off,0,false,0.0f, 1,-1,-1,-1,0,-1,-1, "Panel status: DEMO button (black bg, blue text=DEMO)" },
  { "PS: DEBUG",       0,false,false,false,chute_Off,0,false,0.0f, 2,-1,-1,-1,0,-1,-1, "Panel status: DEMO button (black bg, purple text=DEBUG)" },
  { "PS: CTRL",        0,false,false,false,chute_Off,0,false,0.0f, 0,-1,-1,-1,0,-1,-1, "Panel status: DEMO button (black bg, green text=CTRL)" },
  { "PS: WARP",        0,false,false,false,chute_Off,0,false,0.0f,-1, 1,-1,-1,0,-1,-1, "Panel status: WARP (yellow bg)" },
  { "PS: AUDIO",       0,false,false,false,chute_Off,0,false,0.0f,-1,-1, 1,-1,0,-1,-1, "Panel status: AUDIO (green bg)" },
  { "PS: THRTL ENA",   0,false,false,false,chute_Off,0,false,0.0f,-1,-1,-1,-1,(1<<3),-1,-1, "Panel status: THRTL ENA (green bg) -- forced on" },
  { "PS: TRIM SET",    0,false,false,false,chute_Off,0,false,0.0f,-1,-1,-1,-1,(1<<4),-1,-1, "Panel status: TRIM SET (cyan bg) -- forced on" },
  { "PS: THRTL PREC",  0,false,false,false,chute_Off,0,false,0.0f,-1,-1,-1,-1,(1<<8),-1,-1, "Panel status: THRTL PREC (green bg) -- forced on" },
  { "PS: PREC INPUT",  0,false,false,false,chute_Off,0,false,0.0f,-1,-1,-1,-1,(1<<9),-1,-1, "Panel status: PREC INPUT (green bg) -- forced on" },
  { "PS: SIMPIT LOST", 0,false,false,false,chute_Off,0,false,0.0f,-1,-1,-1, 1,0,-1,-1, "Panel status: SIMPIT LOST (red bg)" },
  // --- SPCFT/PLN/RVR button states (black bg, colored text) ---
  // psVehCtrl:    0=ctrl_Rover, 1=ctrl_Plane, 2=ctrl_Spacecraft
  // psVesselType: actual VesselType enum values: type_Ship=7, type_Plane=8, type_Rover=5
  { "PS: SPCFT match", 0,false,false,false,chute_Off,0,false,0.0f,-1,-1,-1,-1,0, 2, 7, "SPCFT button: black bg, green text=SPCFT (ctrl_Spacecraft, type_Ship)" },
  { "PS: PLN match",   0,false,false,false,chute_Off,0,false,0.0f,-1,-1,-1,-1,0, 1, 8, "SPCFT button: black bg, green text=PLN (ctrl_Plane, type_Plane)" },
  { "PS: RVR match",   0,false,false,false,chute_Off,0,false,0.0f,-1,-1,-1,-1,0, 0, 5, "SPCFT button: black bg, green text=RVR (ctrl_Rover, type_Rover)" },
  { "PS: SPCFT err",   0,false,false,false,chute_Off,0,false,0.0f,-1,-1,-1,-1,0, 1, 7, "SPCFT button: black bg, red text=PLN (ctrl_Plane but type_Ship -- mismatch)" },
  // --- ALL OFF ---
  { "PS: ALL OFF",     0,false,false,false,chute_Off,0,false,0.0f, 0, 0, 0, 0,0, 2, 7, "Panel status: all dark; CTRL+SPCFT show grey text only (both unlit by design)" },
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
  resetSitAndPanelState();  // resets _prevContact and prevPanelStatusMask sentinels

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

  // Panel status overrides -- applied after invalidation
  if (step.psDemo == 0)       { demoMode = false; debugMode = false; }
  else if (step.psDemo == 1)  { demoMode = true;  debugMode = false; }
  else if (step.psDemo == 2)  { demoMode = false; debugMode = true;  }
  if (step.psWarp   == 0)      state.twIndex    = 0;
  else if (step.psWarp   == 1) state.twIndex    = 3;
  if (step.psAudio  == 0)      audioEnabled     = false;
  else if (step.psAudio  == 1) audioEnabled     = true;
  if (step.psSimpit == 0)      simpitConnected  = true;
  else if (step.psSimpit == 1) simpitConnected  = false;
  testPsForceOn = step.psForceOn;

  // Vehicle control mode and vessel type for SPCFT button
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

  // Draw all zones
  drawButton(infoDisp, MASTER_X, MASTER_Y, MASTER_W, MASTER_H,
             masterAlarmLabel, &Roboto_Black_36, state.masterAlarmOn);
  prev.masterAlarmOn = state.masterAlarmOn;
  updateCautWarnPanel(infoDisp, ~state.cautionWarningState, state.cautionWarningState);
  prev.cautionWarningState = state.cautionWarningState;
  updateVesselSitPanel(infoDisp, ~state.vesselSituationState, state.vesselSituationState);
  prev.vesselSituationState = state.vesselSituationState;
  updateDockedIndicator(infoDisp);
  updateFlightCondBlock(infoDisp);
  updatePanelStatus(infoDisp);

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
