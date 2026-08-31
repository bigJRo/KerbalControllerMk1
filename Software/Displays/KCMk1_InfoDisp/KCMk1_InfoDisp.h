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
   INFO DISPLAY UNIT SELECT
   Which physical Info Display board this firmware image targets. Both boards run the
   same image; the unit number picks the I2C slave address, the sidebar layout side,
   the context-routing ladder and the sidebar button set.
     1 = Info Display 1 -> I2C 0x12, panel A1 (left display panel, inboard half).
         Vehicle-type display: the PFD family (SPACECRAFT / AIRCRAFT / ROVER / VEHICLE).
         Sidebar on the LEFT (outboard) edge. No Ascent Autopilot console.
     2 = Info Display 2 -> I2C 0x13, panel B1 (right display panel, inboard half).
         Mission-phase display: LAUNCH / LANDING / DOCKING / TARGET / MANEUVER / ORBIT.
         Sidebar on the RIGHT (outboard) edge. Owns the Ascent Autopilot console.
   Set this before flashing each board.

   This lives in the header rather than AAA_Config.ino because the layout constants
   and sidebar tables below are compile-time conditional on it, and the header is
   processed before any .ino tab in the concatenated sketch translation unit.
****************************************************************************************/
#ifndef INFO_DISP_UNIT
#define INFO_DISP_UNIT 1
#endif
#if (INFO_DISP_UNIT != 1) && (INFO_DISP_UNIT != 2)
#error "INFO_DISP_UNIT must be 1 (Info Display 1) or 2 (Info Display 2)"
#endif

// Role predicates — prefer these to bare INFO_DISP_UNIT comparisons at use sites.
#define INFO_DISP_IS_PFD_UNIT      (INFO_DISP_UNIT == 1)   // vehicle-type display
#define INFO_DISP_IS_MISSION_UNIT  (INFO_DISP_UNIT == 2)   // mission-phase display


/***************************************************************************************
   SCREEN TYPE ENUM
   Thirteen information screens reached via six sidebar buttons (see SB_BTN_SCREEN in
   AAA_Screens.ino, which is per-unit). Several buttons cover more than one screen and
   cycle their modes on a repeat press: PFD covers SCFT/ACFT/ROVR (+VEH on unit 2),
   ORB covers ORB/ORBADV/MNVR, TGT covers TGT/DOCK, LNDG covers LNDG/LNDGRE. The
   sixth button is LNCHAP (Ascent Autopilot console) on unit 2 and VEH on unit 1.
   screen_COUNT is a sentinel — not a real screen.
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
  screen_NAV    = 13,  // Navigation display — compass rose, ground track, target bearing
  screen_COUNT  = 14   // sentinel — not a real screen
};

// RE-ENTRY used to pin itself against automatic switches, because it was manual-only
// and a VESSEL_CHANGE (a lander shedding a heat shield) would otherwise yank it away.
// Both halves of that reasoning are gone: the ladders now run continuously, so a
// re-selection follows immediately after any such event, and the generalised manual
// latch below already makes a deliberate pick stick on every screen rather than one.

// The panel's home screen: where it sits before any telemetry has arrived (boot,
// demo mode) and where it is parked while KSP is out of a flight scene, so the first
// frame after a scene entry is already the right screen for this panel's role rather
// than a screen it then has to switch away from.
//
// This is the role's resting screen, not its context ladder's fallback. On unit 2 the
// ladder rests on ORBIT, but a panel that has never seen telemetry is far more likely
// to be about to launch than to be in orbit, so LAUNCH stays its home. On unit 1 the
// PFD is the answer in every case its ladder can produce for an unknown vessel.
#if INFO_DISP_IS_PFD_UNIT
static const ScreenType SCREEN_HOME = screen_SCFT;   // vehicle-type panel -> PFD
#else
static const ScreenType SCREEN_HOME = screen_LNCH;   // mission-phase panel -> LAUNCH
#endif

// Manual selection latch. Set by any sidebar press that changes the screen, cleared
// on vessel change and on flight-scene entry. While it is set, context auto-routing
// leaves the screen alone — a deliberate pick outlives an incidental context event
// (crossing docking range, a node appearing, an atmosphere transition). This matters
// more with the two panels split by role than it did on a single display: each panel
// now has a job, so a screen the pilot parked is a screen they are using.
// Generalises the per-button _pfdManualOverride / _lnchManualOverride latches.
extern bool       _manualScreenLatch;
extern ScreenType _latchedAgainst;
void clearManualScreenLatch();
void setManualScreenLatch();

// The gate every context auto-switch must pass.
bool contextSwitchAllowed();


/***************************************************************************************
   TWO-STATE MODE WITH A PILOT OVERRIDE
   Generalises the manual selection latch above to any automatic two-way choice a
   screen makes for itself -- SPACECRAFT's velocity reference (SRF/ORB), AIRCRAFT's
   altitude datum (SL/RDR).

   This is more authentic than a purely automatic mode, not less. The Shuttle's ADI
   ATTITUDE switch was a manual three-position selector (INRTL / LVLH / REF) and Apollo's
   was the same; an attitude reference the crew could not choose is the ahistorical part.

   RELEASE RULE is the screen latch's, for the same reason: an override means "not this,
   now", not "never again". `against` records what the automatic rule was saying at the
   moment of the press, so once the rule's own answer changes the pilot's objection is
   about a situation that no longer exists and the latch drops. Pressing again while auto
   already agrees is the explicit "back to auto". Vessel change and scene entry clear it,
   as they clear the screen latch.

   The struct lives here rather than in a .ino because the Arduino builder hoists function
   prototypes above everything a .ino declares -- a prototype taking it by reference could
   not see a definition further down the tab. Same reason as ReCorridor and ApsisTape.
****************************************************************************************/
struct ModeOverride {
  bool manual  = false;   // the pilot has pinned a choice
  bool pinned  = false;   // the choice they pinned
  bool against = false;   // what the automatic rule said when they pinned it
};

