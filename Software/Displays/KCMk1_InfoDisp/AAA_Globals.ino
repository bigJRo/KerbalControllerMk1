/***************************************************************************************
   AAA_Globals.ino -- Variable definitions for Kerbal Controller Mk1 Information Display
   All global variable instances are owned here.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


/***************************************************************************************
   DISPLAY AND TOUCH
****************************************************************************************/
KCM_TFT     infoDisp  = KCM_TFT(KCM_TFT_RS, KCM_TFT_CS, KCM_TFT_RESET);
TouchResult lastTouch;
KCMDoubleBuffer infoDB;


/***************************************************************************************
   SIMPIT OBJECT
   Moved here from SimpitHandler.ino (#9) — owned in globals alongside other
   shared objects. SerialUSB1 is the second USB COM port, dedicated to KSP telemetry.
****************************************************************************************/
KerbalSimpit simpit(SerialUSB1);


/***************************************************************************************
   SCREEN STATE
****************************************************************************************/
ScreenType activeScreen = screen_LNCH;
ScreenType prevScreen   = screen_COUNT;  // sentinel -- forces chrome on first loop


/***************************************************************************************
   SCREEN TRANSITION TIMESTAMP  (#8)
****************************************************************************************/
uint32_t lastScreenSwitch = 0;


/***************************************************************************************
   FLIGHT STATE
****************************************************************************************/
bool simpitConnected = false;  // true after Simpit handshake succeeds
bool _pendingContextSwitch = false;  // set on vessel change; cleared when FLIGHT_STATUS arrives
bool _pendingDockCheck     = false;  // set after context switch; cleared when TARGETINFO arrives
bool flightScene     = false;  // true when KSP is in a flight scene
bool idleState       = false;  // true when master wants standby when not in flight


/***************************************************************************************
   CELESTIAL BODY (derived from gameSOI — not stored in AppState)
   Updated whenever state.gameSOI changes. Access radius via currentBody.radius.
   Initialised to Kerbin so screens work before first Simpit or demo SOI update.
****************************************************************************************/
BodyParams currentBody;


/***************************************************************************************
   APPLICATION STATE
****************************************************************************************/
AppState state;


/***************************************************************************************
   SWITCH TO SCREEN
   Sets activeScreen and forces a full chrome redraw on the next loop pass by
   resetting prevScreen to the sentinel value screen_COUNT.
   Always use this function — never set activeScreen directly.
****************************************************************************************/
void switchToScreen(ScreenType s) {
  // ORB (Apsides) and ORBADV (Advanced Elements) are now separate sidebar screens;
  // _orbAdvancedMode is derived from the target screen in drawStaticScreen(), so no
  // navigation-time reset is needed here.
  activeScreen     = s;
  prevScreen       = screen_COUNT;
  lastScreenSwitch = millis();   // #8 record timestamp for touch debounce and diagnostics
}


/***************************************************************************************
   CONTEXT SCREEN SELECTION
   Returns the most operationally relevant screen for the current vessel state.
   Called on VESSEL_CHANGE_MESSAGE and on entering a flight scene.

   Priority (highest to lowest):
     1. Plane type                             → ACFT
     2. Pre-launch or Landed situation         → LNCH  (any vessel type on the ground)
     3. Lander type (airborne or sub-orbital)  → LNDG (powered descent)
     4. Anything else (orbit, flight…)         → APSI
****************************************************************************************/
ScreenType contextScreen() {
  // 1. Plane always goes to aircraft screen regardless of situation
  if (state.vesselType == type_Plane)
    return screen_ACFT;

  // 1b. Rover always goes to rover screen regardless of situation
  if (state.vesselType == type_Rover)
    return screen_ROVR;

  // 2. Any vessel on the ground → launch screen
  if ((state.situation & sit_PreLaunch) || (state.situation & sit_Landed))
    return screen_LNCH;

  // 3. Lander type in flight → powered descent
  if (state.vesselType == type_Lander) {
    _lndgReentryMode = false;
    return screen_LNDG;
  }

  // 4. Target within docking range → docking screen
  // 4. Target within docking range → docking screen
  // Use tgtDistance alone — KSP may report targetAvailable=false even while
  // actively sending TARGETINFO with a valid distance (observed in KSP1).
  if (state.tgtDistance > 0.0f && state.tgtDistance <= DOCK_DIST_WARN_M)
    return screen_DOCK;

  // 5. Recoverable vessel (debris, probe, etc. that can be recovered) → Vehicle Info
  if (state.isRecoverable)
    return screen_VEH;

  // 6. Everything else (orbit, sub-orbital flight, splashed, unknown)
  return screen_ORB;
}


