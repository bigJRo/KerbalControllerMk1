#ifndef KERBAL_DISPLAY_AUDIO_H
#define KERBAL_DISPLAY_AUDIO_H

/***************************************************************************************
   KerbalDisplayAudio Library
   Non-blocking audio feedback for Kerbal Controller Mk1 displays.
   Provides alert chirps, caution tones, and master alarm two-tone loop.
   All timing is millis()-based — no delay() calls.

   State machine (priority high → low):
     AUDIO_MASTER_ALARM  — two-tone alternating loop, runs until condition clears or silenced
     AUDIO_CAUTION_TONE  — single constant tone, fixed duration
     AUDIO_CHIRP         — two-note ascending or descending sequence, plays once
     AUDIO_IDLE          — silent

   Dependencies: Arduino.h (tone/noTone)

   Licensed under the GNU General Public License v3.0 (GPL-3.0).
   Final code written by J. Rostoker for Jeb's Controller Works.
   Version: 1.0.0
****************************************************************************************/

#include <Arduino.h>

/***************************************************************************************
   PIN CONFIGURATION
   Override before including this header if needed.
****************************************************************************************/
#ifndef AUDIO_PIN
  #define AUDIO_PIN 9
#endif

/***************************************************************************************
   CHIRP FREQUENCIES AND TIMING
   Alert chirp: 2 ascending notes (positive milestone)
   Caution chirp: 2 descending notes (tritone, instinctively unsettling)
****************************************************************************************/
#ifndef AUDIO_CHIRP_ALERT_LO
  #define AUDIO_CHIRP_ALERT_LO    880   // Hz — alert chirp first note (A5)
#endif
#ifndef AUDIO_CHIRP_ALERT_HI
  #define AUDIO_CHIRP_ALERT_HI   1109   // Hz — alert chirp second note (C#6)
#endif
#ifndef AUDIO_CHIRP_CAUTION_HI
  #define AUDIO_CHIRP_CAUTION_HI 1200   // Hz — caution chirp first note (tritone)
#endif
#ifndef AUDIO_CHIRP_CAUTION_LO
  #define AUDIO_CHIRP_CAUTION_LO  849   // Hz — caution chirp second note (tritone)
#endif
#ifndef AUDIO_CHIRP_NOTE_MS
  #define AUDIO_CHIRP_NOTE_MS    120    // ms per note (240ms total per chirp)
#endif

/***************************************************************************************
   CAUTION CONSTANT TONE
****************************************************************************************/
#ifndef AUDIO_CAUTION_TONE_FREQ
  #define AUDIO_CAUTION_TONE_FREQ 1000  // Hz — classic attention tone
#endif
#ifndef AUDIO_CAUTION_TONE_MS
  #define AUDIO_CAUTION_TONE_MS  1200   // ms duration
#endif

/***************************************************************************************
   MASTER ALARM TWO-TONE LOOP
   Space Shuttle C/W specification: 375Hz / 1000Hz alternating at 2.5Hz
****************************************************************************************/
#ifndef AUDIO_ALARM_FREQ_LO
  #define AUDIO_ALARM_FREQ_LO   375    // Hz — Space Shuttle C/W low tone
#endif
#ifndef AUDIO_ALARM_FREQ_HI
  #define AUDIO_ALARM_FREQ_HI  1000    // Hz — Space Shuttle C/W high tone
#endif
#ifndef AUDIO_ALARM_PHASE_MS
  #define AUDIO_ALARM_PHASE_MS  200    // ms per phase — 2.5Hz alternation
#endif

/***************************************************************************************
   MASTER ALARM CONDITION BITS
   Pass to audioMasterAlarm() to track individual conditions independently.
   The alarm tone plays while any bit is set and stops when all clear.
****************************************************************************************/
#define AUDIO_ALARM_GROUND_PROX  (1 << 0)
#define AUDIO_ALARM_HIGH_G       (1 << 1)
#define AUDIO_ALARM_BUS_VOLTAGE  (1 << 2)
#define AUDIO_ALARM_HIGH_TEMP    (1 << 3)
#define AUDIO_ALARM_LOW_DV       (1 << 4)

/***************************************************************************************
   AUDIO STATE ENUM
****************************************************************************************/
enum AudioState : uint8_t {
  AUDIO_IDLE         = 0,
  AUDIO_CHIRP        = 1,  // two-note sequence, plays once
  AUDIO_CAUTION_TONE = 2,  // constant tone, fixed duration
  AUDIO_MASTER_ALARM = 3,  // two-tone loop until condition clears or silenced
};

/***************************************************************************************
   PUBLIC API
****************************************************************************************/

// Initialise audio pin. Call once from setup().
void setupAudio();

// Service audio timing. Call every loop() iteration.
// Fast-returns immediately if audio is idle.
void updateAudio();

// Alert chirp — 2 ascending notes. Suppressed if master alarm is active or silenced.
// Call when a positive threshold is crossed (altitude, velocity, orbital insertion).
void audioAlertChirp();

// Caution chirp — 2 descending notes (tritone). Suppressed if master alarm is active or silenced.
// Call when a caution condition is newly set (descent, entering atmosphere).
void audioCautionChirp();

// Caution constant tone — fixed duration. Suppressed if master alarm is active or silenced.
// Call when ALT caution indicator is newly set.
void audioCautionTone();

// Notify of a master alarm condition change.
//   condBit: one of the AUDIO_ALARM_* bits identifying the condition.
//   on=true:  condition became active — starts alarm if not already silenced.
//   on=false: condition cleared — stops alarm only when ALL conditions clear.
// A new condition activating while alarm is silenced will restart the tone.
void audioMasterAlarm(uint8_t condBit, bool on);

// Silence the master alarm immediately (e.g. master alarm button pressed).
// The alarm will not restart until all conditions clear and one re-triggers.
void audioSilence();

// Returns the current audio state.
AudioState audioGetState();

#endif // KERBAL_DISPLAY_AUDIO_H
