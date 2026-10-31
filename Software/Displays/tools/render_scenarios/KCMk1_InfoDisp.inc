// Render scenarios for the Info Display, for tools/render_screen.py.
//
// Appended to the concatenated sketch by the tool, so it sees every tab's globals.
// Build with -D INFO_DISP_UNIT=1 (default; vehicle panel, sidebar left, VEH key) or
// -D INFO_DISP_UNIT=2 (mission panel, sidebar right, A/P key). Every screen exists on
// both units; only the sidebar differs.
//
// Each scenario is one screen in a representative, physically consistent flight
// state, set directly on the panel's AppState. The demo stepper is run once first so
// every field the screens read (including the autopilot models) starts populated, then
// the scenario overrides what matters and millis() is frozen: the demo stepper is then
// a no-op and the context ladder's answer cannot change. The screen is pinned by the
// manual latch only if the ladder would not choose it itself, so the AUTO / MAN chip
// reads the way it would in flight.
//
// Scenarios: SCFT ACFT ROVR VEH  LNCHPRE LNCH LNCHCIRC  ORB ORBADV MNVR  TGT DOCK NAV
//            LNDG LNDGRE  LNCHAP ORBTAP LNDGAP ACFTAP ROVRAP  Standby
//            INPUT  (the input test screen, Info Display 1: a bench state with a few
//                   inputs down, the throttle touched, the Pre-Warp module missing, TIME
//                   reporting a FAULT, the four displays' status)

HardwareSerial Serial, SerialUSB1, Serial1, Serial2, Serial3;
TwoWire Wire, Wire1, Wire2;
volatile uint32_t ARM_DWT_CYCCNT, ARM_DWT_CTRL, ARM_DEMCR;

// ---- Base: crewed stock-style ship, all switches nominal ---------------------------
static void kcmBaseShip() {
  state.vesselName = "Kerbal X"; state.vesselType = type_Ship; state.ctrlLevel = 3;
  state.isRecoverable = false; state.gameSOI = "Kerbin"; currentBody = getBodyParams("Kerbin");
  state.crewCount = 3; state.crewCapacity = 3; state.commNetSignal = 92;
  state.electricChargePercent = 84.0f; state.coreTempPct = 9; state.skinTempPct = 6;
  state.targetAvailable = false; state.tgtDistance = 0; state.tgtVelocity = 0;
  state.rcs_on = false; state.gear_on = false; state.brakes_on = false; state.airbrake_on = false;
  state.trimEnabled = false; state.drogueDeploy = state.drogueCut = state.mainDeploy = state.mainCut = false;
  state.sasMode = 0;                       // STABILITY ASSIST
  state.mnvrDeltaV = 0; state.mnvrTotalDeltaV = 0; state.mnvrTime = 0; state.mnvrDuration = 0;
  state.apArmed = false; state.apPhase = 0; state.hpPitchMode = state.hpLatMode = state.hpThrMode = 0;
  state.hpFlags = 0; state.hpReason = 0; state.hpReasonAge = 255;
  state.rvFlags = 0; state.rvReason = 0; state.rvReasonAge = 255;
  state.obFlags = 0; state.obMode = 0; state.obPhase = 0; state.obReason = 0; state.obReasonAge = 255;
  state.ldFlags = 0; state.ldMode = 0; state.ldEntry = 0; state.ldReason = 0; state.ldReasonAge = 255;
}

// 115 x 125 km Kerbin orbit, 6 deg inclined, nose prograde. Self-consistent: the vessel
// is at true anomaly 200 deg (just past apoapsis, heading for periapsis), so radius,
// speed (vis-viva, mu = 3.5316e12) and the times to each apsis follow from SMA / ECC.
static void kcmLKO() {
  kcmBaseShip();
  state.situation = sit_Orbit; state.inAtmo = false; state.airDensity = 0.0f;
  state.altitude = 124665.0f; state.radarAlt = 124665.0f;
  state.apoapsis = 125000.0f; state.periapsis = 115000.0f;
  state.semiMajorAxis = 720000.0f; state.eccentricity = 0.0069f;
  state.inclination = 6.0f; state.LAN = 42.0f; state.argOfPe = 118.0f;
  state.trueAnomaly = 200.0f; state.meanAnomaly = 200.3f; state.orbitalPeriod = 2043.0f;
  state.timeToPe = 906.0f; state.timeToAp = 1927.0f;
  state.orbitalVel = 2200.0f; state.surfaceVel = 1990.0f; state.verticalVel = -7.5f;
  state.heading = 90.0f; state.pitch = 2.0f; state.roll = 0.0f;
  state.orbVelHeading = 90.0f; state.orbVelPitch = 0.2f; state.srfVelHeading = 90.0f; state.srfVelPitch = 0.0f;
  state.machNumber = 0; state.IAS = 0; state.gForce = 0.0f; state.throttle = 0.0f;
  state.stageDeltaV = 1450.0f; state.totalDeltaV = 2100.0f; state.stageBurnTime = 180.0f;
}

