# KerbalDisplayAudio

**Kerbal Controller Mk1 — Audio Feedback Library**
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
| `AUDIO_CHIRP_NOTE_MS` | `120` ms | Duration of each chirp note (240 ms total) |
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

Lower-priority sounds are suppressed while a higher-priority state is active. Chirps and caution tones are also suppressed while the alarm is silenced (i.e. while alarm conditions remain active but the crew has acknowledged the alarm).

### Master Alarm

The library drives the master alarm tone but has no knowledge of which conditions are active or why. Condition tracking, the silence latch, and re-trigger logic all live in the calling sketch (`ScreenMain.ino` in the KCMk1 Annunciator). The sketch calls `audioStartAlarm()` when the first condition becomes active and `audioStopAlarm()` when the last condition clears.

`audioStartAlarm()` — starts the two-tone alternating loop. Has no effect if the alarm is already running.

`audioStopAlarm()` — stops the alarm tone and returns to idle. Has no effect if the alarm is not running.

`audioSilence()` — immediately stops the master alarm tone. Unlike `audioStopAlarm()`, this is called by the sketch when the crew presses the master alarm button. The distinction matters because the sketch needs to set its own silence latch *before* stopping the tone, so that its condition tracking logic knows not to restart the alarm for existing conditions.

The sketch is responsible for the following logic (example from `ScreenMain.ino`):
- Maintaining a bitmask of which alarm conditions are currently active
- Calling `audioStartAlarm()` when the mask transitions from 0 to non-zero
- Calling `audioStopAlarm()` when the mask transitions from non-zero to 0
- Managing a silence latch that prevents restart for existing conditions after `audioSilence()`
- Restarting the alarm (and clearing the latch) if a *new* condition fires while silenced

### API

`setupAudio()` — configures `AUDIO_PIN` as output and silences it. Call once from `setup()`.

`updateAudio()` — services all audio timing. Call every `loop()` iteration. Returns immediately if audio is idle.

`audioAlertChirp()` — plays a two-note ascending sequence (A5 → C#6). Signals a positive event such as an altitude or velocity threshold crossing, or orbital insertion. Suppressed if master alarm is active.

`audioCautionChirp()` — plays a two-note descending sequence (tritone interval). Signals a newly-set caution condition such as entering atmosphere or beginning descent. Suppressed if master alarm is active.

`audioCautionTone()` — plays a constant 1000 Hz tone for 1200 ms. Used for the ALT caution condition. Suppressed if master alarm is active.

`audioStartAlarm()` — starts the two-tone master alarm loop. Has no effect if already running. Call when the sketch's alarm condition mask transitions from 0 to non-zero.

`audioStopAlarm()` — stops the master alarm and returns to idle. Has no effect if not running. Call when the sketch's alarm condition mask transitions from non-zero to 0.

`audioSilence()` — immediately stops the master alarm. Call when the crew presses the master alarm button. The sketch must manage its own silence latch separately — this function only stops the tone.

`audioGetState()` — returns the current `AudioState` enum value (`AUDIO_IDLE`, `AUDIO_CHIRP`, `AUDIO_CAUTION_TONE`, or `AUDIO_MASTER_ALARM`).

---

## Usage

```cpp
#include <KerbalDisplayAudio.h>

// Sketch-side alarm condition tracking (application-specific)
static const uint8_t ALARM_HIGH_TEMP = (1 << 0);
static const uint8_t ALARM_LOW_DV    = (1 << 1);
static uint8_t alarmMask = 0;
static bool    alarmSilenced = false;

void setAlarmCondition(uint8_t bit, bool on) {
  if (on) {
    bool wasActive = (alarmMask != 0);
    alarmMask |= bit;
    if (!wasActive && !alarmSilenced)        audioStartAlarm();
    else if (wasActive && alarmSilenced) { alarmSilenced = false; audioStartAlarm(); }
    else if (!alarmSilenced && audioGetState() != AUDIO_MASTER_ALARM) audioStartAlarm();
  } else {
    alarmMask &= ~bit;
    if (alarmMask == 0) { alarmSilenced = false; audioStopAlarm(); }
  }
}

void setup() {
  setupAudio();
}

void loop() {
  updateAudio();  // must be called every loop pass

  // Set or clear a condition based on telemetry
  // setAlarmCondition(ALARM_HIGH_TEMP, state.maxTemp > tempAlarm);

  // Silence on button press
  // if (masterAlarmButtonPressed) { alarmSilenced = true; audioSilence(); }
}
```

---

## Notes

- **Condition tracking is the sketch's responsibility** — the library only drives the tone. The sketch must maintain the active condition bitmask, the silence latch, and all re-trigger logic. See `ScreenMain.ino` in the KCMk1 Annunciator for the reference implementation.
- **`audioEnabled` gating** — the library has no internal enable/disable flag. The calling sketch is responsible for gating calls behind its own `audioEnabled` flag. In KCMk1 sketches this is done in `ScreenMain.ino` and `audioSilence()` is called unconditionally on scene change and vessel switch.
- **Single output pin** — all audio is multiplexed through one `tone()` pin. Only one sound plays at a time; the state machine priority order determines which.
- **No blocking** — `updateAudio()` never calls `delay()`. All timing is `millis()`-based. The function is a no-op when idle.
- **`tone()` on Teensy 4.0** — `tone()` uses a hardware timer. `AUDIO_PIN` must be a PWM-capable pin.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
