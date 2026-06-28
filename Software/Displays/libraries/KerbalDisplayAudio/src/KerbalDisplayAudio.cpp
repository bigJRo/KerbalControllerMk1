#include "KerbalDisplayAudio.h"

/***************************************************************************************
   KerbalDisplayAudio Library — Implementation
   Non-blocking audio state machine for KCMk1 display panels.
   All public API and configuration defines are in KerbalDisplayAudio.h.
****************************************************************************************/

static AudioState _audioState      = AUDIO_IDLE;
static uint32_t   _audioPhaseStart = 0;
static bool       _chirpSecondNote = false;
static bool       _chirpAscending  = true;
static bool       _alarmHiPhase    = true;

// True while a master-alarm condition is latched but the crew has silenced the
// tone. Decoupled from _audioState so lower-priority chirps and caution tones
// may play while silenced, then return to the silenced (tone-off) state rather
// than IDLE — the latch is preserved without blocking other cues (issue #12).
static bool       _alarmSilenced   = false;

// Chirps and caution tones are suppressed ONLY while the master alarm is
// audibly sounding. A silenced master alarm does NOT block them (issue #12):
// a silenced alarm no longer holds higher priority than other tones.
static inline bool _alarmAudible() {
  return _audioState == AUDIO_MASTER_ALARM;
}

// Where a one-shot cue (chirp / caution tone) returns when it finishes: back to
// the silenced master-alarm latch if one is held, otherwise idle.
static inline AudioState _restState() {
  return _alarmSilenced ? AUDIO_MASTER_ALARM_SILENCED : AUDIO_IDLE;
}

// Internal: start a chirp sequence (used by audioAlertChirp and audioCautionChirp)
static void _startChirp(bool ascending) {
  _chirpAscending  = ascending;
  _chirpSecondNote = false;
  _audioPhaseStart = millis();
  _audioState      = AUDIO_CHIRP;
  tone(AUDIO_PIN, ascending ? AUDIO_CHIRP_ALERT_LO : AUDIO_CHIRP_CAUTION_HI);
}

void setupAudio() {
  pinMode(AUDIO_PIN, OUTPUT);
  noTone(AUDIO_PIN);
}

void updateAudio() {
  if (_audioState == AUDIO_IDLE) return;
  uint32_t now = millis();

  switch (_audioState) {

    case AUDIO_CHIRP:
      if (!_chirpSecondNote) {
        if (now - _audioPhaseStart >= AUDIO_CHIRP_NOTE_MS) {
          _chirpSecondNote = true;
          _audioPhaseStart = now;
          tone(AUDIO_PIN, _chirpAscending ? AUDIO_CHIRP_ALERT_HI : AUDIO_CHIRP_CAUTION_LO);
        }
      } else {
        if (now - _audioPhaseStart >= AUDIO_CHIRP_NOTE_MS) {
          noTone(AUDIO_PIN);
          _audioState = _restState();  // back to silenced latch, or idle
        }
      }
      break;

    case AUDIO_CAUTION_TONE:
      if (now - _audioPhaseStart >= AUDIO_CAUTION_TONE_MS) {
        noTone(AUDIO_PIN);
        _audioState = _restState();  // back to silenced latch, or idle
      }
      break;

    case AUDIO_MASTER_ALARM:
      if (now - _audioPhaseStart >= AUDIO_ALARM_PHASE_MS) {
        _audioPhaseStart = now;
        _alarmHiPhase    = !_alarmHiPhase;
        tone(AUDIO_PIN, _alarmHiPhase ? AUDIO_ALARM_FREQ_HI : AUDIO_ALARM_FREQ_LO);
      }
      break;

    case AUDIO_MASTER_ALARM_SILENCED:
      // Latch held but tone is off — nothing to service. Lower-priority cues
      // are NOT suppressed in this state (see _alarmAudible()); when one plays
      // it returns here via _restState() once it finishes.
      break;

    case AUDIO_IDLE:
      break;  // handled by early-return above; case listed to silence -Wswitch
  }
}

void audioAlertChirp() {
  if (_alarmAudible()) return;
  _startChirp(true);
}

void audioCautionChirp() {
  if (_alarmAudible()) return;
  _startChirp(false);
}

void audioCautionTone() {
  if (_alarmAudible()) return;
  noTone(AUDIO_PIN);
  _audioPhaseStart = millis();
  _audioState      = AUDIO_CAUTION_TONE;
  tone(AUDIO_PIN, AUDIO_CAUTION_TONE_FREQ);
}

void audioStartAlarm() {
  if (_audioState == AUDIO_MASTER_ALARM) return;
  // Resume from silenced state, or start fresh from idle/lower-priority state.
  // Becoming audible clears any silence latch.
  _alarmSilenced   = false;
  _alarmHiPhase    = true;
  _audioPhaseStart = millis();
  _audioState      = AUDIO_MASTER_ALARM;
  tone(AUDIO_PIN, AUDIO_ALARM_FREQ_HI);
}

// audioStopAlarm() — call when ALL alarm CONDITIONS have cleared. Transitions
// either AUDIO_MASTER_ALARM or AUDIO_MASTER_ALARM_SILENCED to AUDIO_IDLE.
// Distinct from audioSilence(): stopAlarm ends the alarm latch entirely; silence
// only quiets the tone while keeping the latch held.
void audioStopAlarm() {
  // Clear the master-alarm latch entirely.
  _alarmSilenced = false;
  if (_audioState == AUDIO_MASTER_ALARM ||
      _audioState == AUDIO_MASTER_ALARM_SILENCED) {
    noTone(AUDIO_PIN);
    _audioState = AUDIO_IDLE;
  }
  // If a chirp/caution cue is sounding when conditions clear, it keeps playing
  // and returns to IDLE on completion (the latch is now cleared).
}

// audioSilence() — immediately stops the tone but keeps the master alarm latch
// held by transitioning AUDIO_MASTER_ALARM -> AUDIO_MASTER_ALARM_SILENCED.
// Use when the crew acknowledges an active alarm without conditions clearing.
// Chirps/tones remain suppressed. Call audioStopAlarm() when conditions actually
// clear, or audioStartAlarm() to resume the audible alarm.
// Has no effect unless the master alarm is currently audible.
void audioSilence() {
  if (_audioState == AUDIO_MASTER_ALARM) {
    noTone(AUDIO_PIN);
    _alarmSilenced = true;
    _audioState    = AUDIO_MASTER_ALARM_SILENCED;
  }
}

AudioState audioGetState() {
  return _audioState;
}