// Stock-style jet at 4,500 m in a gentle left turn, heading west.
static void kcmPlane() {
  kcmBaseShip();
  state.vesselName = "Aeris 3A"; state.vesselType = type_Plane; state.crewCount = 1; state.crewCapacity = 1;
  state.situation = sit_Flying; state.inAtmo = true; state.airDensity = 0.72f;
  state.altitude = 4500.0f; state.radarAlt = 4310.0f;
  state.apoapsis = 5200.0f; state.periapsis = -598000.0f;
  state.orbitalVel = 380.0f; state.surfaceVel = 210.0f; state.verticalVel = 2.1f;
  state.IAS = 190.0f; state.machNumber = 0.62f; state.gForce = 1.02f; state.throttle = 0.75f;
  state.heading = 270.0f; state.pitch = 5.0f; state.roll = -12.0f;
  state.srfVelHeading = 273.0f; state.srfVelPitch = 4.0f; state.orbVelHeading = 273.0f; state.orbVelPitch = 4.0f;
  state.stageDeltaV = 0.0f; state.totalDeltaV = 0.0f; state.stageBurnTime = 0.0f;
  state.timeToAp = 30.0f; state.timeToPe = 1500.0f;
}

// Rover driving on the Mun.
static void kcmRover() {
  kcmBaseShip();
  state.vesselName = "Rover Mk2"; state.vesselType = type_Rover; state.crewCount = 2; state.crewCapacity = 2;
  state.gameSOI = "Mun"; currentBody = getBodyParams("Mun");
  state.situation = sit_Landed; state.inAtmo = false; state.airDensity = 0.0f;
  state.altitude = 412.0f; state.radarAlt = 2.0f;
  state.surfaceVel = 12.4f; state.orbitalVel = 12.4f; state.verticalVel = 0.4f; state.wheelThrottle = 0.6f;
  state.heading = 45.0f; state.pitch = 3.5f; state.roll = -2.0f;
  state.srfVelHeading = 45.0f; state.srfVelPitch = 2.0f; state.gear_on = true; state.commNetSignal = 64;
  state.stageDeltaV = 0.0f; state.totalDeltaV = 0.0f;
  // Driving to a flag 850 m away, 15 deg right of the nose, closing at 11.8 m/s.
  state.targetAvailable = true; state.tgtDistance = 850.0f; state.tgtVelocity = -11.8f;
  state.tgtHeading = 60.0f; state.tgtPitch = 0.0f;
}

// Lander in powered descent over the Mun, 310 m up.
static void kcmMunDescent() {
  kcmBaseShip();
  state.vesselName = "Munar Lander"; state.vesselType = type_Lander; state.crewCount = 2; state.crewCapacity = 2;
  state.gameSOI = "Mun"; currentBody = getBodyParams("Mun");
  state.situation = sit_SubOrb; state.inAtmo = false; state.airDensity = 0.0f;
  state.altitude = 2400.0f; state.radarAlt = 310.0f;
  state.apoapsis = 14000.0f; state.periapsis = -180000.0f;
  state.orbitalVel = 12.0f; state.surfaceVel = 9.2f; state.verticalVel = -6.5f;
  state.throttle = 0.62f; state.gear_on = true; state.rcs_on = true; state.sasMode = 2;   // RETROGRADE
  state.heading = 45.0f; state.pitch = 88.0f; state.roll = 0.0f;
  state.srfVelHeading = 200.0f; state.srfVelPitch = -60.0f;
  state.stageDeltaV = 780.0f; state.totalDeltaV = 780.0f; state.stageBurnTime = 210.0f;
  state.timeToAp = 0.0f; state.timeToPe = 0.0f;
}

