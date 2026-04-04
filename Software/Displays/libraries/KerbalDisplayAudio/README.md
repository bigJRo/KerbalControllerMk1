# KerbalDisplayAudio

**Kerbal Controller Mk1 — Audio Feedback Library** · v1.0.1
Non-blocking audio state machine for RA8875-based KSP controller display panels.
Part of the KCMk1 controller system.

---

## Overview

KerbalDisplayAudio provides audio feedback for KCMk1 display panels via the Arduino `tone()` API. It manages a priority-ordered state machine of four audio modes — master alarm, caution tone, caution chirp, and alert chirp — with all timing driven by `millis()` so that `loop()` never blocks.

The master alarm tone is modelled on the Space Shuttle Caution & Warning specification: 375 Hz / 1000 Hz alternating at 2.5 Hz. The library drives the tone on command via `audioStartAlarm()` and `audioStopAlarm()`. Condition tracking — which warnings are active, the silence latch, and re-trigger logic — is the responsibility of the calling sketch, not the library.

---

## Hardware

| Pin | Define | Default | Function |
|-----|--------|---------|----------|
| 9 | `AUDIO_PIN` | `9` | Piezo buzzer or speaker output |

To override the pin, define it before including the library:

```cpp
#define AUDIO_PIN 8
#include <KerbalDisplayAudio.h>
```

---

## Dependencies

| Dependency | Notes |
|------------|-------|
| `Arduino.h` | `tone()` / `noTone()` — available on all Arduino-compatible targets |

No external libraries required.

---

## Configuration

All frequency and timing constants are overrideable. Define them before the `#include`.

