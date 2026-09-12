// Render scenarios for the Annunciator, for tools/render_screen.py.
//
// Appended to the concatenated sketch by the tool, so it sees every tab's globals.
// Defines the core objects the stub headers declare extern, sets up one named
// flight state, and runs the firmware's own loop() — the screen is drawn by exactly
// the code path the panel uses. C&W bits come from updateCautionWarningState()
// on the telemetry below, not from hand-set bits, so a rendering shows what the
// panel would actually light for that state.
//
// Scenarios (argv[1]):
//   MainOrbit     Main screen, stable 115x125 km Kerbin orbit, nothing to report.
//   MainReentry   Main screen mid re-entry: committed Pe, hot skin, plasma blackout.
//   MainLampTest  Main screen with every tile lit (Demo.ino's lamp test) — a key to
//                 every indicator's ON colour.
//   SOI           SOI detail screen for Kerbin.
//   Standby       Standby splash.

HardwareSerial Serial, SerialUSB1, Serial1, Serial2, Serial3;
TwoWire Wire, Wire1, Wire2;
volatile uint32_t ARM_DWT_CYCCNT, ARM_DWT_CTRL, ARM_DEMCR;

static void kcmRunFrames(int n) { for (int i = 0; i < n; i++) { loop(); stubMillis() += 50; } }

// Crewed stock-style ship in a stable low Kerbin orbit.
static void kcmNominalOrbit() {
  state.vesselName = "KERBAL X";
  state.vesselType = type_Ship;  state.vehCtrlMode = ctrl_Spacecraft;
  state.gameSOI = "Kerbin";      currentBody = getBodyParams("Kerbin");
  state.vesselSituationState = (1 << VSIT_ORBIT);
  inFlight = true; flightScene = true; inAtmo = false; hasO2 = false; inEVA = false;
  state.alt_sl = 120000.0f; state.alt_surf = 120000.0f;
  state.apoapsis = 125000.0f; state.periapsis = 115000.0f;
  state.vel_surf = 2250.0f; state.vel_vert = 0.0f; state.gForces = 0.0f;
  state.stageDV = 1450.0f; state.stageBurnTime = 180.0f; state.throttleCmd = 0;
  state.EC = 820.0f; state.EC_total = 1000.0f;
  state.LF_stage = 540.0f; state.LF_stage_tot = 720.0f;
  state.OX_stage = 660.0f; state.OX_stage_tot = 880.0f;
  state.mono = 90.0f; state.mono_tot = 100.0f;
  state.tacFood = 300.0f;    state.tacFood_tot = 300.0f;      // TAC-LS supplies for 3 crew
  state.tacWater = 200.0f;   state.tacWater_tot = 200.0f;
  state.tacOxygen = 2000.0f; state.tacOxygen_tot = 2000.0f;
  state.maxTemp = 12; state.skinTemp = 8; state.crewCount = 3; state.stage = 2;
  state.twIndex = 0; physTW = false; state.commNet = 100; state.ctrlGrp = 1; state.capValue = 0;
  state.gear_on = false; state.abort_on = false;
  state.modeFlags = (1u << MF_AUDIO) | (1u << MF_THRTL_ENA);
}

// The same ship 22 km up on a committed re-entry, in the plasma blackout.
static void kcmReentry() {
  kcmNominalOrbit();
  state.vesselSituationState = (1 << VSIT_SUBORBIT);
  inAtmo = true; hasO2 = true;
  state.alt_sl = 22000.0f; state.alt_surf = 22000.0f;
  state.apoapsis = 80000.0f; state.periapsis = -50000.0f;    // Pe below reentryAlt: Pe LOW red
  state.vel_surf = 2300.0f; state.vel_vert = -600.0f; state.gForces = 4.2f;   // ~37 s to impact
  state.airDensity = 0.07f; state.atmoPressure = 4.5f;       // q ~185 kPa: past drogue limit, CHUTE ENV red
  state.stageDV = 0.0f; state.stageBurnTime = 0.0f; state.throttleCmd = 0;
  state.LF_stage = 72.0f; state.OX_stage = 88.0f;              // 10 %: PROP LOW yellow tier
  state.mono = 15.0f;                                          // RCS LOW
  state.maxTemp = 78; state.skinTemp = 94;                     // skin past tempAlarm: HIGH TEMP
  state.commNet = 0;                                           // blackout: COMM LOST
  state.stage = 4;
}

int main(int argc, char **argv) {
  const char *scenario = argc > 1 ? argv[1] : "MainOrbit";
  const char *outPath  = argc > 2 ? argv[2] : "screen.ppm";
  stubMillis() = 60000;                // well past every start-up hold-off window
  demoMode = false; audioEnabled = false; debugMode = false;
  standaloneMode = true; standaloneTest = false; lampTest = false;
  setupSD();

  String sc = scenario;
  if (sc == "MainOrbit" || sc == "MainReentry" || sc == "SOI") {
    if (sc == "MainReentry") kcmReentry(); else kcmNominalOrbit();
    updateCautionWarningState();
    switchToScreen(sc == "SOI" ? screen_SOI : screen_Main);
  } else if (sc == "MainLampTest") {
    kcmNominalOrbit();
    demoMode = true; lampTest = true;     // Demo.ino's graphics check: every tile ON
    state.modeFlags = 0;                  // (the lamp test sets them all itself)
    switchToScreen(screen_Main);
  } else if (sc == "Standby") {
    switchToScreen(screen_Standby);
  } else {
    fprintf(stderr, "unknown scenario '%s'\n", scenario);
    return 2;
  }
  kcmRunFrames(3);                        // chrome pass + update passes, as on the panel
  if (!infoDisp.writePPM(outPath)) { fprintf(stderr, "cannot write %s\n", outPath); return 1; }
  fprintf(stderr, "%s: C&W 0x%08X master=%d\n", scenario,
          (unsigned)state.cautionWarningState, (int)state.masterAlarmOn);
  return 0;
}