// Capsule on Kerbin re-entry, 58 km, heat shield forward (nose retrograde).
static void kcmReentry() {
  kcmBaseShip();
  state.situation = sit_SubOrb; state.inAtmo = true; state.airDensity = 0.0009f;
  state.altitude = 58000.0f; state.radarAlt = 58000.0f;
  state.apoapsis = 95000.0f; state.periapsis = 28000.0f;
  state.orbitalVel = 2450.0f; state.surfaceVel = 2280.0f; state.verticalVel = -420.0f;
  state.machNumber = 6.9f; state.gForce = 1.8f; state.throttle = 0.0f;
  state.heading = 270.0f; state.pitch = 11.0f; state.roll = 0.0f;                      // retrograde
  state.srfVelHeading = 90.0f; state.srfVelPitch = -11.0f; state.orbVelHeading = 90.0f; state.orbVelPitch = -9.0f;
  state.sasMode = 2; state.coreTempPct = 45; state.skinTempPct = 72;
  state.stageDeltaV = 0.0f; state.totalDeltaV = 0.0f; state.stageBurnTime = 0.0f;
  state.timeToAp = 0.0f; state.timeToPe = 210.0f;
}

// Booster 18.5 km up in the gravity turn, throttle wide open.
static void kcmAscent() {
  kcmBaseShip();
  state.situation = sit_Flying; state.inAtmo = true; state.airDensity = 0.12f;
  state.altitude = 18500.0f; state.radarAlt = 18400.0f;
  state.apoapsis = 46000.0f; state.periapsis = -580000.0f;
  state.timeToAp = 62.0f; state.timeToPe = 800.0f;
  state.orbitalVel = 690.0f; state.surfaceVel = 520.0f; state.verticalVel = 310.0f;
  state.throttle = 1.0f; state.machNumber = 1.8f; state.gForce = 2.1f;
  state.heading = 90.0f; state.pitch = 62.0f; state.roll = 0.0f;
  state.srfVelHeading = 90.0f; state.srfVelPitch = 60.0f; state.orbVelHeading = 90.0f; state.orbVelPitch = 40.0f;
  state.stageDeltaV = 1650.0f; state.totalDeltaV = 3100.0f; state.stageBurnTime = 95.0f;
}

static void kcmSetLnchPrelaunch(bool pre) { _lnchPrelaunchMode = pre; _lnchPrelaunchDismissed = !pre; _lnchCoastLatched = false; }

struct KcmScenario { const char *name; ScreenType s; void (*setup)(); };
static void scSCFT()   { kcmLKO(); }
static void scACFT()   { kcmPlane(); }
static void scROVR()   {
  kcmRover();
  // Feed the endurance sampler a minute of discharge so ENDUR has an answer.
  for (int i = 0; i < 60; i++) { stubMillis() += 1000; state.electricChargePercent -= 0.02f; rovrEnduranceService(); }
}
static void scVEH()    { kcmLKO(); }
static void scLNCHPRE(){ kcmBaseShip(); state.situation = sit_PreLaunch; state.inAtmo = true; state.airDensity = 1.22f;
                         state.altitude = 72.0f; state.radarAlt = 0.0f; state.throttle = 0.0f; state.stageDeltaV = 1650.0f;
                         state.totalDeltaV = 3100.0f; state.stageBurnTime = 128.0f; state.heading = 90.0f; state.pitch = 90.0f;
                         state.electricChargePercent = 100.0f; state.gear_on = true; kcmSetLnchPrelaunch(true); }
