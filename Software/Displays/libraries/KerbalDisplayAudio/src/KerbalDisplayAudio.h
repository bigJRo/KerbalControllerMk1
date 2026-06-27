#ifndef KERBAL_DISPLAY_AUDIO_H
#define KERBAL_DISPLAY_AUDIO_H

/***************************************************************************************
   KerbalDisplayAudio Library
   Non-blocking audio feedback for Kerbal Controller Mk1 displays.
   Provides alert chirps, caution tones, and master alarm two-tone loop.
   All timing is millis()-based — no delay() calls.

   State machine (priority high → low):
     AUDIO_MASTER_ALARM  — two-tone alternating loop, started/stopped by the sketch
     AUDIO_CAUTION_TONE  — single constant tone, fixed duration
     AUDIO_CHIRP         — two-note ascending or descending sequence, plays once
     AUDIO_IDLE          — silent

   Master alarm condition tracking (which warnings are active, silence latch,
   re-trigger logic) is the responsibility of the calling sketch, not this library.
   The library only drives the tone: audioStartAlarm() / audioStopAlarm() / audioSilence().

   Dependencies: Arduino.h (tone/noTone)

   Licensed under the GNU General Public License v3.0 (GPL-3.0).
   Final code written by J. Rostoker for Jeb's Controller Works.
   Version: 1.0.2
****************************************************************************************/

/***************************************************************************************
   LIBRARY VERSION
   Follows the same MAJOR.MINOR.PATCH scheme used by all KCMk1 sketches.
****************************************************************************************/
#define KERBAL_DISPLAY_AUDIO_VERSION_MAJOR 1
#define KERBAL_DISPLAY_AUDIO_VERSION_MINOR 0
#define KERBAL_DISPLAY_AUDIO_VERSION_PATCH 2

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
   AUDIO STATE ENUM
****************************************************************************************/
enum AudioState : uint8_t {
  AUDIO_IDLE                  = 0,
  AUDIO_CHIRP                 = 1,  // two-note sequence, plays once
  AUDIO_CAUTION_TONE          = 2,  // constant tone, fixed duration
  AUDIO_MASTER_ALARM          = 3,  // two-tone loop until condition clears or silenced
  AUDIO_MASTER_ALARM_SILENCED = 4,  // alarm latched, tone off (crew acknowledged)
};

/***************************************************************************************
   PUBLIC API
****************************************************************************************/

// Initialise audio pin. Call once from setup().
void setupAudio();

// Service audio timing. Call every loop() iteration.
// Fast-returns immediately if audio is idle.
void updateAudio();

// Alert chirp — 2 ascending notes. Suppressed if master alarm is active.
// Call when a positive threshold is crossed (altitude, velocity, orbital insertion).
void audioAlertChirp();

// Caution chirp — 2 descending notes (tritone). Suppressed if master alarm is active.
// Call when a caution condition is newly set (descent, entering atmosphere).
void audioCautionChirp();

// Caution constant tone — fixed duration. Suppressed if master alarm is active.
// Call when ALT caution indicator is newly set.
void audioCautionTone();

// Start the master alarm two-tone loop. Has no effect if already running.
// Call when the sketch's alarm condition mask transitions from 0 to non-zero.
void audioStartAlarm();

// Stop the master alarm and return to idle. Has no effect if not running.
// Call when the sketch's alarm condition mask transitions from non-zero to 0.
void audioStopAlarm();

// Silence the master alarm tone without clearing the latch.
// Transitions AUDIO_MASTER_ALARM -> AUDIO_MASTER_ALARM_SILENCED. Chirps and
// caution tones remain suppressed so non-critical audio doesn't play while
// alarm conditions are still active. Call audioStopAlarm() when conditions
// actually clear, or audioStartAlarm() to resume the tone.
// Has no effect unless the master alarm is currently active.
void audioSilence();

// Returns the current audio state.
AudioState audioGetState();

#endif // KERBAL_DISPLAY_AUDIO_H
