#pragma once
/***************************************************************************************
   KCMk1_InfoDisp.h -- Shared declarations for Kerbal Controller Mk1 Information Display
   Included by every .ino tab. Defines types, enums, structs, and extern declarations.
****************************************************************************************/

#include <KerbalDisplayCommon.h>   // pulls in KCM_Display (KCM_TFT) + ILI9341_t3 fonts
#include <KCM_Touch.h>             // FT5316 touch: TouchResult/setupTouch/isTouched/readTouch
#include <KerbalDisplayAudio.h>
#include <KerbalSimpit.h>
#include <KCMk1_SystemConfig.h>   // shared hardware/threshold constants (KCMk1_SystemConfig library)

// rev-2 compat: the screens declare font pointers as `tFont` (the old sumotoy type
// name). Fonts are now ILI9341_t3 — alias so the existing declarations compile.
typedef ILI9341_t3_font_t tFont;


/***************************************************************************************
   SCREEN TYPE ENUM
   Thirteen information screens reached via ten right-hand sidebar buttons: SCFT/ACFT/
   ROVR share one "PFD" button (context- or title-selected, see SB_BTN_SCREEN in
   AAA_Screens.ino), most of the rest map 1:1. LNCHAP (Ascent Autopilot) has its own
   "ASC" button. ORBADV (Advanced Orbital Elements) has no button — it is a title-tap
   sub-mode of ORB. LNDGRE (Re-entry) has its own button. ORBADV is manual-select only
   (contextScreen() never auto-picks it). screen_COUNT is a sentinel — not a real screen.
****************************************************************************************/
enum ScreenType : uint8_t {
  screen_LNCH   = 0,   // Launch
  screen_ORB    = 1,   // Orbit (Apsides graphic)
  screen_SCFT   = 2,   // Spacecraft attitude (EADI)
  screen_MNVR   = 3,   // Maneuver
  screen_TGT    = 4,   // Target / Rendezvous (RPOD display)
  screen_DOCK   = 5,   // Docking
  screen_LNDG   = 6,   // Landing (powered descent)
  screen_VEH    = 7,   // Vehicle
  screen_ACFT   = 8,   // Aircraft
  screen_ROVR   = 9,   // Rover
  screen_ORBADV = 10,  // Orbit — Advanced Elements (text readout)
  screen_LNDGRE = 11,  // Landing — Re-entry
  screen_LNCHAP = 12,  // Ascent Autopilot (replaces ORB+ sidebar slot; ORB+ via ORB title tap)
  screen_COUNT  = 13   // sentinel — not a real screen
};

// Manual-lock screen: automatic context switches (vessel change, dock re-check,
// scene entry) leave RE-ENTRY alone so a deliberate selection sticks — e.g. a
// re-entry lander shedding debris (heat shield / chutes) emits VESSEL_CHANGE, which
// would otherwise yank REEN away. ADVANCED ORBIT is no longer locked: a context
// event may switch away from it like any other ordinary screen.
inline bool isManualLockScreen(ScreenType s) {
  return s == screen_LNDGRE;
}

static const uint8_t SCREEN_COUNT = (uint8_t)screen_COUNT;


/***************************************************************************************
   DISPLAY OBJECT AND TOUCH
****************************************************************************************/
extern KCM_TFT     infoDisp;
extern TouchResult lastTouch;

// Hardware double buffer (RA8876 page flip). The loop redraws the whole active
// screen to the hidden page each frame (Model A) and flips — tear-free and free
// of single-buffer overdraw artifacts.
extern KCMDoubleBuffer infoDB;


/***************************************************************************************
   SCREEN STATE
   switchToScreen() is the only way to change screens — never set activeScreen directly.
   prevScreen == screen_COUNT is the sentinel that triggers a chrome redraw in loop().
****************************************************************************************/
extern ScreenType activeScreen;
extern ScreenType prevScreen;
void switchToScreen(ScreenType s);


