/***************************************************************************************
   GPWS.ino -- Ground Proximity Warning System function for the KCMk1 Annunciator

   WHAT THIS IS
   An aviation-style GPWS that runs entirely on the Annunciator's Teensy 4.1. It
   watches the Simpit telemetry already collected in `state` (surface altitude,
   vertical speed, gear, situation, and roll/bank from ROTATION_DATA) and speaks voice
   callouts through the DFPlayer Mini on the 7" board (Serial2). It is INDEPENDENT of
   the tone() master-alarm state
   machine in KerbalDisplayAudio: the tone alarm still owns CW_GROUND_PROX and the
   other WARNING tones; GPWS voice is layered on top through the separate DFPlayer
   audio path. The two can sound together.

   CONFIGURATION SOURCE
   The GPWS is configured by four parameters that ORIGINATE on the GPWS Input Panel
   module (KC-01-1880, I2C 0x2A) and are relayed to the Annunciator by the master
   controller inside the inbound I2C command (see I2CSlave.ino, rev-3 9-byte form).
   The three-state / two-flag config byte uses the SAME bit layout the GPWS Input
   module already reports in its packet, so the master can pass that byte straight
   through:
       bits 1:0  gpwsMode   0=OFF, 1=ACTIVE (full), 2=PROX (proximity only)
       bit  2    proxAlarm  arms the imminent-impact TERRAIN/PULL UP hard warning
       bit  3    rdvRadar   rendezvous radar (reserved -- see note below)
   plus an int16 altitude threshold (metres, the GPWS decision height / "MINIMUMS").

   MODE SEMANTICS
     OFF     -- GPWS fully inhibited, DFPlayer silent.
     ACTIVE  -- full suite: descending altitude callouts, MINIMUMS, SINK RATE,
                DON'T SINK (altitude loss after takeoff), TOO LOW GEAR, BANK ANGLE,
                plus TERRAIN/PULL UP when proxAlarm is armed.
     PROX    -- proximity only: TERRAIN/PULL UP hard warning when proxAlarm is
                armed; every soft callout (altitude ladder, sink rate, don't sink,
                gear, bank angle) is suppressed. Matches the GPWS Input "proximity
                tone only" amber state.

   proxAlarm  gates the hard TERRAIN/PULL UP warning in BOTH ACTIVE and PROX modes.
              On the GPWS Input panel BTN02 arms it (and, pressed while ACTIVE,
              forces the panel into PROX with proxAlarm on).
   rdvRadar   is stored and logged but drives no callouts yet: rendezvous callouts
              need a target-range Simpit channel the Annunciator does not subscribe
              to. Reserved so the wire format is stable when that arrives.

   AUDIO ENGINE
   The DFPlayer is fire-and-forget over a 9600-baud UART with a hardware BUSY line
   (KCM_AUDIO_BUSY_PIN, pin 11): BUSY low = a clip is playing. gpwsUpdate() is fully
   non-blocking -- it never calls delay(); recurring warnings re-arm from millis()
   cadence timers and the BUSY line. Hard warnings (TERRAIN/PULL UP) preempt any soft
   clip already playing; soft callouts only start when the player is idle, so a
   higher-priority warning is never talked over.

   Priority (high -> low): TERRAIN/PULL UP > DON'T SINK > SINK RATE > TOO LOW GEAR >
   BANK ANGLE > MINIMUMS > altitude ladder. Exactly one condition owns the audio each
   frame.

   COVERAGE vs A REAL GPWS (conscious scope decisions)
   A classic ground-proximity warning computer has seven modes plus the EGPWS/TAWS
   forward-looking additions. KSP/KerbalSimpit exposes only surface altitude, vertical
   speed, surface speed, gear and situation -- there is no forward-looking terrain,
   ILS glideslope, flap position, roll angle or wind. So we implement the modes those
   inputs can support and DELIBERATELY OMIT the rest rather than fake them:
     Mode 1  Excessive descent rate ....... IMPLEMENTED. SINK RATE (altitude-scaled
                                            rate boundary) and the hard PULL UP envelope.
     Mode 2  Excessive terrain closure ..... ADAPTED/FOLDED. No forward-looking terrain,
                                            so closure is approximated by time-to-impact
                                            on surface altitude; "TERRAIN" is the hard-
                                            warning entry annunciation before PULL UP.
     Mode 3  Altitude loss after takeoff ... IMPLEMENTED ("DON'T SINK"). Armed on
                                            liftoff, disarmed once past the climbout
                                            ceiling; fires on net altitude loss from
                                            the post-takeoff peak.
     Mode 4  Unsafe terrain clearance ...... PARTIAL. 4A TOO LOW GEAR implemented
                                            (recurring while in the envelope). 4B TOO
                                            LOW FLAPS omitted (no flap state in Simpit);
                                            4C climb-clearance term omitted.
     Mode 5  Below glideslope .............. NOT IMPLEMENTED ("GLIDESLOPE"). No ILS.
     Mode 6  Advisory callouts ............. IMPLEMENTED. Altitude ladder + MINIMUMS,
                                            and BANK ANGLE (excessive roll in atmo,
                                            from ROTATION_DATA).
     Mode 7  Windshear .................... NOT IMPLEMENTED. No wind/windshear data.
     EGPWS   Forward-looking terrain (TCF/ .. NOT IMPLEMENTED. No terrain database or
             TAD), "TERRAIN AHEAD" .......... GPS look-ahead in KSP.
   See the coverage table in KCMk1_Annunciator/README.md for the same summary.

   CALLOUT CADENCE vs A REAL GPWS
   Real-system behaviour we match, and where we deliberately differ:
     - Altitude callouts / MINIMUMS: spoken ONCE per descent through the level, never
       repeated. Matches Mode 6 advisory behaviour exactly.
     - PULL UP (Mode 1/2 hard): a real "WHOOP WHOOP PULL UP" repeats essentially
       gaplessly while in the envelope. We replay every GPWS_HARD_GAP_MS (~ one clip
       length) gated on the BUSY line, giving a near-continuous repeat with no overlap.
     - SINK RATE (Mode 1), DON'T SINK (Mode 3), TOO LOW GEAR (Mode 4A) and BANK ANGLE
       (Mode 6): real systems re-annunciate roughly every 1-1.5 s while the vessel
       stays in the envelope. Each repeats on its own GPWS_*_GAP_MS cadence, firing
       immediately on entry (the *_Active flag is false) and then on the cadence timer,
       always gated on the BUSY line so clips never overlap.

   The DFPlayer path plays numbered clips from ITS OWN microSD card (not the Teensy
   BMP card). The clip-number map is the GPWS_CLIP_* enum below and is documented in
   KCMk1_Annunciator/README.md (folder /01, tracks 001..016).

   All GPWS timing/altitude tunables live in AAA_Config.ino (GPWS_* constants) so the
   behaviour can be flight-tuned without touching this logic.

   Licensed under the GNU General Public License v3.0 (GPL-3.0).
   Final code written by Jason Rostoker for Jeb's Controller Works.
****************************************************************************************/
#include "KCMk1_Annunciator.h"