/***************************************************************************************
   STANDBY SCREEN
   Shown when not in a flight scene (menus, tracking station, etc.).
   Displays the shared splash BMP used by all KCMk1 panels.
****************************************************************************************/
void drawStandbyScreen(KCM_TFT &tft) {
  // Present the full-screen splash through the double buffer: draw it to the
  // hidden page, then flip. During standby the loop returns early (no further
  // flips), so this held image stays on screen.
  infoDB.beginFrame(tft);
  drawStandbySplash(tft);   // #5A delegates to KDC library (subsumes #11 setXY fix)
  infoDB.flip(tft);
}


/***************************************************************************************
   TIME-TO-GROUND ESTIMATION
   Shared by the powered-descent and re-entry landing screens. Replaces the naive
   radarAlt/|verticalVel| estimate with a regime-aware calculation:
     - coasting in vacuum / thin atmosphere -> Keplerian time to the ground radius
     - under thrust or in dense atmosphere  -> kinematic projection using the
       measured vertical acceleration (captures thrust braking and drag)
     - degenerate / near-surface            -> naive constant-velocity fallback
   Returns seconds to ground, or -1.0 when not applicable (caller shows "---").
****************************************************************************************/

// Low-pass-filtered vertical acceleration (m/s^2). Advanced once per frame; call
// only from estimateTimeToGround() so the sample interval stays ~one frame.
static float _ttgUpdateAccel() {
  static float    prevVv = 0.0f;
  static uint32_t prevMs = 0;
  static float    accel  = 0.0f;
  uint32_t now = millis();
  if (prevMs != 0) {
    float dt = (float)(now - prevMs) / 1000.0f;
    if (dt > 0.01f && dt < 1.0f)
      accel += 0.30f * ((state.verticalVel - prevVv) / dt - accel);   // ~3-sample smoothing
  }
  prevVv = state.verticalVel;
  prevMs = now;
  return accel;
}

// Smallest positive root of  0.5*a*t^2 + v*t + h = 0  (h>0 AGL, v<0 descending).
// Returns -1 if the vessel would arrest its descent before reaching the ground.
static float _ttgKinematic(float h, float v, float a) {
  if (fabsf(a) < 0.05f) return (v < 0.0f) ? (h / -v) : -1.0f;   // ~constant velocity
  float disc = v * v - 2.0f * a * h;
  if (disc < 0.0f) return -1.0f;
  float sq = sqrtf(disc);
  float t1 = (-v - sq) / a, t2 = (-v + sq) / a, t = -1.0f;
  if (t1 > 0.0f)                          t = t1;
  if (t2 > 0.0f && (t < 0.0f || t2 < t))  t = t2;
  return t;
}

// Keplerian time from the current (descending) point to the ground radius, from
// the received orbit elements. Valid for elliptical coasting arcs (0 <= e < 1).
// Returns -1 if periapsis is above the ground (won't impact) or elements unusable.
static float _ttgKeplerian() {
  float e = state.eccentricity, a = state.semiMajorAxis, period = state.orbitalPeriod;
  float Rb = currentBody.radius;
  if (a <= 0.0f || period <= 0.0f || Rb <= 0.0f || e < 0.0f || e >= 1.0f) return -1.0f;

  float r_g    = Rb + (state.altitude - state.radarAlt);   // ground radius beneath us
  float r_peri = a * (1.0f - e);
  if (r_peri > r_g) return -1.0f;                          // periapsis above ground

  float p      = a * (1.0f - e * e);
  float cosNu  = constrain((p / r_g - 1.0f) / e, -1.0f, 1.0f);
  float nu     = acosf(cosNu);                             // [0, PI]
  float Ecc    = 2.0f * atan2f(sqrtf(1.0f - e) * sinf(nu * 0.5f),
                               sqrtf(1.0f + e) * cosf(nu * 0.5f));
  float M      = Ecc - e * sinf(Ecc);                      // mean anomaly from periapsis
  float dtToPe = M / (TWO_PI / period);                    // r_g crossing -> periapsis
  float ttg    = state.timeToPe - dtToPe;                  // now -> r_g crossing
  return (ttg > 0.0f) ? ttg : -1.0f;
}

float estimateTimeToGround() {
  float aVert = _ttgUpdateAccel();   // advance the accel tracker every frame

  bool inOrbitOrEscape = (state.situation == sit_Orbit || state.situation == sit_Escaping);
  if (inOrbitOrEscape || state.radarAlt <= 0.0f || state.verticalVel >= -0.05f)
    return -1.0f;

  bool powered = (state.throttle > 0.02f);
  bool draggy  = (state.inAtmo && state.airDensity > 0.05f);
  float t = (powered || draggy) ? _ttgKinematic(state.radarAlt, state.verticalVel, aVert)
                                : _ttgKeplerian();
  if (t >= 0.0f) return t;
  return fabsf(state.radarAlt / state.verticalVel);   // naive fallback
}