// Effective value: the pinned choice while held, the automatic one otherwise. Releases a
// held choice whose situation has passed, so call this every frame rather than caching.
bool modeResolve(ModeOverride &m, bool autoValue);

// One press: flip the effective value and hold it -- or drop back to auto if the flip
// lands on what the automatic rule already wants.
void modeToggle(ModeOverride &m, bool autoValue);

void modeClearOverride(ModeOverride &m);

// Per-frame context routing — releases a stale override, then routes if allowed.
void updateContextScreen();

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
   This sketch requires KerbalDisplayCommon >= 3.5.0
****************************************************************************************/
static const uint8_t SKETCH_VERSION_MAJOR = 1;
static const uint8_t SKETCH_VERSION_MINOR = 11;
static const uint8_t SKETCH_VERSION_PATCH = 0;   // 1.11.0: pilot-selectable attitude references


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
extern const float TGT_CONTEXT_MAX_M;    // TARGET auto-select outer bound (m)
extern const float MNVR_CONTEXT_LEAD_S;  // MANEUVER auto-select lead before ignition (s)
extern const uint32_t CONTEXT_DWELL_MS;
extern const float DOCK_CTX_RELEASE_M, TGT_CTX_RELEASE_MIN_M, TGT_CTX_RELEASE_MAX_M;
extern const float MNVR_CTX_RELEASE_S, REENTRY_CTX_MACH;
extern const float LNDG_CTX_ALT_M, LNDG_CTX_ALT_RELEASE_M, LNDG_CTX_VVERT_MS;
extern const float   LNCH_Q_WARN_KPA;
extern const float   LNCH_Q_ALARM_KPA;
extern const float   LNCH_G_WARN;
extern const float   LNCH_G_ALARM;
extern const float   LNCH_COAST_THROTTLE;
extern const float   LNCH_CTX_VVERT_RELEASE_MS;
extern const int16_t NAV_DRIFT_WARN_DEG;

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
  float     tgtVelocity   = 0.0f;    // m/s along the bearing, negative = closing.
                                     //   NOT Simpit's raw field, which is the unsigned
                                     //   magnitude of the whole relative velocity --
                                     //   SimpitHandler projects it onto the line of
                                     //   sight so this sign convention is real.
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

/***************************************************************************************
   COMPASS ROSE (Compass.ino)
   Shared rotating compass card, used by ROVER and NAV. Pure geometry and pixels: it
   reads no telemetry and owns no state. The caller supplies its own radii and its own
   prev-drawn caches, so the redraw gating stays per-screen — the same split the
   reticle layer (MNVR/TGT/DOCK) and the EADI tape (SCFT/ACFT) already use.
****************************************************************************************/
struct CompassGeom {
  int16_t cx, cy;                 // card centre
  int16_t rRing;                  // outer ring
  int16_t rTickOuter;             // tick outer end (inside rRing, so tick-erase spares it)
  int16_t rTickMajorInner;        // 30 deg tick inner end
  int16_t rTickMinorInner;        // 5 deg tick inner end
  int16_t rLetter, rNumLabel;     // label centres
  int16_t noseRTip, noseRBase, noseHalfW;   // fixed 12 o'clock triangle, outside the ring
  int16_t mkRTip, mkRBase, mkHalfW;         // bearing marker, inside the ring
};
struct CompassCache       { float prevHeading = -9999.0f; };
struct CompassMarkerCache { bool prevAvail = false; float prevScreenDeg = -9999.0f; };