static void scLNCH()   { kcmAscent(); kcmSetLnchPrelaunch(false); }
static void scLNCHCIRC(){ kcmAscent(); kcmSetLnchPrelaunch(false);
                         // Coasting to apoapsis with a circularisation node placed there. Suborbital
                         // (Pe -300 km); vis-viva gives 497 m/s to circularise at the 82.5 km Ap.
                         // Ignition is 29 s out (node 55 s, burn 52 s), nose 3 deg above the node.
                         state.inAtmo = false; state.airDensity = 0.0f; state.altitude = 80000.0f; state.radarAlt = 80000.0f;
                         state.apoapsis = 82500.0f; state.periapsis = -300000.0f; state.timeToAp = 55.0f; state.timeToPe = 700.0f;
                         state.semiMajorAxis = 491250.0f; state.eccentricity = 0.389f;
                         state.orbitalVel = 1788.0f; state.surfaceVel = 1610.0f; state.verticalVel = 60.0f;
                         state.throttle = 0.0f; state.machNumber = 0.0f; state.gForce = 0.0f;
                         state.heading = 90.0f; state.pitch = 3.0f; state.srfVelPitch = 2.0f; state.orbVelPitch = 1.9f;
                         state.mnvrTime = 55.0f; state.mnvrDeltaV = 497.0f; state.mnvrTotalDeltaV = 497.0f;
                         state.mnvrDuration = 52.0f; state.mnvrHeading = 90.0f; state.mnvrPitch = 0.0f;
                         state.sasMode = 9;                                          // MANEUVER
                         state.stageDeltaV = 940.0f; state.stageBurnTime = 85.0f; _lnchCoastLatched = true; }
static void scORB()    { kcmLKO(); }
static void scORBADV() { kcmLKO(); }
static void scMNVR()   { kcmLKO(); state.mnvrTime = 185.0f; state.mnvrDeltaV = 842.0f; state.mnvrTotalDeltaV = 842.0f;
                         state.mnvrDuration = 48.0f; state.mnvrHeading = 92.0f; state.mnvrPitch = 1.0f; state.sasMode = 9; } // MANEUVER
static void scTGT()    { kcmLKO(); state.targetAvailable = true; state.tgtDistance = 1250.0f; state.tgtVelocity = -4.2f;
                         state.tgtHeading = 93.0f; state.tgtPitch = 3.0f; state.tgtVelHeading = 91.0f; state.tgtVelPitch = 1.0f;
                         state.sasMode = 7; }                                                                          // TARGET
static void scDOCK()   { kcmLKO(); state.targetAvailable = true; state.tgtDistance = 42.0f; state.tgtVelocity = -0.4f;
                         state.tgtHeading = 90.5f; state.tgtPitch = 0.8f; state.tgtVelHeading = 90.2f; state.tgtVelPitch = 0.3f;
                         state.sasMode = 7; state.rcs_on = true; state.pitch = 0.0f; }
static void scNAV()    { kcmPlane(); state.targetAvailable = true; state.tgtDistance = 18500.0f; state.tgtVelocity = -205.0f;
                         state.tgtHeading = 284.0f; state.tgtPitch = -1.0f; }
static void scLNDG()   { kcmMunDescent(); }
static void scLNDGRE() { kcmReentry(); }
static void scLNCHAP() { kcmAscent(); state.apArmed = true; state.apPhase = 2; state.apTargetAlt = 80000.0f;
                         state.apInclination = 0.0f; state.apSoutherly = false; state.apLoft = 1.0f; state.apRollEnable = false;
                         state.apMaxG = 4.0f; state.apCmdPitch = 62.0f; state.apCmdHeading = 90.0f; state.apCmdThrottle = 1.0f;
                         state.apDynPressure = 16200.0f; state.hpFlags = 0x10; }
static void scORBTAP() { kcmLKO(); state.mnvrDeltaV = 842.0f; state.mnvrTotalDeltaV = 842.0f; state.mnvrTime = 185.0f;
                         state.mnvrDuration = 48.0f; state.obMode = 1; state.obPhase = 1;
                         state.obFlags = 0x01 | 0x04 | 0x08 | 0x20; state.obTargetAp = 250000.0f; state.obTargetPe = 80000.0f;
                         state.obTargetInc = 0.0f; state.obApprRate = -2.0f; state.obApprDist = 50.0f;
                         state.obDvTotal = 842.0f; state.obDvRemaining = 842.0f; state.obTIgnition = 185.0f;
                         state.obBurnDuration = 48.0f; state.obAccelEst = 8.6f; state.obCmdThrottle = 0.0f; }
