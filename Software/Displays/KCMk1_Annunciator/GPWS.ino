/***************************************************************************************
   GPWS.ino -- Ground Proximity Warning System function for the KCMk1 Annunciator

   WHAT THIS IS
   An aviation-faithful GPWS that runs entirely on the Annunciator's Teensy 4.1. It
   watches the Simpit telemetry already collected in `state` (surface altitude,
   vertical speed, surface speed, gear, situation, roll, and target range/closing
   speed) and speaks voice callouts through the DFPlayer Mini on the 7" board
   (Serial2). It is INDEPENDENT of the tone() master-alarm state machine in
   KerbalDisplayAudio -- GPWS audio is all on the DFPlayer path and can coexist with
   the C&W tone.

   CONTROL MODEL (see the mode/flag mapping the master relays over I2C)
   Real GPWS has no per-mode arming: system on = every mode active. The GPWS Input
   panel's OFF/ACTIVE/PROX button + proxAlarm/rdvRadar flags are mapped to that as:
     OFF                         -> silent.
     ACTIVE (green)              -> EVERYTHING: all warning modes (1/2/3/4/6-bank)
                                    PLUS the altitude callouts + MINIMUMS + bug tone.
     PROX (amber) + proxAlarm    -> altitude callouts + MINIMUMS + bug tone ONLY
                                    (no warnings).
     PROX (amber) + rdvRadar     -> target-DISTANCE callouts + bug tone ONLY, on the
                                    Simpit target range instead of altitude.
   proxAlarm and rdvRadar are mutually exclusive on the panel (each its own amber
   submode); pressing either turns the button amber. So the three audio profiles are:
     warningsOn   = ACTIVE
     altCallouts  = ACTIVE || (PROX && !rdvRadar)
     distCallouts =           (PROX &&  rdvRadar)

   REAL-GPWS MODE COVERAGE (conscious scope decisions)
   KSP/KerbalSimpit exposes height-above-terrain (alt_surf ~ radio altitude), vertical
   speed, surface speed, gear, situation, roll, and target range -- but no ILS
   glideslope, flap position, wind, or forward-looking terrain database. So:
     Mode 1  Excessive descent rate .. IMPLEMENTED. SINK RATE (outer) and PULL UP
                                       (inner) as floored descent-rate-vs-altitude
                                       envelopes, active below ~2450 ft (747 m).
     Mode 2  Terrain closure ......... IMPLEMENTED (approx). Closure rate derived from
                                       the smoothed rate of change of alt_surf (which
                                       captures terrain rising under the vessel);
                                       TERRAIN then PULL UP. Landing-config submode 2B
                                       (needs flaps) omitted.
     Mode 3  Altitude loss after ..... IMPLEMENTED ("DON'T SINK"). Armed on liftoff,
             takeoff                    disarmed past the climbout ceiling; fires on
                                       altitude loss proportional to height AGL.
     Mode 4  Unsafe terrain clearance  PARTIAL. 4A TOO LOW GEAR (< ~500 ft, gear up)
                                       and speed-expanded TOO LOW TERRAIN (< ~1000 ft
                                       at high speed). 4B TOO LOW FLAPS omitted (no
                                       flap data).
     Mode 5  Below glideslope ........ NOT IMPLEMENTED. No ILS in KSP.
     Mode 6  Advisory callouts ....... IMPLEMENTED. Radio-altitude callouts (real FEET
                                       values), MINIMUMS at the decision height, and
                                       an altitude-dependent BANK ANGLE envelope.
     Mode 7  Windshear ............... NOT IMPLEMENTED. No wind data.

   THRESHOLD "BUG" (encoder value relayed from the GPWS Input panel, 0-9999 m)
   The threshold is a settable bug. Crossing it plays a bug TONE clip on the DFPlayer:
   descending through `threshold` m AGL in altitude profiles, or closing through
   `threshold` m range in the distance profile. In altitude profiles the threshold is
   also the MINIMUMS decision height -- so at the crossing you get the bug tone AND the
   spoken "MINIMUMS". (Distance profile: bug tone only -- "MINIMUMS" is an altitude
   term.)

   CALLOUT UNITS
   Altitude callouts use the real radio-altitude FEET values (2500/1000/500/.../10 ft),
   compared against alt_surf converted to metres internally. Distance callouts are in
   METRES (500/200/100/50/40/30/20/10/5/1). The spoken number clips are shared between
   the two ladders where the number matches.

   CADENCE (vs a real GPWS)
   Altitude/distance callouts and MINIMUMS: spoken ONCE per crossing. PULL UP repeats
   near-gaplessly (HARD_GAP_MS, BUSY-gated). SINK RATE / DON'T SINK / TOO LOW
   GEAR/TERRAIN / BANK ANGLE re-annunciate every ~1.5 s while in the envelope. All
   non-blocking: gpwsUpdate() never calls delay().

   PRIORITY (high -> low, one clip owns the DFPlayer per frame):
     PULL UP > TERRAIN > SINK RATE > DON'T SINK > TOO LOW TERRAIN > TOO LOW GEAR >
     BANK ANGLE > bug TONE > callout ladder / MINIMUMS.

   CROSSING-TRACKER DISCIPLINE
   `_prevAlt` / `_prevDist` are the ladder crossing detectors. They advance only when
   the ladder is serviced (a rung spoken or none pending); while a higher-priority clip
   owns the DFPlayer or it is busy, they are left frozen so a deferred callout
   announces the CURRENT altitude/range, not a backlog. While climbing (or the range
   opening) the tracker is bumped to the current value so a non-descending suppressor
   can't strand it. The bug-tone crossing uses SEPARATE simple trackers so the tone
   fires reliably regardless of ladder deferral.

   Clip numbers are the GPWS_CLIP_* map below; documented in KCMk1_Annunciator/README.md
   (DFPlayer folder /01). All tunables live in the TUNABLES block below.

   Licensed under the GNU General Public License v3.0 (GPL-3.0).
   Final code written by Jason Rostoker for Jeb's Controller Works.
****************************************************************************************/
#include "KCMk1_Annunciator.h"


