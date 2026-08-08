/***************************************************************************************
   Demo.ino -- Demo/test mode for Kerbal Controller Mk1 Annunciator
   Drives all AppState fields independently at configurable rates, simulating the
   telemetry Simpit would normally deliver. Display logic runs identically in demo
   and production since both write to the same state variables.
   Set demoMode = true in AAA_Config.ino to enable, or send it via I2C from the master.
****************************************************************************************/
#include "KCMk1_Annunciator.h"



/***************************************************************************************
   DEMO DATA
****************************************************************************************/
static const char *demoVesselNames[] = {
  "TEST CRAFT NAME",
  "OV-102 COLUMBIA",
  "OV-099 CHALLENGER",
  "OV-103 DISCOVERY",
  "OV-104 ATLANTIS",
  "OV-105 ENDEAVOUR",
  "OV-101 ENTERPRISE"
};
static const uint8_t demoVesselCount = sizeof(demoVesselNames) / sizeof(demoVesselNames[0]);
static uint8_t demoVesselIndex = 0;

static const char *demoBodyList[] = {
  "Kerbol", "Moho", "Eve", "Gilly", "Kerbin", "Mun", "Minmus",
  "Duna", "Ike", "Dres", "Jool", "Laythe", "Vall", "Tylo", "Bop", "Pol", "Eeloo"
};
static const uint8_t demoBodyCount = sizeof(demoBodyList) / sizeof(demoBodyList[0]);
static uint8_t demoBodyIndex = 255;  // wraps to 0 on first step


/***************************************************************************************
   DEMO UPDATE INTERVALS (ms)
****************************************************************************************/
static const uint32_t DEMO_RATE_VESSEL    = 8000;
static const uint32_t DEMO_RATE_SITUATION = 500;
static const uint32_t DEMO_RATE_TEMP      = 750;
static const uint32_t DEMO_RATE_CREW      = 3000;
static const uint32_t DEMO_RATE_TW        = 1000;
static const uint32_t DEMO_RATE_COMMS     = 500;
static const uint32_t DEMO_RATE_STAGE     = 4000;
static const uint32_t DEMO_RATE_BODY      = 5000;
static const uint32_t DEMO_RATE_ALT       = 200;
static const uint32_t DEMO_RATE_VEL       = 200;
static const uint32_t DEMO_RATE_GFORCE    = 300;
static const uint32_t DEMO_RATE_EC        = 1000;
static const uint32_t DEMO_RATE_DV        = 2000;
static const uint32_t DEMO_RATE_THROTTLE  = 250;
static const uint32_t DEMO_RATE_GEAR      = 4000;
static const uint32_t DEMO_RATE_ATMO      = 6000;
static const uint32_t DEMO_RATE_DOCKED    = 4500;   // toggle DOCKED indicator
static const uint32_t DEMO_RATE_MODEFLAGS = 700;    // walk the bottom mode grid
static const uint32_t DEMO_RATE_FLAGS     = 1500;   // exercise flag/resource C&W tiles


/***************************************************************************************
   DEMO LAST-UPDATE TIMESTAMPS
****************************************************************************************/
static uint32_t demoLast_vessel    = 0;
static uint32_t demoLast_situation = 0;
static uint32_t demoLast_temp      = 0;
static uint32_t demoLast_crew      = 0;
static uint32_t demoLast_tw        = 0;
static uint32_t demoLast_comms     = 0;
static uint32_t demoLast_stage     = 0;
static uint32_t demoLast_body      = 0;
static uint32_t demoLast_alt       = 0;
static uint32_t demoLast_vel       = 0;
static uint32_t demoLast_gforce    = 0;
static uint32_t demoLast_ec        = 0;
static uint32_t demoLast_dv        = 0;
static uint32_t demoLast_throttle  = 0;
static uint32_t demoLast_gear      = 0;
static uint32_t demoLast_atmo      = 0;
static uint32_t demoLast_docked    = 0;
static uint32_t demoLast_modeflags = 0;
static uint32_t demoLast_flags     = 0;


/***************************************************************************************
   DEMO SWEEP COUNTERS
****************************************************************************************/
static float   demoAlt      = 0.0f;
static float   demoVelVert  = 50.0f;
static float   demoEC       = 1000.0f;
static float   demoDV       = 2000.0f;
static uint8_t demoThrottle = 0;
static uint8_t demoSitIdx   = 0;
static uint8_t demoModeBit  = 0;
static uint8_t demoFlagPhase = 0;


/***************************************************************************************
   LAMP TEST (#7)
   Graphics check independent of C&W logic: force every state-driven tile ON so all
   graphics can be verified regardless of whether the telemetry logic would trigger
   them. Enable with `lampTest = true` (and demoMode = true) in AAA_Config.ino.
   Note: the inner regime column still shows one tile (driven by flightCondIndex);
   the C&W grid, situation column, mode grid, DOCK and MASTER ALARM all light.
****************************************************************************************/
static void runLampTest() {
  state.cautionWarningState  = ((uint32_t)1 << CW_COUNT) - 1;   // all C&W bits
  state.vesselSituationState = 0xFF;                            // all situation flags
  state.modeFlags            = (uint16_t)(((uint32_t)1 << MF_COUNT) - 1); // all mode tiles
  chuteEnvState              = chute_Green;                     // CHUTE ENV (green)
  state.masterAlarmOn        = true;
}