/***************************************************************************************
   SKETCH VERSION
   Follows semantic versioning: MAJOR.MINOR.PATCH
     MAJOR — incompatible structural changes (screen layout overhaul, new hardware)
     MINOR — new features added (new screen, new data source, new display element)
     PATCH — bug fixes, tuning, colour/label tweaks
   This sketch requires KerbalDisplayCommon >= 3.3.1
****************************************************************************************/
static const uint8_t SKETCH_VERSION_MAJOR = 1;
static const uint8_t SKETCH_VERSION_MINOR = 0;
static const uint8_t SKETCH_VERSION_PATCH = 5;   // 1.0.5: reticle rings are the colour bands


/***************************************************************************************
   OPERATING MODE FLAGS
****************************************************************************************/
extern bool       debugMode;
extern bool       demoMode;
extern bool       fpsDiag;   // true = print frame-rate / render-time diagnostics to Serial (~1 Hz)
extern const bool STANDALONE_TEST;  // true = skip I2C master handshake (no master connected)
extern const float STALL_SPEED_MS;
extern const float REENTRY_SAS_AERO_STABLE_MACH;
extern const float LNDG_CHUTE_MAIN_MAX_Q;    // main chute rip dynamic pressure (Pa)
extern const float LNDG_CHUTE_DROGUE_MAX_Q;  // drogue rip dynamic pressure (Pa)
extern const float LNDG_CHUTE_SEMI_DENSITY;
extern const float LNDG_DROGUE_FULL_ALT;
extern const float LNDG_MAIN_FULL_ALT;
extern const uint8_t DISPLAY_ROTATION;

// Flight state (populated by SimpitHandler.ino)
extern bool simpitConnected;  // true after Simpit handshake succeeds
extern bool flightScene;      // true when KSP is in a flight scene
extern bool idleState;        // true when master wants standby when not in flight

// I2CSlave.ino
extern volatile bool i2cProceedReceived;
void setupI2CSlave();
void updateI2CState();
void buildI2CPacketAndAssert();

// BootScreen.ino
void bootSimText(KCM_TFT &tft);

// Simpit object (defined in SimpitHandler.ino)
extern KerbalSimpit simpit;


/***************************************************************************************
   DISPLAY STATE
   AppState holds all telemetry values shown on screen. In Phase 1 (demo) these are
   driven by Demo.ino. In Phase 2 (Simpit) they will be populated by SimpitHandler.ino.
   All float fields default to 0.0f; String fields to "---".
****************************************************************************************/
struct AppState {
  // Altitude & velocity
  float     altitude      = 0.0f;    // m ASL
  float     radarAlt      = 0.0f;    // m AGL (terrain)
  float     orbitalVel    = 0.0f;    // m/s
  float     surfaceVel    = 0.0f;    // m/s
  float     verticalVel   = 0.0f;    // m/s

  // Apsides & time
  float     apoapsis      = 0.0f;    // m
  float     periapsis     = 0.0f;    // m
  float     timeToAp      = 0.0f;    // seconds
  float     timeToPe      = 0.0f;    // seconds

  // Orbital elements (from ORBIT_MESSAGE)
  float     inclination   = 0.0f;    // degrees
  float     eccentricity  = 0.0f;
  float     semiMajorAxis = 0.0f;    // m
  float     orbitalPeriod = 0.0f;    // seconds
  float     LAN           = 0.0f;    // longitude of ascending node, degrees
  float     argOfPe       = 0.0f;    // argument of periapsis, degrees
  float     trueAnomaly   = 0.0f;    // degrees
  float     meanAnomaly   = 0.0f;    // degrees

  // Delta-V & burn
  float     stageDeltaV   = 0.0f;    // m/s
  float     totalDeltaV   = 0.0f;    // m/s
  float     stageBurnTime = 0.0f;    // seconds remaining
  float     throttle      = 0.0f;    // 0.0..1.0 main engine throttle (0-100%)
  float     wheelThrottle = 0.0f;    // -1.0..1.0 wheel throttle (rovers): +ve=fwd, -ve=rev — WHEEL_CMD_MESSAGE (w.throttle)

  // Maneuver node
  float     mnvrTime      = 0.0f;    // seconds to next maneuver node
  float     mnvrDeltaV    = 0.0f;    // m/s of next maneuver
  float     mnvrTotalDeltaV = 0.0f;  // m/s — dV remaining across all planned maneuver nodes (Simpit maneuver deltaVTotal)
  float     mnvrDuration  = 0.0f;    // seconds burn for next maneuver
  float     mnvrHeading   = 0.0f;    // degrees — heading to point for burn (KSP1 only)
  float     mnvrPitch     = 0.0f;    // degrees — pitch to point for burn (KSP1 only)