/***************************************************************************************
   DFPLAYER INSTANCE
****************************************************************************************/
static KCM_DFPlayer gpwsDfp(KCM_DFPLAYER_SERIAL);


/***************************************************************************************
   CLIP NUMBER MAP (DFPlayer folder /01)
   Warnings/markers are dedicated clips; the number words (9-22) are shared between the
   altitude (feet) ladder and the distance (metre) ladder. Spoken text in README.
****************************************************************************************/
#define GPWS_FOLDER            1

#define GPWS_CLIP_PULL_UP       1    // "PULL UP"
#define GPWS_CLIP_TERRAIN       2    // "TERRAIN, TERRAIN"
#define GPWS_CLIP_SINK_RATE     3    // "SINK RATE"
#define GPWS_CLIP_TOO_LOW_GEAR  4    // "TOO LOW, GEAR"
#define GPWS_CLIP_TOO_LOW_TERR  5    // "TOO LOW, TERRAIN"
#define GPWS_CLIP_DONT_SINK     6    // "DON'T SINK"
#define GPWS_CLIP_BANK_ANGLE    7    // "BANK ANGLE"
#define GPWS_CLIP_MINIMUMS      8    // "MINIMUMS"
#define GPWS_CLIP_TONE          9    // bug tone / beep (threshold crossing)
// Number words (shared: feet altitude callouts + metre distance callouts)
#define GPWS_CLIP_2500         10    // "TWO THOUSAND FIVE HUNDRED"
#define GPWS_CLIP_1000         11    // "ONE THOUSAND"
#define GPWS_CLIP_500          12    // "FIVE HUNDRED"
#define GPWS_CLIP_400          13    // "FOUR HUNDRED"
#define GPWS_CLIP_300          14    // "THREE HUNDRED"
#define GPWS_CLIP_200          15    // "TWO HUNDRED"
#define GPWS_CLIP_100          16    // "ONE HUNDRED"
#define GPWS_CLIP_50           17    // "FIFTY"
#define GPWS_CLIP_40           18    // "FORTY"
#define GPWS_CLIP_30           19    // "THIRTY"
#define GPWS_CLIP_20           20    // "TWENTY"
#define GPWS_CLIP_10           21    // "TEN"
#define GPWS_CLIP_5            22    // "FIVE"
#define GPWS_CLIP_1            23    // "ONE"