/***************************************************************************************
   DFPLAYER INSTANCE
   Owned here -- the GPWS function is the only consumer of sampled audio on the
   Annunciator. Serial2 and the BUSY pin come from KCMk1_SystemConfig.
****************************************************************************************/
static KCM_DFPlayer gpwsDfp(KCM_DFPLAYER_SERIAL);


/***************************************************************************************
   CLIP NUMBER MAP (DFPlayer folder /01, tracks 001..016)
   These are the track numbers of the voice clips on the DFPlayer microSD card.
   The spoken text for each is documented in KCMk1_Annunciator/README.md.
****************************************************************************************/
#define GPWS_FOLDER            1     // /01 on the DFPlayer card

#define GPWS_CLIP_PULL_UP      1     // "PULL UP"            (hard, recurring)
#define GPWS_CLIP_TERRAIN      2     // "TERRAIN, TERRAIN"   (hard, entry)
#define GPWS_CLIP_SINK_RATE    3     // "SINK RATE"          (caution, recurring)
#define GPWS_CLIP_TOO_LOW_GEAR 4     // "TOO LOW, GEAR"      (caution, once)
#define GPWS_CLIP_MINIMUMS     5     // "MINIMUMS"           (once, at threshold)
#define GPWS_CLIP_1000         6     // "ONE THOUSAND"
#define GPWS_CLIP_500          7     // "FIVE HUNDRED"
#define GPWS_CLIP_400          8     // "FOUR HUNDRED"
#define GPWS_CLIP_300          9     // "THREE HUNDRED"
#define GPWS_CLIP_200         10     // "TWO HUNDRED"
#define GPWS_CLIP_100         11     // "ONE HUNDRED"
#define GPWS_CLIP_50          12     // "FIFTY"
#define GPWS_CLIP_40          13     // "FORTY"
#define GPWS_CLIP_30          14     // "THIRTY"
#define GPWS_CLIP_20          15     // "TWENTY"
#define GPWS_CLIP_10          16     // "TEN"
#define GPWS_CLIP_DONT_SINK   17     // "DON'T SINK"     (Mode 3, recurring)
#define GPWS_CLIP_BANK_ANGLE  18     // "BANK ANGLE"     (Mode 6, recurring)