  // Attitude (from ROTATION_DATA_MESSAGE / vesselPointingMessage)
  float     heading       = 0.0f;    // degrees 0-360
  float     pitch         = 0.0f;    // degrees -90..+90
  float     roll          = 0.0f;    // degrees -180..+180
  float     orbVelHeading = 0.0f;    // orbital velocity vector heading
  float     orbVelPitch   = 0.0f;    // orbital velocity vector pitch
  float     srfVelHeading = 0.0f;    // surface velocity vector heading
  float     srfVelPitch   = 0.0f;    // surface velocity vector pitch

  // Aircraft
  float     machNumber    = 0.0f;
  float     IAS           = 0.0f;    // m/s indicated airspeed
  float     gForce        = 0.0f;    // g

  // Action groups
  bool      gear_on       = false;
  bool      airbrake_on   = false;   // airbrake CAG (AIRBRAKE_CAG) is ON — AIRCRAFT screen
  bool      trimEnabled   = false;   // trim-hold enabled (from master, I2C controlByte bit 2) — SCFT/ACFT
  bool      brakes_on     = false;
  bool      drogueDeploy  = false;   // drogue deploy CAG is ON
  bool      drogueCut     = false;   // drogue cut CAG is ON (terminal state)
  bool      mainDeploy    = false;   // main chute deploy CAG is ON
  bool      mainCut       = false;   // main chute cut CAG is ON (terminal state)
  float     airDensity    = 0.0f;    // kg/m3 from ATMO_CONDITIONS_MESSAGE (0 in vacuum)

  // Target (only valid when targetAvailable)
  bool      targetAvailable = false;
  float     tgtDistance   = 0.0f;    // m
  float     tgtVelocity   = 0.0f;    // m/s relative (negative = closing)
  float     tgtHeading    = 0.0f;    // degrees — bearing to target
  float     tgtPitch      = 0.0f;    // degrees — elevation to target
  float     tgtVelHeading = 0.0f;    // degrees — heading of relative velocity vector
  float     tgtVelPitch   = 0.0f;    // degrees — pitch of relative velocity vector

  // Orbit intercepts — KSP2 only (INTERSECTS_MESSAGE not available in KSP1).
  // Fields retained as stubs for future KSP2 or closest-approach implementation.
  float     intercept1Dist = -1.0f;
  float     intercept1Time = -1.0f;
  float     intercept2Dist = -1.0f;
  float     intercept2Time = -1.0f;

  // RCS state
  bool      rcs_on        = false;   // from ACTIONSTATUS_MESSAGE & RCS_ACTION

  // Resources
  float     electricChargePercent = 0.0f;  // 0.0..100.0 — from ELECTRIC_CHARGE resource channel

  // Thermal (from TEMP_LIMIT_MESSAGE)
  uint8_t   coreTempPct  = 0;   // hottest part core temp as % of limit (0–100)
  uint8_t   skinTempPct  = 0;   // hottest part skin temp as % of limit (0–100)

  // Vessel info
  String          vesselName    = "---";
  VesselType      vesselType    = type_Unknown;
  uint8_t         ctrlLevel     = 3;       // 0=none, 1=limited probe, 2=limited manned, 3=full
  VesselSituation situation     = sit_Flying;
  bool            isRecoverable = false;
  String          gameSOI       = "---";
  uint8_t         crewCount     = 0;
  uint8_t         crewCapacity  = 0;
  uint8_t         commNetSignal = 0;    // CommNet signal strength 0-100%; 0 when CommNet unused
  bool            inAtmo        = false;   // true when vessel is in atmosphere
  uint8_t         sasMode       = 255;     // AutopilotMode enum; 255 = SAS disabled