/***************************************************************************************
   CALLOUT LADDERS
   Altitude ladder: real radio-altitude FEET values, stored as metres AGL.
   Distance ladder: metres of target range.
****************************************************************************************/
struct GpwsRung { float val; uint8_t clip; };

static const GpwsRung ALT_LADDER[] = {   // feet -> metres
  { 762.0f, GPWS_CLIP_2500 },  // 2500 ft
  { 304.8f, GPWS_CLIP_1000 },  // 1000 ft
  { 152.4f, GPWS_CLIP_500  },  //  500 ft
  { 121.9f, GPWS_CLIP_400  },  //  400 ft
  {  91.4f, GPWS_CLIP_300  },  //  300 ft
  {  61.0f, GPWS_CLIP_200  },  //  200 ft
  {  30.5f, GPWS_CLIP_100  },  //  100 ft
  {  15.2f, GPWS_CLIP_50   },  //   50 ft
  {  12.2f, GPWS_CLIP_40   },  //   40 ft
  {   9.1f, GPWS_CLIP_30   },  //   30 ft
  {   6.1f, GPWS_CLIP_20   },  //   20 ft
  {   3.0f, GPWS_CLIP_10   },  //   10 ft
};
static const uint8_t ALT_LADDER_COUNT = sizeof(ALT_LADDER) / sizeof(ALT_LADDER[0]);

static const GpwsRung DIST_LADDER[] = {  // metres of range
  { 500.0f, GPWS_CLIP_500 },
  { 200.0f, GPWS_CLIP_200 },
  { 100.0f, GPWS_CLIP_100 },
  {  50.0f, GPWS_CLIP_50  },
  {  40.0f, GPWS_CLIP_40  },
  {  30.0f, GPWS_CLIP_30  },
  {  20.0f, GPWS_CLIP_20  },
  {  10.0f, GPWS_CLIP_10  },
  {   5.0f, GPWS_CLIP_5   },
  {   1.0f, GPWS_CLIP_1   },
};
static const uint8_t DIST_LADDER_COUNT = sizeof(DIST_LADDER) / sizeof(DIST_LADDER[0]);


/***************************************************************************************
   TUNABLES  (all GPWS flight-tuning lives here)
   Descent-rate / closure envelopes are floored piecewise-linear boundaries in the form
   rate > floor + slope * altitude, approximating the published Honeywell Mark VII
   envelopes (representative values; exact boundaries vary by GPWS model).
****************************************************************************************/
static const uint8_t  GPWS_VOLUME          = 24;      // DFPlayer volume 0..30

// Mode 1 -- descent-rate envelopes (m/s vs m AGL), active below the ceiling.
static const float M1_CEIL_M        = 746.8f;  // 2450 ft
static const float SINK_FLOOR_MS    = 5.1f;    // ~1000 ft/min at low altitude
static const float SINK_SLOPE       = 0.0272f; // -> ~25 m/s (5000 fpm) at 747 m
static const float PULLUP_FLOOR_MS  = 7.62f;   // ~1500 ft/min at low altitude
static const float PULLUP_SLOPE     = 0.0374f; // -> ~35 m/s (7000 fpm) at 747 m