void compassPolar(const CompassGeom &g, float screenDeg, int16_t r, int16_t &x, int16_t &y);
void compassDrawRing(KCM_TFT &tft, const CompassGeom &g);
void compassDrawTicks(KCM_TFT &tft, const CompassGeom &g, float headingDeg, bool erase);
void compassDrawLabels(KCM_TFT &tft, const CompassGeom &g, float headingDeg, bool erase);
void compassDrawNose(KCM_TFT &tft, const CompassGeom &g);
void compassDrawMarker(KCM_TFT &tft, const CompassGeom &g, float screenDeg,
                       uint16_t colour, bool erase);
bool compassUpdateCard(KCM_TFT &tft, const CompassGeom &g, CompassCache &c,
                       float headingDeg, float threshDeg);
bool compassUpdateMarkerPair(KCM_TFT &tft, const CompassGeom &g,
                             CompassMarkerCache &ca, bool availA, float degA, uint16_t colA,
                             CompassMarkerCache &cb, bool availB, float degB, uint16_t colB,
                             float threshDeg);
void compassUpdateMarker(KCM_TFT &tft, const CompassGeom &g, CompassMarkerCache &c,
                         bool available, float screenDeg, uint16_t colour, float threshDeg);


// Re-entry corridor boundaries (metres ASL), used by the RE-ENTRY screen. Declared
// here (not in the .ino) so Arduino's auto-generated prototypes for the helpers that
// return it see the type. See Screen_LNDG_Reentry.ino for how the bands are derived.
struct ReCorridor { float dangerLine, safeTop, atmoTop; bool valid; };

// Apsis-tape frame (metres ASL) for the circularisation screen's convergence tape:
// the altitudes at the top and bottom of the scale and the tick spacing between them.
// Here rather than in the .ino for the same reason as ReCorridor above -- the helper
// that computes it returns one by value, and Arduino's generated prototype for that
// helper is emitted ahead of anything the .ino declares itself.
struct ApsisTape { float top, bottom, step; bool valid; };

// Re-entry corridor, shared by the RE-ENTRY screen and the mission context ladder so
// both agree on what a re-entry is. _rePeRegime: 0 danger, 1 safe, 2 aerobrake,
// 3 no re-entry, -1 n/a.
ReCorridor _reCorridor();
int8_t     _rePeRegime(const ReCorridor &c, float pe);


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
static const uint16_t CONTENT_W = SCREEN_W - SIDEBAR_W;   // 940 (moved here from AAA_Screens.ino
                                                          // so the touch helpers below can see it)
static const uint8_t  ROW_COUNT = 24;  // max cache slots per screen (Ascent Autopilot uses the most)


/***************************************************************************************
   SIDEBAR SIDE — per unit
   Both info displays sit inboard on their own panel, so their inner edges meet at the
   console centreline: Annunciator | Info 1 || Info 2 | Resource Display. Putting each
   sidebar on its panel's OUTBOARD edge mirrors the two about that centreline and buys
   three things:
     - the two content areas become adjacent, so the PFD on unit 1 and the phase screen
       on unit 2 read as one field rather than being split by 168 px of button column;
     - every content-area touch target on unit 1 (RCS/SAS/GEAR, the pre-launch dismiss
       tap) moves 84 px inboard, toward the pilot, rather than away;
     - each sidebar falls under its own hand — left off the A2 throttle/translation
       stick, right off the B2 rotation stick — so the forearm approaches from outboard
       and does not cross the content it is navigating.
   Content occupies [CONTENT_X, CONTENT_X + CONTENT_W); the sidebar occupies
   [SIDEBAR_X, SIDEBAR_X + SIDEBAR_W). The 1 px divider rule always sits on the
   sidebar's inboard edge, against the content.
****************************************************************************************/
#if INFO_DISP_IS_PFD_UNIT
static const uint16_t SIDEBAR_X     = 0;                    // left (outboard on panel A1)
static const uint16_t CONTENT_X     = SIDEBAR_W;
static const uint16_t SIDEBAR_DIV_X = SIDEBAR_W - 1;        // divider on the inboard edge
static const uint16_t SIDEBAR_BTN_X = 0;
#else
static const uint16_t SIDEBAR_X     = SCREEN_W - SIDEBAR_W; // right (outboard on panel B1)
static const uint16_t CONTENT_X     = 0;
static const uint16_t SIDEBAR_DIV_X = SCREEN_W - SIDEBAR_W;
static const uint16_t SIDEBAR_BTN_X = SCREEN_W - SIDEBAR_W + 1;
#endif