  // Ascent autopilot — status echoed from Controller_Main (AscentStatus over I2C).
  // The parameter fields carry the autopilot's currently-confirmed values; the
  // InfoDisp stages touch/keypad edits locally and sends them back via the outbound
  // command channel. Actual Ap/Pe, g-force and SoI body reuse the existing fields
  // (apoapsis / periapsis / gForce / gameSOI).
  bool      apArmed        = false;   // autopilot engaged
  uint8_t   apPhase        = 0;       // 0 IDLE,1 VERTICAL,2 GRAVITY TURN,3 COAST,4 CIRCULARIZE,5 COMPLETE,6 ABORT
  float     apTargetAlt    = 0.0f;    // m   — commanded target apoapsis
  float     apInclination  = 0.0f;    // deg 0-180 (0 equatorial, 90 polar, >90 retrograde)
  bool      apSoutherly    = false;   // launch direction: false = N (ascending), true = S (descending)
  float     apLoft         = 1.0f;    // exponent ~0.5-2.0 (<1 aggressive, 1 balanced, >1 lofted)
  bool      apRollEnable   = false;   // roll hold enabled
  float     apRollDeg      = 0.0f;    // deg -180..180 (roll hold target)
  float     apMaxG         = 0.0f;    // g cap (0 = off)
  float     apCmdPitch     = 0.0f;    // deg above horizon (commanded)
  float     apCmdHeading   = 0.0f;    // deg azimuth (commanded)
  float     apCmdThrottle  = 0.0f;    // 0..1 (commanded)
  float     apDynPressure  = 0.0f;    // Pa (dynamic pressure)
};

extern AppState state;
extern BodyParams currentBody;

// Re-entry corridor boundaries (metres ASL), used by the RE-ENTRY screen. Declared
// here (not in the .ino) so Arduino's auto-generated prototypes for the helpers that
// return it see the type. See Screen_LNDG_Reentry.ino for how the bands are derived.
struct ReCorridor { float dangerLine, safeTop, atmoTop; bool valid; };


/***************************************************************************************
   LAYOUT CONSTANTS
   Defined here so both Screens.ino and TouchEvents.ino can reference them.
****************************************************************************************/
// PHASE 2 (rev-2 redesign): the shared framework now spans the full 1024x600 panel.
// The navigation chrome (sidebar, title bar) and text-row screens derive their
// geometry from these constants and expand automatically. Graphical screens still
// carry absolute 800x480 coordinates and render top-left-anchored until each is
// redesigned in its own step.
static const uint16_t SCREEN_W  = KCM_SCREEN_W;   // 1024
static const uint16_t SCREEN_H  = KCM_SCREEN_H;   // 600
static const uint16_t SIDEBAR_W = 84;
static const uint8_t  ROW_COUNT = 24;  // max cache slots per screen (Ascent Autopilot uses the most)


/***************************************************************************************
   FUNCTION DECLARATIONS
****************************************************************************************/

// Screen*.ino — chrome (static elements drawn once on transition)
void drawStaticScreen(KCM_TFT &tft, ScreenType s);

// AAA_Screens.ino — shared SAS-mode navball label/palette (SCFT/ACFT/ROVR)
void sasNavballLabel(uint8_t mode, const char *&v, uint16_t &fg, uint16_t &bg);

// EADIBall.ino — shared PFD tape/box/roll-readout helpers (SCFT + ACFT).
// The two attitude screens share pixel-identical tape/box/roll geometry; only the
// marker sets (which triangles each draws) and roll warn/alarm colouring differ, so
// those are passed in as parameters. Prototypes are declared here (rather than left to
// Arduino's auto-prototype pass) because the signatures take references and a struct
// pointer. Each caller keeps and passes its OWN prev-state caches by reference so the
// dirty-suppress/reset timing stays per-screen and byte-identical to before.
struct EadiTapeMarker { float value; uint16_t colour; };   // one coloured tape triangle
void eadiDrawPitchTape(KCM_TFT &tft, float pitch,
                       const EadiTapeMarker *markers, uint8_t nMarkers);
void eadiDrawHeadingTape(KCM_TFT &tft, float hdg, int16_t &prevHdgBox,
                         const EadiTapeMarker *markers, uint8_t nMarkers);
void eadiUpdatePitchBox(KCM_TFT &tft, float pitch, int16_t &prevBox);
void eadiUpdateHdgBox(KCM_TFT &tft, float hdg, int16_t &prevBox);
void eadiUpdateRollReadout(KCM_TFT &tft, float roll, uint16_t fg, uint16_t bg,
                           int16_t &prevReadout, uint16_t &prevFg);