// Mode 2 -- terrain closure (m/s) from the smoothed rate of change of alt_surf.
static const float M2_CEIL_M        = 746.8f;
static const float TERR_FLOOR_MS    = 10.0f;   // closure floor at low altitude
static const float TERR_SLOPE       = 0.040f;  // closure boundary slope
static const float CLOSURE_ALPHA    = 0.30f;   // low-pass factor for closure rate
static const float CLOSURE_MIN_DT_S = 0.05f;   // min dt before recomputing closure

// Mode 3 -- DON'T SINK (altitude loss after takeoff).
static const float M3_CEIL_M        = 457.0f;  // ~1500 ft climbout window
static const float M3_LOSS_FRAC     = 0.10f;   // allowable loss ~10% of height AGL
static const float M3_LOSS_MIN_M    = 3.0f;    // minimum absolute loss to ever fire

// Mode 4 -- unsafe terrain clearance (gear up).
static const float GEAR_ALT_M       = 152.4f;  // 500 ft  -> TOO LOW GEAR
static const float TERR4_ALT_M      = 304.8f;  // 1000 ft -> TOO LOW TERRAIN (high speed)
static const float TOOLOW_SPEED_MS  = 98.0f;   // ~190 kt: above this the envelope expands

// Mode 6 -- BANK ANGLE, altitude-dependent limit (deg) ramped over a height band.
static const float BANK_LO_DEG      = 10.0f;   // limit at/below the low ramp altitude
static const float BANK_HI_DEG      = 45.0f;   // limit at/above the high ramp altitude
static const float BANK_RAMP_LO_M   = 9.1f;    // 30 ft
static const float BANK_RAMP_HI_M   = 45.7f;   // 150 ft

// General.
static const float DESCENT_DEADBAND_MS = 0.1f;    // |vel_vert| below this = level
static const float ALT_JUMP_M          = 2000.0f; // single-frame alt jump -> re-seed
static const float DIST_JUMP_M         = 2000.0f; // single-frame range jump -> re-seed
static const float MIN_DEDUP_M         = 8.0f;    // ladder rung within this of threshold = MINIMUMS
static const float THR_MIN_M           = 1.0f;    // valid threshold range (bug tone / minimums)
static const float THR_MAX_M           = 9999.0f;
static const float TGT_MIN_M           = 0.5f;    // minimum range for a valid target

// Repeat cadences (ms) -- ~1-1.5 s, matching real re-annunciation.
static const uint16_t HARD_GAP_MS   = 1400;
static const uint16_t SINK_GAP_MS   = 1500;
static const uint16_t MODE3_GAP_MS  = 1500;
static const uint16_t TERR4_GAP_MS  = 1500;
static const uint16_t GEAR_GAP_MS   = 1500;
static const uint16_t BANK_GAP_MS   = 1500;


/***************************************************************************************
   LIVE CONFIG (relayed by the master over I2C via gpwsSetConfig())
****************************************************************************************/
#define GPWS_MODE_OFF     0
#define GPWS_MODE_ACTIVE  1
#define GPWS_MODE_PROX    2

static uint8_t  _gpwsMode      = GPWS_MODE_OFF;
static bool     _gpwsProxAlarm = false;
static bool     _gpwsRdvRadar  = false;
static int16_t  _gpwsThreshold = 200;


/***************************************************************************************
   RUNTIME STATE
****************************************************************************************/
static bool     _gpwsEnabled  = false;
static bool     _busyAttached = false;

static float    _prevAlt      = -1.0f;   // altitude ladder crossing tracker
static float    _prevDist     = -1.0f;   // distance ladder crossing tracker

static float    _tonePrevAlt  = -1.0f;   // bug-tone crossing tracker (altitude)
static float    _tonePrevDist = -1.0f;   // bug-tone crossing tracker (range)
static bool     _tonePending  = false;   // bug tone crossed, awaiting the DFPlayer