// Touch coordinates arrive in panel space. Screens lay themselves out in content
// space, which is offset by CONTENT_X, so a content-area handler must be given the
// translated x. On unit 2 CONTENT_X is 0 and these are all identities.
inline bool     touchInSidebar(uint16_t x) { return x >= SIDEBAR_X && x < SIDEBAR_X + SIDEBAR_W; }
inline bool     touchInContent(uint16_t x) { return x >= CONTENT_X && x < CONTENT_X + CONTENT_W; }
inline uint16_t touchContentX(uint16_t x)  { return (uint16_t)(x - CONTENT_X); }

/***************************************************************************************
   ASCENT AUTOPILOT COMMAND CONTRACT
   Opcodes carried in the outbound I2C command frame (Screen_LNCH_AscentAP.ino builds
   it, I2CSlave.ino ships it, Controller_Main executes it). Byte-level layout is in
   Documents/Developer/Ascent_Autopilot_Interface.md.

   They live here rather than in the console tab because Demo.ino also has to read them
   — in demo mode there is no master, so the demo executes these commands itself — and
   Demo.ino compiles before Screen_LNCH_AscentAP.ino in the concatenated sketch.
****************************************************************************************/
enum {
  AP_CMD_NOP             = 0x00,
  AP_CMD_SET_TARGET_ALT  = 0x01,   // payload = metres
  AP_CMD_SET_INCLINATION = 0x02,   // payload = degrees
  AP_CMD_SET_LAUNCH_DIR  = 0x03,   // payload = 0 north / 1 south
  AP_CMD_SET_LOFT        = 0x04,   // payload = exponent
  AP_CMD_SET_ROLL        = 0x05,   // payload = degrees, or AP_ROLL_OFF to disable hold
  AP_CMD_SET_MAXG        = 0x06,   // payload = g (0 = off)
  AP_CMD_ARM             = 0x10,   // payload = 0
  AP_CMD_DISARM          = 0x11,   // payload = 0
};
static const float AP_ROLL_OFF = 1.0e9f;   // roll-hold disable sentinel (outside +/-180)

// Demo.ino — stands in for Controller_Main while demoMode is set: applies an AP command
// to the demo's own autopilot model, which stepDemoState() then publishes into state.ap*
// so the console's command/echo round trip closes. Returns true if the opcode was
// recognised, which is what raises the console's pending cue.
bool apDemoApplyCommand(uint8_t op, float payload);

// Ascent Autopilot (Screen_LNCH_AscentAP.ino) — the armed state as annunciated: what
// the autopilot itself reports, never a pilot tap Controller_Main has not echoed back.
// The ARM button, the ARMED/DISARMED banner and the sidebar ASC key all read this, so
// they cannot disagree and none of them can say DISARMED while the vehicle is still
// being flown by the autopilot. A tap in flight shows as a pending cue on the ARM
// button, not as a change of state.
bool apArmedAnnunciated();

// Title-bar AUTO/MAN chip (AAA_Screens.ino) — whether the screen was chosen by the
// context ladder or is being held by hand.
void updateModeChip(KCM_TFT &tft);
void invalidateModeChip();

// Sidebar (AAA_Screens.ino). drawSidebar() paints the strip as chrome on a screen
// change; updateSidebar() repaints it mid-flight when a key's state colour changes.
void drawSidebar(KCM_TFT &tft);
void updateSidebar(KCM_TFT &tft);

// Drawing region control (AAA_Screens.ino). Screens draw in content space; the canvas
// origin is offset so those coordinates land in the content region of the panel.
void canvasContentRegion(KCM_TFT &tft);
void canvasPanelRegion(KCM_TFT &tft);


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

// Screen_NAV.ino — navigation display (compass rose, ground track, target bearing)
void chromeScreen_NAV(KCM_TFT &tft);
void drawScreen_NAV(KCM_TFT &tft);

// Screen*.ino — update (dynamic values redrawn each loop)
void updateScreen(KCM_TFT &tft, ScreenType s);

// Standby screen (shown when not in a flight scene)
void drawStandbyScreen(KCM_TFT &tft);

// Context-dependent screen selection on vessel/scene change. contextScreen() is the
// entry point every caller uses; it dispatches to the ladder for this unit. Both
// ladders are declared — and compiled — on both units, so flipping INFO_DISP_UNIT
// changes only which one runs.
ScreenType contextScreen();          // -> vehicle or mission ladder, per INFO_DISP_UNIT
ScreenType vehicleContextScreen();   // Info Display 1 — "what am I flying?"
ScreenType missionContextScreen();   // Info Display 2 — "what phase am I in?"

// Sidebar button ↔ screen mapping (6 buttons; PFD covers SCFT/ACFT/ROVR, +VEH on unit 2)
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
extern bool _lnchCoastLatched;
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
