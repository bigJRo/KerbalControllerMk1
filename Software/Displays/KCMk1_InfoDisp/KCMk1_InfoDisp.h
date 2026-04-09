#pragma once
/***************************************************************************************
   KCMk1_InfoDisp.h -- Shared declarations for Kerbal Controller Mk1 Information Display
   Included by every .ino tab. Defines types, enums, structs, and extern declarations.
****************************************************************************************/

#include <KerbalDisplayCommon.h>
#include <KerbalDisplayAudio.h>
#include <KerbalSimpit.h>
#include <KCMk1_SystemConfig.h>   // shared hardware/threshold constants (KCMk1_SystemConfig library)
// I2C slave interface will be added in Phase 3.


/***************************************************************************************
   SCREEN TYPE ENUM
   Eight information screens navigated by the right-hand sidebar.
   screen_COUNT is a sentinel — not a real screen.
****************************************************************************************/
enum ScreenType : uint8_t {
  screen_LNCH  = 0,   // Launch
  screen_ORB   = 1,   // Orbit (Apsides default + Advanced Elements tap-through)
  screen_ATT   = 2,   // Attitude
  screen_MNVR  = 3,   // Maneuver
  screen_RNDZ  = 4,   // Rendezvous / Target
  screen_DOCK  = 5,   // Docking
  screen_LNDG  = 6,   // Landing
  screen_VEH   = 7,   // Vehicle
  screen_ACFT  = 8,   // Aircraft
  screen_MISC  = 9,   // Rover
  screen_COUNT = 10   // sentinel — not a real screen
};

static const uint8_t SCREEN_COUNT = (uint8_t)screen_COUNT;


/***************************************************************************************
   DISPLAY OBJECT AND TOUCH
****************************************************************************************/
extern RA8875      infoDisp;
extern TouchResult lastTouch;


/***************************************************************************************
   SCREEN STATE
   switchToScreen() is the only way to change screens — never set activeScreen directly.
   prevScreen == screen_COUNT is the sentinel that triggers a chrome redraw in loop().
****************************************************************************************/
extern ScreenType activeScreen;
extern ScreenType prevScreen;
extern uint32_t   lastScreenSwitch;   // #8 timestamp of last switchToScreen() call
void switchToScreen(ScreenType s);


/***************************************************************************************
   SKETCH VERSION
   Follows semantic versioning: MAJOR.MINOR.PATCH
     MAJOR — incompatible structural changes (screen layout overhaul, new hardware)
     MINOR — new features added (new screen, new data source, new display element)
     PATCH — bug fixes, tuning, colour/label tweaks
   This sketch requires KerbalDisplayCommon >= 2.0.0
****************************************************************************************/
static const uint8_t SKETCH_VERSION_MAJOR = 0;
static const uint8_t SKETCH_VERSION_MINOR = 13;
static const uint8_t SKETCH_VERSION_PATCH = 3;


/***************************************************************************************
   OPERATING MODE FLAGS
****************************************************************************************/
extern bool  debugMode;
extern bool  demoMode;
extern const float STALL_SPEED_MS;
extern const float REENTRY_SAS_AERO_STABLE_MACH;
extern const float LNDG_DROGUE_SAFE_MS;
extern const float LNDG_DROGUE_RISKY_MS;
extern const float LNDG_MAIN_SAFE_MS;
extern const float LNDG_MAIN_RISKY_MS;
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
void bootSimText(RA8875 &tft);

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
};

extern AppState state;
extern BodyParams currentBody;


/***************************************************************************************
   LAYOUT CONSTANTS
   Defined here so both Screens.ino and TouchEvents.ino can reference them.
****************************************************************************************/
// SCREEN_W and SCREEN_H provided by KCMk1_SystemConfig.h as KCM_SCREEN_W / KCM_SCREEN_H
static const uint16_t SCREEN_W  = KCM_SCREEN_W;   // #3A alias for local code
static const uint16_t SCREEN_H  = KCM_SCREEN_H;   // #3A alias for local code
static const uint16_t SIDEBAR_W = 80;
static const uint8_t  ROW_COUNT = 17;  // max cache slots per screen (LNCH pre-launch uses slots up to 16)


/***************************************************************************************
   FUNCTION DECLARATIONS
****************************************************************************************/

// Screen*.ino — chrome (static elements drawn once on transition)
void drawStaticScreen(RA8875 &tft, ScreenType s);

// Screen*.ino — update (dynamic values redrawn each loop)
void updateScreen(RA8875 &tft, ScreenType s);

// Standby screen (shown when not in a flight scene)
void drawStandbyScreen(RA8875 &tft);

// Context-dependent screen selection on vessel/scene change
ScreenType contextScreen();

// TouchEvents.ino
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

// LNDG mode state
extern bool _lndgReentryMode;
extern bool _lndgReentryRow3PeA;
extern bool _lndgReentryRow0TPe;
extern bool _lndgReentryRow1SL;  // true when row 3 shows PeA (radarAlt > 2000m), false = V.Hrz

// LNDG parachute deployment state — reset on vessel switch
extern bool _drogueDeployed;
extern bool _mainDeployed;
extern bool _drogueCut;
extern bool _mainCut;
extern bool _drogueArmedSafe;
extern bool _mainArmedSafe;

// RNDZ/DOCK chrome state — defined in Screen_RNDZ/DOCK.ino, used by AAA_Screens.ino dispatch
extern bool _rndzChromDrawn;
extern bool _dockChromDrawn;
extern bool _vesselDocked;
extern uint32_t _dockedTimestamp;
extern bool _pendingContextSwitch;  // set on vessel change; cleared when FLIGHT_STATUS arrives
extern bool _pendingDockCheck;      // set after context switch; cleared when TARGETINFO arrives
extern bool _orbAdvancedMode; // true = ADVANCED ELEMENTS tap-through view, false = APSIDES default
extern bool _prevShowAp;      // Screen_ORB: which time row was last shown (reset on vessel switch)
extern bool _attPrevOrbMode;  // Screen_ATT: previous orbital mode (reset on vessel switch)