static void scLNDGAP() { kcmMunDescent(); state.ldMode = 1; state.ldFlags = 0x01 | 0x08 | 0x10 | 0x80; state.sasMode = 255; state.ldDescRate = -4.0f;   // DESC, ATT REF VERT, vertical hold flying (SAS off)
                         state.ldHovrAlt = 50.0f; state.ldTwr = 0.0f; state.ldMargin = 150.0f; state.ldEntryAoa = 8.0f;
                         state.ldEntryRoll = 0.0f; state.ldAccelSource = 1; state.ldAccelEst = 14.2f; state.ldIgnAlt = 2235.0f;
                         state.ldCmdThrottle = 0.62f; }
static void scACFTAP() { kcmPlane(); state.roll = 0.0f; state.verticalVel = 0.0f; state.srfVelHeading = 270.0f;
                         state.hpPitchMode = 4; state.hpLatMode = 2; state.hpThrMode = 1; state.hpFlags = 0x01 | 0x02 | 0x08;
                         state.hpAtt = 5.0f; state.hpAoa = 3.0f; state.hpVs = 0.0f; state.hpAlt = 4500.0f; state.hpRoll = 0.0f;
                         state.hpHdg = 270.0f; state.hpIas = 190.0f; state.hpMach = 0.62f; state.hpGs = 3.0f; state.hpCmdThrottle = 0.75f; }
static void scROVRAP() { kcmRover(); state.rvFlags = 0x01 | 0x02; state.rvCruise = 12.0f; state.rvHdg = 45.0f;
                         state.rvMaxSpeed = 20.0f; state.rvMaxSlope = 20.0f; state.rvMaxRoll = 25.0f;
                         state.rvFollowRange = 30.0f; state.rvStopDist = 15.0f; state.rvCmdWheel = 0.6f; }

static const KcmScenario kcmScenarios[] = {
  { "SCFT", screen_SCFT, scSCFT }, { "ACFT", screen_ACFT, scACFT }, { "ROVR", screen_ROVR, scROVR }, { "VEH", screen_VEH, scVEH },
  { "LNCHPRE", screen_LNCH, scLNCHPRE }, { "LNCH", screen_LNCH, scLNCH }, { "LNCHCIRC", screen_LNCH, scLNCHCIRC },
  { "ORB", screen_ORB, scORB }, { "ORBADV", screen_ORBADV, scORBADV }, { "MNVR", screen_MNVR, scMNVR },
  { "TGT", screen_TGT, scTGT }, { "DOCK", screen_DOCK, scDOCK }, { "NAV", screen_NAV, scNAV },
  { "LNDG", screen_LNDG, scLNDG }, { "LNDGRE", screen_LNDGRE, scLNDGRE },
  { "LNCHAP", screen_LNCHAP, scLNCHAP }, { "ORBTAP", screen_ORBTAP, scORBTAP }, { "LNDGAP", screen_LNDGAP, scLNDGAP },
  { "ACFTAP", screen_ACFTAP, scACFTAP }, { "ROVRAP", screen_ROVRAP, scROVRAP },
};

static void kcmPaint(ScreenType want) {
  switchToScreen(want);
  for (int i = 0; i < 3; i++) loop();     // chrome frame + incremental frames, millis frozen
}