/***************************************************************************************
   ALTITUDE CALLOUT LADDER
   Fixed radio-altimeter callout rungs (metres AGL). As the vessel descends through
   a rung, its number is spoken once. A rung within GPWS_MIN_DEDUP_M of the configured
   threshold is skipped in favour of the "MINIMUMS" callout at the threshold itself.
****************************************************************************************/
struct GpwsRung { float altM; uint8_t clip; };
static const GpwsRung GPWS_LADDER[] = {
  { 1000.0f, GPWS_CLIP_1000 },
  {  500.0f, GPWS_CLIP_500  },
  {  400.0f, GPWS_CLIP_400  },
  {  300.0f, GPWS_CLIP_300  },
  {  200.0f, GPWS_CLIP_200  },
  {  100.0f, GPWS_CLIP_100  },
  {   50.0f, GPWS_CLIP_50   },
  {   40.0f, GPWS_CLIP_40   },
  {   30.0f, GPWS_CLIP_30   },
  {   20.0f, GPWS_CLIP_20   },
  {   10.0f, GPWS_CLIP_10   },
};
static const uint8_t GPWS_LADDER_COUNT = sizeof(GPWS_LADDER) / sizeof(GPWS_LADDER[0]);

// Sane bounds for treating the configured threshold as a spoken "MINIMUMS" rung.
// A threshold of 0 (or absurdly high) yields no minimums callout.
static const float GPWS_MIN_THRESHOLD_M = 10.0f;
static const float GPWS_MAX_THRESHOLD_M = 2000.0f;


/***************************************************************************************
   LIVE CONFIG (set by the master over I2C via gpwsSetConfig())
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
static bool     _gpwsEnabled   = false;   // last-frame enable latch (for edge-off stop)
static bool     _busyAttached  = false;   // DFPlayer BUSY pin wired and usable
static float    _prevAlt       = -1.0f;   // previous surface altitude (crossing detect)

static bool     _hardActive    = false;   // in the TERRAIN/PULL UP hard zone
static bool     _terrainDone   = false;   // entry "TERRAIN" already spoken this episode
static uint32_t _lastHardMs    = 0;       // last PULL UP cadence stamp

static uint32_t _lastSinkMs    = 0;       // last SINK RATE cadence stamp

static bool     _gearActive    = false;   // TOO LOW GEAR condition currently held
static uint32_t _lastGearMs    = 0;       // last TOO LOW GEAR cadence stamp

static bool     _bankActive    = false;   // BANK ANGLE condition currently held
static uint32_t _lastBankMs    = 0;       // last BANK ANGLE cadence stamp

// Mode 3 (DON'T SINK) -- altitude loss after takeoff.
static bool     _wasOnGround   = false;   // previous-frame on-ground state (liftoff edge)
static bool     _m3Armed       = false;   // in the post-takeoff climbout window
static float    _m3MaxAlt      = 0.0f;    // peak surface alt reached since liftoff
static bool     _m3Active      = false;   // DON'T SINK condition currently held
static uint32_t _lastM3Ms      = 0;       // last DON'T SINK cadence stamp


/***************************************************************************************
   HELPERS
****************************************************************************************/
static inline void gpwsPlay(uint8_t clip) {
  gpwsDfp.playFolderTrack(GPWS_FOLDER, clip);
  if (debugMode) {
    Serial.print(F("Annunciator: GPWS clip "));
    Serial.println(clip);
  }
}

// Clear all transient callout latches (does not touch config). Used on reset and on
// the enabled -> disabled edge so the next descent starts clean.
static void gpwsClearLatches() {
  _prevAlt     = -1.0f;
  _hardActive  = false;
  _terrainDone = false;
  _gearActive  = false;
  _bankActive  = false;
  // Require a REAL on-ground frame before a liftoff edge can arm Mode 3, so a vessel
  // loaded already airborne (no PRELAUNCH/LANDED frame) does not spuriously arm.
  _wasOnGround = false;
  _m3Armed     = false;
  _m3MaxAlt    = 0.0f;
  _m3Active    = false;
}