// Mode 2 closure
static float    _closPrevAlt  = -1.0f;
static uint32_t _closPrevMs   = 0;
static float    _closRate     = 0.0f;    // smoothed closure rate (m/s, +ve = closing)

// Mode 1/2 hard warning
static bool     _hardActive   = false;
static bool     _terrainDone  = false;
static uint32_t _lastHardMs   = 0;

static uint32_t _lastSinkMs   = 0;

// Mode 3
static bool     _wasOnGround  = false;
static bool     _m3Armed      = false;
static float    _m3MaxAlt     = 0.0f;
static bool     _m3Active     = false;
static uint32_t _lastM3Ms     = 0;

// Mode 4
static bool     _terr4Active  = false;
static uint32_t _lastTerr4Ms  = 0;
static bool     _gearActive   = false;
static uint32_t _lastGearMs   = 0;

// Mode 6
static bool     _bankActive   = false;
static uint32_t _lastBankMs   = 0;


/***************************************************************************************
   HELPERS
****************************************************************************************/
static inline void gpwsPlay(uint8_t clip) {
  gpwsDfp.playFolderTrack(GPWS_FOLDER, clip);
  if (debugMode) { Serial.print(F("Annunciator: GPWS clip ")); Serial.println(clip); }
}

static inline bool gpwsThresholdValid() {
  float t = (float)_gpwsThreshold;
  return (t >= THR_MIN_M && t <= THR_MAX_M);
}

// Bank-angle limit at the current altitude (altitude-dependent ramp).
static float gpwsBankLimit(float alt) {
  if (alt <= BANK_RAMP_LO_M) return BANK_LO_DEG;
  if (alt >= BANK_RAMP_HI_M) return BANK_HI_DEG;
  return BANK_LO_DEG + (BANK_HI_DEG - BANK_LO_DEG) *
         (alt - BANK_RAMP_LO_M) / (BANK_RAMP_HI_M - BANK_RAMP_LO_M);
}

// Lowest rung in [prev, cur] the value descended/closed THROUGH. Returns clip, or 0.
// When minClip != 0 the threshold is treated as an extra "minimums" rung (and numeric
// rungs within MIN_DEDUP_M of it are masked so they don't double up with MINIMUMS).
static uint8_t gpwsCrossedClip(const GpwsRung *tbl, uint8_t n, float prev, float cur,
                               float thr, bool thrValid, uint8_t minClip) {
  uint8_t best = 0; float bestVal = 1e9f;
  for (uint8_t i = 0; i < n; i++) {
    float r = tbl[i].val;
    if (minClip && thrValid && fabsf(r - thr) < MIN_DEDUP_M) continue;
    if (prev > r && cur <= r && r < bestVal) { best = tbl[i].clip; bestVal = r; }
  }
  if (minClip && thrValid && prev > thr && cur <= thr && thr < bestVal) best = minClip;
  return best;
}

static void gpwsClearLatches() {
  _prevAlt = _prevDist = -1.0f;
  _tonePrevAlt = _tonePrevDist = -1.0f;
  _tonePending = false;
  _closPrevAlt = -1.0f; _closPrevMs = 0; _closRate = 0.0f;
  _hardActive = false; _terrainDone = false;
  _wasOnGround = false; _m3Armed = false; _m3MaxAlt = 0.0f; _m3Active = false;
  _terr4Active = false; _gearActive = false; _bankActive = false;
}