int main(int argc, char **argv) {
  const char *scenario = argc > 1 ? argv[1] : "SCFT";
  const char *outPath  = argc > 2 ? argv[2] : "screen.ppm";
  stubMillis() = 60000;
  debugMode = false; fpsDiag = false;
  infoDB.begin(infoDisp);
  setupSD();

  String sc = scenario;
  if (sc == "Standby") {
    demoMode = false; flightScene = false;
    drawStandbyScreen(infoDisp);
  } else if (sc == "INPUT") {
    demoMode = false; flightScene = true;
    uint8_t f0[40] = { 0xAB, 0 };
    uint16_t present = 0x1FFF & ~(1u << 10), fault = 1u << 5;   // PRE-WARP missing; TIME reports a FAULT
    f0[2] = (uint8_t)present; f0[3] = (uint8_t)(present >> 8);
    f0[4] = (uint8_t)fault;   f0[5] = (uint8_t)(fault >> 8);
    f0[6] = f0[2]; f0[7] = f0[3];
    f0[8] = 0x04;                                   // UI B2
    f0[10] = 0x00; f0[11] = 0x00; f0[12] = 0x67;    // FUNCTION switches: MSTR..: SCE, UPTLM, A/P, ENGINE, THRTL
    f0[13] = 0x00; f0[14] = 0x02;                   // ACTION B9
    f0[15] = 0x00; f0[16] = 0x04;                   // STABILITY B10 (HOLD)
    f0[17] = 0x01; f0[18] = 0x00; f0[19] = 0x3E;    // VEHICLE BRAKES; Switch Group 2
    f0[20] = 0x00; f0[21] = 0x10;                   // TIME B12 (selector SPC)
    f0[24] = 0x02; f0[25] = 35;  f0[26] = (uint8_t)-60; f0[27] = 12;   // rotation: BTN01, deflected
    f0[28] = 0x00; f0[29] = 0;   f0[30] = 0;   f0[31] = (uint8_t)-25;
    f0[32] = 0x15;                                  // GPWS: state 1, BTN01 just pressed
    f0[34] = 62;  f0[35] = 0x01 | 0x04 | 0x20;      // lever 62 %, touched, active, UP just pressed
    f0[36] = 0x30; f0[37] = (uint8_t)-4;            // control group 3, ENC2 -4
    f0[38] = 2;   f0[39] = 0x00;                    // SPC, safe lines open
    uint8_t f1[24] = { 0xAB, 1, 3, 7, 0, 17, 15, 14, 18, 19, 6, 5, 13, 2, 22, 0x2C, 0x01, 0, 0 };
    f1[2 + 10] = 0xFF;                              // PRE-WARP: no line
    uint32_t life = (2u << (2 * 12)) | (1u << (2 * 8));   // DUAL ENC DISABLED, TRANSLATION SLEEPING, rest ACTIVE
    for (int i = 0; i < 4; i++) f1[19 + i] = (uint8_t)(life >> (8 * i));
    uint8_t f2[24] = { 0xAB, 2, 0x0F };             // all four displays answering
    f2[3] = 0x03; f2[4] = 0x21; f2[5] = 0x00;       // Annunciator: link, flight; two C&W lit
    f2[8] = 0x03; f2[9] = 18;                       // Info 1: link, flight, on this screen
    f2[10] = 0x03; f2[11] = 12;                     // Info 2: link, flight, ascent console
    f2[12] = 0x0B; f2[13] = 9;                      // Resource: link, flight, caution, 9 bars
    inputTestTouch(2, 900, 60, 980, 110); stubMillis() += 50;
    inputTestTouch(2, 700, 260, 780, 310);          // the gesture opens it
    inputTestFrame(f0, 40); inputTestFrame(f1, 24); inputTestFrame(f2, 24);
    inputTestService(infoDisp);
  } else {
    const KcmScenario *pick = nullptr;
    for (const KcmScenario &k : kcmScenarios) if (sc == k.name) pick = &k;
    if (!pick) { fprintf(stderr, "unknown scenario '%s'\n", scenario); return 2; }

    // Populate every field once through the panel's own demo, then freeze the demo
    // (millis() no longer advances past its 100 ms step) and lay the scenario over it.
    demoMode = true; flightScene = true;
    initDemoMode();
    stubMillis() += 100; stepDemoState();
    demoMode = false;                        // loop() runs Simpit (a no-op stub) instead
    pick->setup();

    // Let the ladder choose if it agrees (AUTO chip); otherwise pin the screen (MAN).
    clearManualScreenLatch();
    kcmPaint(pick->s);
    if (activeScreen != pick->s) {
      switchToScreen(pick->s);
      setManualScreenLatch();
      kcmPaint(pick->s);
    }
    if (activeScreen != pick->s) { fprintf(stderr, "%s: ladder moved the screen to %d\n", scenario, (int)activeScreen); return 3; }
  }
  if (!infoDisp.writePPM(outPath)) { fprintf(stderr, "cannot write %s\n", outPath); return 1; }
  fprintf(stderr, "%s: unit %d screen %d %s", scenario, INFO_DISP_UNIT, (int)activeScreen,
          _manualScreenLatch ? "MAN" : "AUTO");
  return 0;
}