// True when the relayed altitude threshold is a sane decision height. Shared by the
// ladder/MINIMUMS crossing check and the TOO LOW GEAR altitude gate so both consumers
// agree on what a usable threshold is -- a garbled 0/negative or absurd value from an
// unconfigured GPWS Input panel disables both threshold-based callouts rather than
// disabling only MINIMUMS while leaving the gear warning on a bogus altitude.
static inline bool gpwsThresholdValid() {
  float thr = (float)_gpwsThreshold;
  return (thr >= GPWS_MIN_THRESHOLD_M && thr <= GPWS_MAX_THRESHOLD_M);
}

// Lowest ladder/minimums rung the vessel just descended THROUGH between prevAlt and
// alt. Returns the clip number, or 0 if no rung was crossed. "Lowest" (closest to the
// ground) wins when several rungs are crossed in one span -- so a callout deferred
// behind higher-priority audio announces the vessel's CURRENT altitude, not a backlog.
static uint8_t gpwsCrossedClip(float prevAlt, float alt) {
  uint8_t  best     = 0;
  float    bestAlt  = 1e9f;
  float    thr      = (float)_gpwsThreshold;
  bool     thrValid = gpwsThresholdValid();

  // Fixed numeric rungs (masking any rung that collides with the minimums callout).
  for (uint8_t i = 0; i < GPWS_LADDER_COUNT; i++) {
    float r = GPWS_LADDER[i].altM;
    if (thrValid && fabsf(r - thr) < GPWS_MIN_DEDUP_M) continue;  // spoken as MINIMUMS
    if (prevAlt > r && alt <= r && r < bestAlt) {
      best    = GPWS_LADDER[i].clip;
      bestAlt = r;
    }
  }

  // Minimums pseudo-rung at the configured decision height.
  if (thrValid && prevAlt > thr && alt <= thr && thr < bestAlt) {
    best = GPWS_CLIP_MINIMUMS;
  }

  return best;
}


/***************************************************************************************
   SET CONFIG
   Called from I2CSlave.ino when the master relays the GPWS Input panel state.
   cfgByte bit layout matches the GPWS Input module's reported state byte:
     bits 1:0 = mode, bit 2 = proxAlarm, bit 3 = rdvRadar.
   thresholdM is the altitude threshold in metres (the module's int16 value).
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
    Serial.print(F("Annunciator: GPWS cfg mode="));
    Serial.print(_gpwsMode);
    Serial.print(F(" prox="));
    Serial.print(_gpwsProxAlarm);
    Serial.print(F(" rdv="));
    Serial.print(_gpwsRdvRadar);
    Serial.print(F(" thr="));
    Serial.println(_gpwsThreshold);
  }
}


/***************************************************************************************
   RESET
   Flush all GPWS audio and callout state. Called from setup() and on scene exit /
   vessel switch (SimpitHandler.ino) so callouts never carry across a flight boundary.
   Config (mode/flags/threshold) is preserved -- it is owned by the master.
****************************************************************************************/
void gpwsReset() {
  gpwsDfp.stop();   // plain UART command -- does not need the BUSY sense line
  gpwsClearLatches();
  _gpwsEnabled = false;
}


/***************************************************************************************
   SETUP
   Bring up the DFPlayer and attach the BUSY line. Call once from setup().
****************************************************************************************/
void gpwsSetup() {
  gpwsDfp.begin(GPWS_VOLUME, KCM_DFPLAYER_BAUD);
  gpwsDfp.attachBusyPin(KCM_AUDIO_BUSY_PIN);
  _busyAttached = gpwsDfp.busyPinAttached();
  gpwsClearLatches();
  if (debugMode) Serial.println(F("Annunciator: GPWS ready (DFPlayer on Serial2)"));
}


