# KerbalDisplayAudio

**Kerbal Controller Mk1 — Audio Feedback Library**
Non-blocking audio state machine for RA8875-based KSP controller display panels.
Part of the KCMk1 controller system.

---

## Overview

KerbalDisplayAudio provides audio feedback for KCMk1 display panels via the Arduino `tone()` API. It manages a priority-ordered state machine of four audio modes — master alarm, caution tone, caution chirp, and alert chirp — with all timing driven by `millis()` so that `loop()` never blocks.

The master alarm is modelled on the Space Shuttle Caution & Warning specification: 375 Hz / 1000 Hz alternating at 2.5 Hz. Individual alarm conditions are tracked independently via a bitmask so that silencing and re-triggering behave correctly across multiple simultaneous warnings.

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

### Master Alarm Condition Tracking

The master alarm tracks up to 8 independent conditions via a bitmask. Each condition is identified by one of the `AUDIO_ALARM_*` bit constants. The alarm tone plays while any bit is set and stops automatically when all bits clear.

**Silence behaviour:** calling `audioSilence()` stops the tone and sets an internal latch. The alarm will not restart for existing conditions. However, if a *new* condition activates while silenced, the alarm restarts immediately — the crew cannot inadvertently miss a fresh warning. The silence latch is cleared when all conditions clear.

**Alarm condition bits:**

| Constant | Bit | Default use |
|----------|-----|-------------|
| `AUDIO_ALARM_GROUND_PROX` | `(1 << 0)` | `CW_GROUND_PROX` |
| `AUDIO_ALARM_HIGH_G` | `(1 << 1)` | `CW_HIGH_G` |
| `AUDIO_ALARM_BUS_VOLTAGE` | `(1 << 2)` | `CW_BUS_VOLTAGE` |
| `AUDIO_ALARM_HIGH_TEMP` | `(1 << 3)` | `CW_HIGH_TEMP` |
| `AUDIO_ALARM_LOW_DV` | `(1 << 4)` | `CW_LOW_DV` |

### API

`setupAudio()` — configures `AUDIO_PIN` as output and silences it. Call once from `setup()`.

`updateAudio()` — services all audio timing. Call every `loop()` iteration. Returns immediately if audio is idle.

`audioAlertChirp()` — plays a two-note ascending sequence (A5 → C#6). Signals a positive event such as an altitude or velocity threshold crossing, or orbital insertion. Suppressed if master alarm is active or silenced.

`audioCautionChirp()` — plays a two-note descending sequence (tritone interval). Signals a newly-set caution condition such as entering atmosphere or beginning descent. Suppressed if master alarm is active or silenced.

`audioCautionTone()` — plays a constant 1000 Hz tone for 1200 ms. Used for the ALT caution condition. Suppressed if master alarm is active or silenced.

`audioMasterAlarm(condBit, on)` — notifies the library that a master alarm condition has changed state. Pass one of the `AUDIO_ALARM_*` bits and `true` to set or `false` to clear. The alarm tone starts when the first condition is set and stops when the last condition clears.

`audioSilence()` — immediately silences the master alarm. Has no effect on chirps or caution tones. The alarm will not restart for currently active conditions, but a new condition activating will restart it.

`audioGetState()` — returns the current `AudioState` enum value (`AUDIO_IDLE`, `AUDIO_CHIRP`, `AUDIO_CAUTION_TONE`, or `AUDIO_MASTER_ALARM`).

---

## Usage

```cpp
#include <KerbalDisplayAudio.h>

void setup() {
  setupAudio();
}

void loop() {
  updateAudio();  // must be called every loop pass

  // Example: trigger alarm when a condition becomes active
  // audioMasterAlarm(AUDIO_ALARM_HIGH_TEMP, true);   // condition set
  // audioMasterAlarm(AUDIO_ALARM_HIGH_TEMP, false);  // condition cleared

  // Example: alert chirp on orbital insertion
  // audioAlertChirp();

  // Example: silence alarm on button press
  // if (masterAlarmButtonPressed) audioSilence();
}
```

---

## Notes

- **`audioEnabled` gating** — the library has no internal enable/disable flag. The calling sketch is responsible for gating calls behind its own `audioEnabled` flag. In KCMk1 sketches this is done in `ScreenMain.ino` before each `audioMasterAlarm()` / `audioCautionTone()` / `audioAlertChirp()` call, and `audioSilence()` is called unconditionally on scene change and vessel switch.
- **Single output pin** — all audio is multiplexed through one `tone()` pin. Only one sound plays at a time; the state machine priority order determines which.
- **No blocking** — `updateAudio()` never calls `delay()`. All timing is `millis()`-based. The function is a no-op when idle.
- **`tone()` on Teensy 4.0** — `tone()` uses a hardware timer. `AUDIO_PIN` must be a PWM-capable pin.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