/***************************************************************************************
   SET CONFIG  (from I2CSlave.ino)
   cfgByte: bits1:0=mode, bit2=proxAlarm, bit3=rdvRadar (GPWS Input state-byte layout).
   thresholdM: int16 metres -- altitude decision height, or target range in rdv mode.
****************************************************************************************/
void gpwsSetConfig(uint8_t cfgByte, int16_t thresholdM) {
  uint8_t newMode = cfgByte & 0x03;
  bool    newProx = (cfgByte >> 2) & 0x01;
  bool    newRdv  = (cfgByte >> 3) & 0x01;

  bool changed = (newMode != _gpwsMode) || (newProx != _gpwsProxAlarm) ||
                 (newRdv != _gpwsRdvRadar) || (thresholdM != _gpwsThreshold);

  _gpwsMode      = newMode;
  _gpwsProxAlarm = newProx;
  _gpwsRdvRadar  = newRdv;
  _gpwsThreshold = thresholdM;

  if (debugMode && changed) {
    Serial.print(F("Annunciator: GPWS cfg mode=")); Serial.print(_gpwsMode);
    Serial.print(F(" prox=")); Serial.print(_gpwsProxAlarm);
    Serial.print(F(" rdv="));  Serial.print(_gpwsRdvRadar);
    Serial.print(F(" thr="));  Serial.println(_gpwsThreshold);
  }
}

void gpwsReset() {
  gpwsDfp.stop();
  gpwsClearLatches();
  _gpwsEnabled = false;
}

void gpwsSetup() {
  gpwsDfp.begin(GPWS_VOLUME, KCM_DFPLAYER_BAUD);
  gpwsDfp.attachBusyPin(KCM_AUDIO_BUSY_PIN);
  _busyAttached = gpwsDfp.busyPinAttached();
  gpwsClearLatches();
  if (debugMode) Serial.println(F("Annunciator: GPWS ready (DFPlayer on Serial2)"));
}