// eadiHdgDelta (heading wrap to ±180°) moved to KerbalDisplayCommon ≥ 3.2.0 — it had
// eighteen call sites across the sketch and no dependency on anything sketch-local.

// The reticle marker layer moved to KerbalDisplayCommon ≥ 3.3.0 as well:
// ReticleGeom / ReticleDotCache / ReticleAngles and reticleProject / reticleClampDot /
// reticleEraseDot / reticleRepairDotChrome / reticleUpdateDots. None of it reads
// telemetry, so it belongs with the reticleDrawBase chrome it draws on, and MNVR /
// DOCK / TGT now share one implementation.
//
// What stays here is the seam — the one reticle function that touches the global
// `state`, turning vessel/target telemetry into the library's ReticleAngles.
// Defined in AAA_Screens.ino.
ReticleAngles reticleComputeAngles();

// Screen*.ino — update (dynamic values redrawn each loop)
void updateScreen(KCM_TFT &tft, ScreenType s);

// Standby screen (shown when not in a flight scene)
void drawStandbyScreen(KCM_TFT &tft);

// Context-dependent screen selection on vessel/scene change
ScreenType contextScreen();

// Sidebar button ↔ screen mapping (10 buttons; PFD covers SCFT/ACFT/ROVR)
uint8_t    screenToButton(ScreenType s);
ScreenType pfdContextScreen();
ScreenType pfdScreenForSel(uint8_t sel);
ScreenType pfdSelectedScreen();

// TouchEvents.ino  (rev-2: FT5316 polling driver — no ISR)
void processTouchEvents();

// Demo.ino
void initDemoMode();
void stepDemoState();

// SimpitHandler.ino
void initSimpit();


/***************************************************************************************
   ROW CACHE
   Tracks last-drawn value and colours per screen row for flicker-free updates.
   Defined here (not Screens.ino) so TouchEvents.ino can reference rowCache[].
****************************************************************************************/
struct RowCache {
  String   value = "\x01";   // sentinel — never matches a real formatted string
  uint16_t fg    = 0x0001;
  uint16_t bg    = 0x0001;
};


/***************************************************************************************
   CROSS-FILE STATE
   Variables defined in Screens.ino, accessed by TouchEvents.ino.
   Declared extern here so TouchEvents doesn't need inline extern declarations.
****************************************************************************************/
extern RowCache    rowCache  [SCREEN_COUNT][ROW_COUNT];   // #32 use named constants
extern PrintState  printState[SCREEN_COUNT][ROW_COUNT];  // #32 use named constants

// LNCH phase state
extern bool _lnchOrbitalMode;
extern bool _lnchManualOverride;
extern bool _lnchPrelaunchMode;
extern bool _lnchPrelaunchDismissed;

// PFD button state (SPACECRAFT/AIRCRAFT/ROVER — title-touch cycle)
extern bool    _pfdManualOverride;
extern uint8_t _pfdManualSel;

// LNDG mode state
extern bool _lndgReentryMode;
extern bool _lndgReentryRow0TPe;
extern bool _lndgReentryRow1SL;  // row 1 label: true = Alt.SL (above atmosphere), false = Alt.Rdr

// LNDG parachute deployment state — reset on vessel switch
extern bool _drogueDeployed;
extern bool _mainDeployed;
extern bool _drogueCut;
extern bool _mainCut;
extern bool _drogueArmedSafe;
extern bool _mainArmedSafe;

// RNDZ/DOCK chrome state — defined in Screen_RNDZ/DOCK.ino, used by AAA_Screens.ino dispatch
extern bool _tgtChromDrawn;
extern bool _dockChromDrawn;
extern bool _vesselDocked;
extern uint32_t _dockedTimestamp;
extern bool _pendingContextSwitch;  // set on vessel change; cleared when FLIGHT_STATUS arrives
extern bool _pendingDockCheck;      // set after context switch; cleared when TARGETINFO arrives
extern bool _orbAdvancedMode; // true = ADVANCED ELEMENTS tap-through view, false = APSIDES default
extern bool _scftPrevOrbMode;  // Screen_SCFT: previous orbital mode (reset on vessel switch)