/***************************************************************************************
   STEP DEMO STATE
   Advances each field at its own rate. Calls updateCautionWarningState() at end
   so C&W panel and master alarm reflect the demo values exactly as in production.
****************************************************************************************/
void stepDemoState() {
  if (lampTest) { runLampTest(); return; }   // graphics-only check (#7)
  uint32_t now = millis();

  // Vessel name and type
  if (now - demoLast_vessel >= DEMO_RATE_VESSEL) {
    demoLast_vessel = now;
    demoVesselIndex = (demoVesselIndex + 1) % demoVesselCount;
    state.vesselName = demoVesselNames[demoVesselIndex];
    state.vesselType = (VesselType)((state.vesselType + 1) % 17);
  }

  // Vessel situation -- cycle through meaningful states
  if (now - demoLast_situation >= DEMO_RATE_SITUATION) {
    demoLast_situation = now;
    static const uint8_t sitSeq[] = {
      (1 << VSIT_PRELAUNCH),
      (1 << VSIT_FLIGHT),
      (1 << VSIT_SUBORBIT),
      (1 << VSIT_ORBIT),
      (1 << VSIT_ESCAPE),
      (1 << VSIT_SPLASH),
      (1 << VSIT_LANDED),   // (#7) was missing — LANDED (and CONTACT) never lit
    };
    static const uint8_t sitSeqLen = sizeof(sitSeq) / sizeof(sitSeq[0]);
    demoSitIdx = (demoSitIdx + 1) % sitSeqLen;
    uint8_t dockedBit = state.vesselSituationState & 0x01;
    state.vesselSituationState = dockedBit | sitSeq[demoSitIdx];
    inFlight = (sitSeq[demoSitIdx] & ((1<<VSIT_FLIGHT)|(1<<VSIT_SUBORBIT)|(1<<VSIT_ORBIT)|(1<<VSIT_ESCAPE))) != 0;
  }

  // Temperatures
  if (now - demoLast_temp >= DEMO_RATE_TEMP) {
    demoLast_temp = now;
    state.maxTemp  = (state.maxTemp  + 7) % 101;
    state.skinTemp = (state.skinTemp + 5) % 101;
  }

  // Crew count
  if (now - demoLast_crew >= DEMO_RATE_CREW) {
    demoLast_crew = now;
    state.crewCount = (state.crewCount + 1) % 8;
  }

  // Time warp
  if (now - demoLast_tw >= DEMO_RATE_TW) {
    demoLast_tw = now;
    state.twIndex++;
    if (state.twIndex > 8) { physTW = !physTW; state.twIndex = 0; }
  }

  // Comms strength
  if (now - demoLast_comms >= DEMO_RATE_COMMS) {
    demoLast_comms = now;
    state.commNet = (state.commNet + 3) % 101;
  }

  // Stage
  if (now - demoLast_stage >= DEMO_RATE_STAGE) {
    demoLast_stage = now;
    state.stage = (state.stage + 1) % 20;
  }

  // SOI / body
  if (now - demoLast_body >= DEMO_RATE_BODY) {
    demoLast_body = now;
    demoBodyIndex = (demoBodyIndex + 1) % demoBodyCount;
    state.gameSOI = demoBodyList[demoBodyIndex];
    currentBody   = getBodyParams(state.gameSOI);
  }

  // Altitude -- sweep 0->800km then descend through GROUND PROX zone
  if (now - demoLast_alt >= DEMO_RATE_ALT) {
    demoLast_alt = now;
    if (demoAlt >= 0.0f) {
      demoAlt += 5000.0f;
      if (demoAlt > 800000.0f) demoAlt = -300.0f;
    } else {
      demoAlt += 30.0f;
      if (demoAlt >= 0.0f) demoAlt = 0.0f;
    }
    float surfAlt  = max(demoAlt, 0.0f);
    state.alt_sl   = surfAlt;
    state.alt_surf = surfAlt;
    state.apoapsis = surfAlt * 1.2f;
    if (demoAlt < 0.0f) state.vel_vert = -35.0f;
  }

  // Velocity
  if (now - demoLast_vel >= DEMO_RATE_VEL) {
    demoLast_vel  = now;
    demoVelVert   = sinf(now * 0.001f) * 200.0f;
    if (demoAlt >= 0.0f) state.vel_vert = demoVelVert;
    state.vel_surf = max(demoAlt, 0.0f) * 0.001f + 50.0f;
  }

  // G-forces
  if (now - demoLast_gforce >= DEMO_RATE_GFORCE) {
    demoLast_gforce = now;
    state.gForces   = sinf(now * 0.0007f) * 9.0f + 3.0f;
  }

  // Electric charge
  if (now - demoLast_ec >= DEMO_RATE_EC) {
    demoLast_ec = now;
    demoEC -= 25.0f;
    if (demoEC < 0.0f) demoEC = 1000.0f;
    state.EC       = demoEC;
    state.EC_total = 1000.0f;
  }

  // Delta-V and burn time
  if (now - demoLast_dv >= DEMO_RATE_DV) {
    demoLast_dv = now;
    demoDV -= 50.0f;
    if (demoDV < 0.0f) demoDV = 2000.0f;
    state.stageDV       = demoDV;
    state.stageBurnTime = demoDV / 20.0f;
  }

  // Throttle
  if (now - demoLast_throttle >= DEMO_RATE_THROTTLE) {
    demoLast_throttle = now;
    demoThrottle = (demoThrottle + 5) % 101;
    state.throttleCmd = demoThrottle;
  }

  // Gear
  if (now - demoLast_gear >= DEMO_RATE_GEAR) {
    demoLast_gear = now;
    state.gear_on = !state.gear_on;
  }

  // Atmosphere flags
  if (now - demoLast_atmo >= DEMO_RATE_ATMO) {
    demoLast_atmo = now;
    static uint8_t atmoPhase = 0;
    atmoPhase = (atmoPhase + 1) % 4;
    inAtmo = (atmoPhase == 1 || atmoPhase == 2);
    hasO2  = (atmoPhase == 2);
  }

  // DOCKED indicator toggle (#7 — was never driven in demo)
  if (now - demoLast_docked >= DEMO_RATE_DOCKED) {
    demoLast_docked = now;
    state.vesselSituationState ^= (1 << VSIT_DOCKED);
  }

  // Bottom mode grid: walk a single bit through all MF_* tiles, then all-on,
  // then all-off, so every mode tile lights for inspection (#8 / #9).
  if (now - demoLast_modeflags >= DEMO_RATE_MODEFLAGS) {
    demoLast_modeflags = now;
    demoModeBit = (demoModeBit + 1) % (MF_COUNT + 2);
    uint16_t walk = (demoModeBit < MF_COUNT)  ? (1u << demoModeBit)      // one at a time
                  : (demoModeBit == MF_COUNT) ? ((1u << MF_COUNT) - 1)   // all on
                                              : 0u;                      // all off
    // DEMO / AUDIO / DEBUG tiles reflect the actual panel mode (#8); the rest
    // walk so every tile can be inspected.
    uint16_t base = 0;
    if (demoMode)     base |= (1u << MF_DEMO);
    if (audioEnabled) base |= (1u << MF_AUDIO);
    if (debugMode)    base |= (1u << MF_DEBUG);
    state.modeFlags = base | walk;
  }

  // Exercise the flag/resource-driven C&W tiles so each lights at some point (#9):
  // ABORT, EVA ACTIVE, SRB ACTIVE, RCS LOW, PROP LOW/RATIO.
  if (now - demoLast_flags >= DEMO_RATE_FLAGS) {
    demoLast_flags = now;
    demoFlagPhase = (demoFlagPhase + 1) % 6;
    state.abort_on = (demoFlagPhase == 1);
    inEVA          = (demoFlagPhase == 2);
    // SRB ACTIVE requires a DECREASING solid-fuel reading — sweep it down so the
    // "burning" test passes rather than toggling on/off.
    static float demoSF = 100.0f;
    demoSF -= 12.0f;
    if (demoSF < 0.0f) demoSF = 100.0f;
    state.SF_stage     = demoSF;
    state.SF_stage_tot = 100.0f;
    state.mono     = (demoFlagPhase == 4) ? 5.0f : 90.0f;       // RCS LOW (<15-20%)
    state.mono_tot = 100.0f;
    if (demoFlagPhase == 5) { state.LF_stage = 3.0f;  state.OX_stage = 20.0f; } // PROP LOW + ratio off
    else                    { state.LF_stage = 45.0f; state.OX_stage = 55.0f; }
    state.LF_stage_tot = 100.0f;
    state.OX_stage_tot = 100.0f;
  }

  updateCautionWarningState();
}


/***************************************************************************************
   INIT DEMO MODE
   Called from setup() when demoMode is true. Sets the initial state for the demo
   and starts on the main screen with flightScene active.
   Note: activeScreen and prevScreen are set directly here rather than via
   switchToScreen() because invalidateAllState() is called explicitly at the end,
   making a switchToScreen() call redundant at this point. switchToScreen() is
   safe to call after initDemoMode() returns (Phase 2B wires this up).
****************************************************************************************/
void initDemoMode() {
  flightScene = true;
  activeScreen  = screen_Main;
  prevScreen    = screen_COUNT;
  demoBodyIndex = 0;
  state.gameSOI = demoBodyList[demoBodyIndex];
  currentBody   = getBodyParams(state.gameSOI);
  demoVesselIndex  = 0;
  state.vesselName = demoVesselNames[demoVesselIndex];
  invalidateAllState();
  // Enter the main screen explicitly (matches the pattern of the other panels).
  switchToScreen(screen_Main);
}