/***************************************************************************************
   UPDATE  -- call every loop() pass.
****************************************************************************************/
void gpwsUpdate() {
  // --- Enable gate -------------------------------------------------------------
  bool enabled = flightScene && audioEnabled && (_gpwsMode != GPWS_MODE_OFF);
  if (!enabled) {
    if (_gpwsEnabled) { gpwsDfp.stop(); gpwsClearLatches(); }
    _gpwsEnabled = false;
    return;
  }
  _gpwsEnabled = true;

  // --- Audio profiles for the current panel state ------------------------------
  bool warningsOn  = (_gpwsMode == GPWS_MODE_ACTIVE);
  bool altCallouts = (_gpwsMode == GPWS_MODE_ACTIVE) ||
                     (_gpwsMode == GPWS_MODE_PROX && !_gpwsRdvRadar);
  bool distCallouts = (_gpwsMode == GPWS_MODE_PROX && _gpwsRdvRadar);

  // --- Geometry ----------------------------------------------------------------
  uint8_t sit  = state.vesselSituationState;
  bool isAloft = bitRead(sit, VSIT_FLIGHT)  || bitRead(sit, VSIT_SUBORBIT) ||
                 bitRead(sit, VSIT_ORBIT)   || bitRead(sit, VSIT_ESCAPE);
  bool onGround = bitRead(sit, VSIT_LANDED) || bitRead(sit, VSIT_SPLASH) ||
                  bitRead(sit, VSIT_PRELAUNCH);
  float alt   = state.alt_surf;
  float vs    = fabsf(state.vel_vert);
  float spd   = state.vel_surf;
  bool descending = (state.vel_vert < -DESCENT_DEADBAND_MS);
  bool airDesc = isAloft && descending && alt > 0.0f;
  float thr   = (float)_gpwsThreshold;
  bool thrOK  = gpwsThresholdValid();

  uint32_t now = millis();
  bool busy = _busyAttached && gpwsDfp.isPlaying();

  // --- Mode 2 closure rate: smoothed -d(alt_surf)/dt ---------------------------
  if (_closPrevMs == 0 || alt < 0.0f) {
    _closPrevAlt = alt; _closPrevMs = now; _closRate = 0.0f;
  } else {
    float dt = (now - _closPrevMs) / 1000.0f;
    if (dt >= CLOSURE_MIN_DT_S) {
      float inst = -(alt - _closPrevAlt) / dt;                 // +ve = closing terrain
      if (fabsf(alt - _closPrevAlt) > ALT_JUMP_M) inst = 0.0f;  // ignore teleport/warp
      _closRate += CLOSURE_ALPHA * (inst - _closRate);
      _closPrevAlt = alt; _closPrevMs = now;
    }
  }

  // --- Mode 3 arm/track (every frame) ------------------------------------------
  if (onGround) {
    _m3Armed = false; _m3MaxAlt = 0.0f;
  } else if (isAloft) {
    if (_wasOnGround) { _m3Armed = true; _m3MaxAlt = alt; }   // liftoff edge
    if (alt > M3_CEIL_M) _m3Armed = false;                    // established climb
    if (alt > _m3MaxAlt) _m3MaxAlt = alt;
  }
  _wasOnGround = onGround;

  // --- Altitude ladder tracker: re-seed on jump, bump up while climbing --------
  if (_prevAlt < 0.0f || fabsf(alt - _prevAlt) > ALT_JUMP_M) _prevAlt = alt;
  if (alt > _prevAlt) _prevAlt = alt;

  // --- Distance ladder tracker (target range) ----------------------------------
  float dist = state.tgtDistance;
  bool  tgtValid = (dist > TGT_MIN_M);
  if (!tgtValid) { _prevDist = -1.0f; _tonePrevDist = -1.0f; }
  else {
    if (_prevDist < 0.0f || fabsf(dist - _prevDist) > DIST_JUMP_M) _prevDist = dist;
    if (dist > _prevDist) _prevDist = dist;   // range opening -> keep tracker current
  }

  // === BUG TONE crossing (independent trackers, fires the tone reliably) =========
  if (altCallouts && thrOK && airDesc) {
    if (_tonePrevAlt < 0.0f || fabsf(alt - _tonePrevAlt) > ALT_JUMP_M) _tonePrevAlt = alt;
    if (alt > _tonePrevAlt) _tonePrevAlt = alt;
    if (_tonePrevAlt > thr && alt <= thr) _tonePending = true;
    _tonePrevAlt = alt;
  } else if (!distCallouts) {
    _tonePrevAlt = -1.0f;
  }
  if (distCallouts && thrOK && tgtValid) {
    if (_tonePrevDist < 0.0f || fabsf(dist - _tonePrevDist) > DIST_JUMP_M) _tonePrevDist = dist;
    if (dist > _tonePrevDist) _tonePrevDist = dist;
    if (_tonePrevDist > thr && dist <= thr) _tonePending = true;
    _tonePrevDist = dist;
  }

  // === Conditions (every frame so held/latch state stays correct) ================
  bool gearUpCond = warningsOn && !state.gear_on && isAloft && alt > 0.0f;
  bool gearCond   = gearUpCond && alt < GEAR_ALT_M;                       // TOO LOW GEAR
  bool terr4Cond  = gearUpCond && alt < TERR4_ALT_M && spd > TOOLOW_SPEED_MS; // TOO LOW TERRAIN
  if (!gearCond)  _gearActive  = false;
  if (!terr4Cond) _terr4Active = false;

  bool bankCond = warningsOn && isAloft && inAtmo &&
                  fabsf(state.roll) > gpwsBankLimit(alt);
  if (!bankCond) _bankActive = false;

  bool m3Sink = warningsOn && _m3Armed && descending &&
                (_m3MaxAlt - alt) > max(M3_LOSS_MIN_M, M3_LOSS_FRAC * _m3MaxAlt);
  if (!m3Sink) _m3Active = false;

  // Mode 1 envelopes.
  bool inM1   = airDesc && alt < M1_CEIL_M;
  bool sink1  = warningsOn && inM1 && vs > (SINK_FLOOR_MS   + SINK_SLOPE   * alt);
  bool pull1  = warningsOn && inM1 && vs > (PULLUP_FLOOR_MS + PULLUP_SLOPE * alt);
  // Mode 2 envelope (terrain closure).
  bool terr2  = warningsOn && isAloft && alt > 0.0f && alt < M2_CEIL_M &&
                _closRate > (TERR_FLOOR_MS + TERR_SLOPE * alt);

  // === Priority ladder (one DFPlayer clip per frame) =============================

  // 1/2: HARD -- PULL UP (Mode 1 inner or Mode 2) with a TERRAIN entry on Mode 2.
  if (pull1 || terr2) {
    if (!_hardActive) { _hardActive = true; _terrainDone = !terr2; }
    if (terr2 && !_terrainDone) {
      gpwsPlay(GPWS_CLIP_TERRAIN); _terrainDone = true; _lastHardMs = now;
    } else if (!busy && (now - _lastHardMs) >= HARD_GAP_MS) {
      gpwsPlay(GPWS_CLIP_PULL_UP); _lastHardMs = now;
    }
    return;
  }
  _hardActive = false; _terrainDone = false;

  // 3: SINK RATE (Mode 1 outer).
  if (sink1) {
    if (!busy && (now - _lastSinkMs) >= SINK_GAP_MS) { gpwsPlay(GPWS_CLIP_SINK_RATE); _lastSinkMs = now; }
    return;
  }

  // 4: DON'T SINK (Mode 3).
  if (m3Sink) {
    if (!busy && (!_m3Active || (now - _lastM3Ms) >= MODE3_GAP_MS)) {
      gpwsPlay(GPWS_CLIP_DONT_SINK); _lastM3Ms = now; _m3Active = true;
    }
    return;
  }

  // 5: TOO LOW TERRAIN (Mode 4, high speed).
  if (terr4Cond) {
    if (!busy && (!_terr4Active || (now - _lastTerr4Ms) >= TERR4_GAP_MS)) {
      gpwsPlay(GPWS_CLIP_TOO_LOW_TERR); _lastTerr4Ms = now; _terr4Active = true;
    }
    return;
  }

  // 6: TOO LOW GEAR (Mode 4).
  if (gearCond) {
    if (!busy && (!_gearActive || (now - _lastGearMs) >= GEAR_GAP_MS)) {
      gpwsPlay(GPWS_CLIP_TOO_LOW_GEAR); _lastGearMs = now; _gearActive = true;
    }
    return;
  }

  // 7: BANK ANGLE (Mode 6).
  if (bankCond) {
    if (!busy && (!_bankActive || (now - _lastBankMs) >= BANK_GAP_MS)) {
      gpwsPlay(GPWS_CLIP_BANK_ANGLE); _lastBankMs = now; _bankActive = true;
    }
    return;
  }

  // 8: bug TONE (threshold crossing) -- above the ladder so it leads the MINIMUMS.
  if (_tonePending) {
    if (!busy) { gpwsPlay(GPWS_CLIP_TONE); _tonePending = false; }
    return;
  }

  // 9: callout ladder + MINIMUMS.
  if (altCallouts && airDesc && _prevAlt > 0.0f) {
    uint8_t clip = gpwsCrossedClip(ALT_LADDER, ALT_LADDER_COUNT, _prevAlt, alt,
                                   thr, thrOK, GPWS_CLIP_MINIMUMS);
    if (clip != 0) {
      if (!busy) { gpwsPlay(clip); _prevAlt = alt; }
      return;
    }
    _prevAlt = alt;
  } else if (distCallouts && tgtValid && _prevDist > 0.0f && dist < _prevDist) {
    uint8_t clip = gpwsCrossedClip(DIST_LADDER, DIST_LADDER_COUNT, _prevDist, dist,
                                   thr, false, 0);   // no MINIMUMS voice for range
    if (clip != 0) {
      if (!busy) { gpwsPlay(clip); _prevDist = dist; }
      return;
    }
    _prevDist = dist;
  } else {
    _prevAlt = alt;
  }
}