/***************************************************************************************
   UPDATE
   Call every loop() pass. Evaluates GPWS conditions from `state` and drives the
   DFPlayer. Highest-priority active condition owns the audio each frame.

   CROSSING-TRACKER DISCIPLINE
   `_prevAlt` is the altitude-ladder crossing detector. The ladder only fires on
   DOWNWARD crossings, so:
     - While climbing, `_prevAlt` is bumped up to the current altitude every frame, so
       a higher-priority callout that returns early during a climb (e.g. BANK ANGLE)
       can never freeze it below the vessel and swallow the upper rungs on the next
       descent.
     - While descending, `_prevAlt` is advanced ONLY when the ladder is actually
       serviced (a rung is spoken, or none is pending). Whenever a higher-priority
       condition owns the audio, or the DFPlayer is still busy, `_prevAlt` is LEFT
       UNTOUCHED so the pending crossing survives. Because gpwsCrossedClip() returns
       the LOWEST rung crossed across the whole [_prevAlt, alt] span, a deferred
       callout announces the vessel's CURRENT altitude, not a stale backlog. This is
       why the higher-priority branches below do not write `_prevAlt`.
****************************************************************************************/
void gpwsUpdate() {
  // --- Enable gate: flight scene, audio on, GPWS not OFF -----------------------
  bool enabled = flightScene && audioEnabled && (_gpwsMode != GPWS_MODE_OFF);
  if (!enabled) {
    if (_gpwsEnabled) {                 // just became disabled -- hush and flush
      gpwsDfp.stop();                   // plain UART command -- BUSY line not required
      gpwsClearLatches();
    }
    _gpwsEnabled = false;
    return;
  }
  _gpwsEnabled = true;

  // --- Which callout classes are permitted in the current mode -----------------
  bool hardEnabled = _gpwsProxAlarm;                     // TERRAIN/PULL UP (ACTIVE or PROX)
  bool softEnabled = (_gpwsMode == GPWS_MODE_ACTIVE);    // ladder/sink/gear/minimums/mode3/bank

  // --- Flight geometry from Simpit state ---------------------------------------
  uint8_t sit    = state.vesselSituationState;
  bool isAloft   = bitRead(sit, VSIT_FLIGHT)  || bitRead(sit, VSIT_SUBORBIT) ||
                   bitRead(sit, VSIT_ORBIT)   || bitRead(sit, VSIT_ESCAPE);
  bool onGround  = bitRead(sit, VSIT_LANDED)  || bitRead(sit, VSIT_SPLASH) ||
                   bitRead(sit, VSIT_PRELAUNCH);
  float alt      = state.alt_surf;
  float vs       = fabsf(state.vel_vert);
  bool descending = (state.vel_vert < -GPWS_DESCENT_DEADBAND_MS);
  bool airDesc   = isAloft && descending && alt > 0.0f;
  float tImpact  = (airDesc && vs > 0.0f) ? (alt / vs) : 1e9f;

  // Seed / re-seed the crossing tracker. A large single-frame jump (vessel switch,
  // warp, terrain step) re-seeds without firing a spurious ladder callout.
  if (_prevAlt < 0.0f || fabsf(alt - _prevAlt) > GPWS_ALT_JUMP_M) {
    _prevAlt = alt;
  }
  // Climbing: keep the tracker at the current altitude (see CROSSING-TRACKER DISCIPLINE).
  if (alt > _prevAlt) _prevAlt = alt;

  uint32_t now = millis();
  bool busy = _busyAttached && gpwsDfp.isPlaying();

  // === Conditions evaluated EVERY frame (before any priority early-return) =======
  // Kept out of the priority branches so their "held" state and Mode-3 tracking stay
  // correct even while a higher-priority episode owns the audio.

  // Mode 3 arm/track: arm on liftoff (ground -> air), disarm once an established climb
  // clears GPWS_MODE3_CEIL_M or the vessel is back on the ground; track the peak alt.
  if (onGround) {
    _m3Armed  = false;
    _m3MaxAlt = 0.0f;
  } else if (isAloft) {
    if (_wasOnGround) { _m3Armed = true; _m3MaxAlt = alt; }   // liftoff edge
    if (alt > GPWS_MODE3_CEIL_M) _m3Armed = false;            // established climb
    if (alt > _m3MaxAlt) _m3MaxAlt = alt;                     // running peak
  }
  _wasOnGround = onGround;

  // DON'T SINK: armed climbout, descending, net loss from the post-takeoff peak.
  bool m3Sink = softEnabled && _m3Armed && descending &&
                (_m3MaxAlt - alt) > GPWS_MODE3_LOSS_M;
  if (!m3Sink) _m3Active = false;

  // TOO LOW GEAR: airborne descending, gear up, below the (valid) decision height.
  bool gearCond = airDesc && !state.gear_on &&
                  gpwsThresholdValid() && alt < (float)_gpwsThreshold;
  if (!gearCond) _gearActive = false;

  // BANK ANGLE: excessive roll in atmospheric flight (not descent-gated).
  bool bankCond = softEnabled && isAloft && inAtmo &&
                  fabsf(state.roll) > GPWS_BANK_DEG;
  if (!bankCond) _bankActive = false;

  // SINK RATE: excessive descent RATE for the current altitude (Mode-1 style), not
  // "any descent". Only near the ground; the allowed rate scales with altitude so a
  // normal approach does not nag but a steep sink does.
  bool sinkExcessive = softEnabled && airDesc && alt < GPWS_SINK_CEIL_M &&
                       vs > (GPWS_SINK_RATE_FLOOR_MS + alt * GPWS_SINK_RATE_SLOPE);

  // === Priority ladder -- exactly one condition owns the audio this frame ========

  // --- Priority 1: HARD warning -- TERRAIN then recurring PULL UP ---------------
  if (hardEnabled && airDesc && tImpact < GPWS_PULLUP_S) {
    if (!_hardActive) { _hardActive = true; _terrainDone = false; }
    if (!_terrainDone) {
      gpwsPlay(GPWS_CLIP_TERRAIN);     // entry annunciation preempts any soft clip
      _terrainDone = true;
      _lastHardMs  = now;
    } else if (!busy && (now - _lastHardMs) >= GPWS_HARD_GAP_MS) {
      gpwsPlay(GPWS_CLIP_PULL_UP);
      _lastHardMs = now;
    }
    return;                            // defer ladder -- do not advance _prevAlt
  }
  _hardActive  = false;
  _terrainDone = false;

  // Below here, everything is a soft callout -- only in ACTIVE mode.
  // Keep the tracker current so no stale backlog builds while soft callouts are off.
  if (!softEnabled) { _prevAlt = alt; return; }

  // --- Priority 2: DON'T SINK (Mode 3, recurring) ------------------------------
  if (m3Sink) {
    if (!busy && (!_m3Active || (now - _lastM3Ms) >= GPWS_MODE3_GAP_MS)) {
      gpwsPlay(GPWS_CLIP_DONT_SINK);
      _lastM3Ms  = now;
      _m3Active  = true;
    }
    return;                            // defer ladder -- do not advance _prevAlt
  }

  // --- Priority 3: SINK RATE (Mode 1, recurring) -------------------------------
  if (sinkExcessive) {
    if (!busy && (now - _lastSinkMs) >= GPWS_SINK_GAP_MS) {
      gpwsPlay(GPWS_CLIP_SINK_RATE);
      _lastSinkMs = now;
    }
    return;                            // defer ladder -- do not advance _prevAlt
  }

  // --- Priority 4: TOO LOW GEAR (Mode 4A, recurring while in envelope) ----------
  if (gearCond) {
    if (!busy && (!_gearActive || (now - _lastGearMs) >= GPWS_GEAR_GAP_MS)) {
      gpwsPlay(GPWS_CLIP_TOO_LOW_GEAR);
      _lastGearMs = now;
      _gearActive = true;
    }
    return;                            // supersedes the ladder -- do not advance _prevAlt
  }

  // --- Priority 5: BANK ANGLE (Mode 6, recurring while over-banked) -------------
  if (bankCond) {
    if (!busy && (!_bankActive || (now - _lastBankMs) >= GPWS_BANK_GAP_MS)) {
      gpwsPlay(GPWS_CLIP_BANK_ANGLE);
      _lastBankMs = now;
      _bankActive = true;
    }
    return;                            // defer ladder -- do not advance _prevAlt
  }

  // --- Priority 6: altitude ladder + MINIMUMS (descent crossings) --------------
  if (airDesc && _prevAlt > 0.0f) {
    uint8_t clip = gpwsCrossedClip(_prevAlt, alt);
    if (clip != 0) {
      // Advance past the rung ONLY when the callout is actually spoken. If the player
      // is busy, leave _prevAlt so the crossing is retried on the next idle frame
      // (gpwsCrossedClip will then pick the lowest still-relevant rung).
      if (!busy) {
        gpwsPlay(clip);
        _prevAlt = alt;
      }
      return;
    }
  }

  // No pending callout -- track altitude normally.
  _prevAlt = alt;
}