| Constant | Default | Description |
|----------|---------|-------------|
| `AUDIO_PIN` | `9` | Output pin for `tone()` |
| `AUDIO_CHIRP_ALERT_LO` | `880` Hz | Alert chirp first note (A5) |
| `AUDIO_CHIRP_ALERT_HI` | `1109` Hz | Alert chirp second note (C#6) |
| `AUDIO_CHIRP_CAUTION_HI` | `1200` Hz | Caution chirp first note (tritone) |
| `AUDIO_CHIRP_CAUTION_LO` | `849` Hz | Caution chirp second note (tritone) |
| `AUDIO_CHIRP_NOTE_MS` | `120` ms | Duration of each chirp note (240 ms total per chirp) |
| `AUDIO_CAUTION_TONE_FREQ` | `1000` Hz | Caution constant tone frequency |
| `AUDIO_CAUTION_TONE_MS` | `1200` ms | Caution constant tone duration |
| `AUDIO_ALARM_FREQ_LO` | `375` Hz | Master alarm low tone (Space Shuttle C/W spec) |
| `AUDIO_ALARM_FREQ_HI` | `1000` Hz | Master alarm high tone (Space Shuttle C/W spec) |
| `AUDIO_ALARM_PHASE_MS` | `200` ms | Duration of each alarm phase (2.5 Hz alternation) |

---

## Features

### State Machine

The audio state machine runs four modes in priority order (high to low):

| Priority | State | Description |
|----------|-------|-------------|
| 1 | `AUDIO_MASTER_ALARM` | Two-tone alternating loop — runs until all conditions clear or silenced |
| 2 | `AUDIO_CAUTION_TONE` | Constant tone for a fixed duration |
| 3 | `AUDIO_CHIRP` | Two-note sequence (ascending or descending), plays once |
| 4 | `AUDIO_IDLE` | Silent |

Lower-priority sounds are suppressed while a higher-priority state is active. Chirps and caution tones are suppressed whenever the state machine is in `AUDIO_MASTER_ALARM` — this includes both the sounding and silenced variants of that state. A silenced alarm still holds `AUDIO_MASTER_ALARM`; `noTone()` has been called but the state has not transitioned to `AUDIO_IDLE`.

### Master Alarm

The library drives the master alarm tone but has no knowledge of which conditions are active or why. Condition tracking, the silence latch, and re-trigger logic all live in the calling sketch (`Audio.ino` in the KCMk1 Annunciator). The sketch calls `audioStartAlarm()` when the first condition becomes active and `audioStopAlarm()` when the last condition clears.

The sketch is responsible for:
- Maintaining a bitmask of which alarm conditions are currently active
- Calling `audioStartAlarm()` when the mask transitions from 0 to non-zero
- Calling `audioStopAlarm()` when the mask transitions from non-zero to 0
- Managing a silence latch that prevents restart for existing conditions after `audioSilence()`
- Restarting the alarm (and clearing the latch) if a new condition fires while silenced

### API

`setupAudio()` — configures `AUDIO_PIN` as output and silences it. Call once from `setup()`.

`updateAudio()` — services all audio timing. Call every `loop()` iteration. Returns immediately if audio is idle.

`audioAlertChirp()` — plays a two-note ascending sequence (A5 → C#6). Signals a positive event such as an altitude or velocity threshold crossing, or orbital insertion. Suppressed if master alarm is active.

`audioCautionChirp()` — plays a two-note descending sequence (tritone interval). Signals a newly-set caution condition such as entering atmosphere or beginning descent. Suppressed if master alarm is active.

`audioCautionTone()` — plays a constant 1000 Hz tone for 1200 ms. Used for the ALT caution condition. Suppressed if master alarm is active.

`audioStartAlarm()` — starts the two-tone master alarm loop. Has no effect if already running. Call when the sketch's alarm condition mask transitions from 0 to non-zero.

`audioStopAlarm()` — stops the master alarm and returns to `AUDIO_IDLE`. Has no effect if not running. Call when the sketch's alarm condition mask transitions from non-zero to 0. Distinct from `audioSilence()`: this clears the alarm state entirely; silence only quiets the tone while keeping `AUDIO_MASTER_ALARM` active.

`audioSilence()` — immediately stops all audio and transitions to `AUDIO_IDLE` unconditionally. Use when the crew presses the master alarm button, or when audio is disabled at runtime. Unlike `audioStopAlarm()`, this works regardless of the current audio state. The sketch must manage its own silence latch separately.

`audioGetState()` — returns the current `AudioState` enum value (`AUDIO_IDLE`, `AUDIO_CHIRP`, `AUDIO_CAUTION_TONE`, or `AUDIO_MASTER_ALARM`).

---

## Usage

```cpp
#include <KerbalDisplayAudio.h>

// Sketch-side alarm condition tracking (application-specific)
static const uint8_t ALARM_HIGH_TEMP = (1 << 0);
static const uint8_t ALARM_LOW_DV    = (1 << 1);
static uint8_t alarmMask     = 0;
static bool    alarmSilenced = false;

void setAlarmCondition(uint8_t bit, bool on) {
  if (on) {
    bool wasActive = (alarmMask != 0);
    alarmMask |= bit;
    if (!wasActive && !alarmSilenced) {
      audioStartAlarm();
    } else if (wasActive && alarmSilenced) {
      alarmSilenced = false;   // new condition overrides silence latch
      audioStartAlarm();
    } else if (!alarmSilenced && audioGetState() != AUDIO_MASTER_ALARM) {
      audioStartAlarm();       // re-trigger after natural stop
    }
  } else {
    alarmMask &= ~bit;
    if (alarmMask == 0) {
      alarmSilenced = false;
      audioStopAlarm();
    }
  }
}

void setup() {
  setupAudio();
}

void loop() {
  updateAudio();   // must be called every loop pass

  // Drive conditions from telemetry, e.g.:
  // setAlarmCondition(ALARM_HIGH_TEMP, state.maxTemp > tempAlarm);

  // Silence on button press:
  // if (masterAlarmButtonPressed) { alarmSilenced = true; audioSilence(); }
}
```

---

## Version History

| Version | Notes |
|---------|-------|
| **1.0.1** | `audioSilence()` now stops all audio unconditionally (previously only stopped when in `AUDIO_MASTER_ALARM` state). Clarified documentation distinguishing `audioSilence()` from `audioStopAlarm()`. |
| **1.0.0** | Initial release. Four-state priority machine: `AUDIO_IDLE`, `AUDIO_CHIRP`, `AUDIO_CAUTION_TONE`, `AUDIO_MASTER_ALARM`. Space Shuttle C/W spec alarm (375 Hz / 1000 Hz at 2.5 Hz). `millis()`-based timing throughout; no `delay()`. |

---

## Notes

- **Condition tracking is the sketch's responsibility** — the library only drives the tone. The sketch must maintain the active condition bitmask, the silence latch, and all re-trigger logic. See `Audio.ino` in the KCMk1 Annunciator for the reference implementation.
- **`audioEnabled` gating** — the library has no internal enable/disable flag. The calling sketch is responsible for gating calls behind its own `audioEnabled` flag. In KCMk1 sketches this is done in `ScreenMain.ino`; `audioSilence()` is called unconditionally on scene change and vessel switch regardless of the flag.
- **Single output pin** — all audio is multiplexed through one `tone()` pin. Only one sound plays at a time; the state machine priority order determines which.
- **No blocking** — `updateAudio()` never calls `delay()`. All timing is `millis()`-based. The function is a no-op when idle.
- **`tone()` on Teensy 4.0** — `tone()` uses a hardware timer. `AUDIO_PIN` must be a PWM-capable pin.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
