# KCMk1TesterCore

**Version:** 1.0.0
**Date:** 2026-11-08
**Board:** KC-01-9001 v2.0 / KC-01-9002 Module Tester (Seeed XIAO RA4M1)

Shared runtime for the per-module tester sketches in `Software/Modules/0_Test Set/Module Testers/`. A sketch describes its module and lists its test steps; the library does the rest: bus access, INA228 power readings, the touchscreen, the step runner with results and a serial report, and the steps that most modules share.

## A tester sketch

```cpp
#include <KCMk1TesterCore.h>

static const KtcStep STEPS[] = {
    { "Identity",       ktcStepIdentity, nullptr },
    { "Boot handshake", ktcStepBoot,     &ktcKbcBoot },
    // ...
};

static const KtcModuleDef MODULE = {
    "Action Control", "KC-01-1801/1802 button board",
    KMC_TYPE_ACTION_CONTROL, 0x22, KMC_BUTTON_PACKET_SIZE,
    KTC_KBC_FW_MAJOR, KTC_KBC_FW_MINOR, 0x00,          // expected identity
    STEPS, sizeof(STEPS) / sizeof(STEPS[0]),
    ktcKbcCleanup, ktcKbcLive16,                       // optional
};

void setup() { ktcBegin(MODULE); }
void loop()  { ktcLoop(); }
```

Up to 12 steps per sketch (`KTC_MAX_STEPS`).

## Files

| File | Contents |
|---|---|
| `KTC_Config.h` | Pin map, on-board I2C addresses, INA228 calibration, timing |
| `KTC_Bus` | Identity, commands, reads, RST/INT lines, INA228 |
| `KTC_UI` | ST7789V + FT6236 screen regions and widgets (rows, chips, bars, swatches, button row, menu) |
| `KTC_Runner` | App flow (splash, wait for module, menu, run, summary, live view), step context and helpers, serial report |
| `KTC_Steps` | Shared steps: identity, boot handshake, enable + INT, input walk, LED walk, visual sequence, lifecycle, supply current; input decoders for each module family |
| `KTC_Kbc` | Ready-made checks for the KerbalButtonCore modules |
| `KTC_Kjc` | Joystick checks: centre/deadzone, axis travel, LED states |
| `KTC_Script` | Script step for modules that drive their own state (7-segment displays) |

## Writing a step

A step is a function called every `loop()` until it returns `KTC_PASS`, `KTC_FAIL` or `KTC_SKIP`:

```cpp
static KtcVerdict stepExample(KtcCtx& c) {
    if (c.entry) {                       // first call (also after RETRY)
        ktcShow(c, "Press the button.", KTC_BTN_FAIL);
        ktcSend(c, KMC_CMD_ENABLE);
    }
    if (c.btn == KTC_BTN_FAIL) return ktcFail(c, "operator FAIL");
    if (c.fresh && (ktcByte(c, 0) & 0x01)) return ktcPass(c, "seen");   // INT-gated packet
    return KTC_RUNNING;
}
```

- `c.phase` / `ktcPhase()` / `ktcPhaseMs()` run a small state machine; `c.v[]` and `c.m[]` are scratch that is cleared at each step start.
- `c.fresh` is true on a call with a new packet. The runner reads only when the module asserts INT; `ktcReadNow()` reads at any time (for modules that answer a read with nothing pending).
- The runner owns **SKIP**, **RETRY** and **ABORT**. After an abort, or at the end of a run, it stops any bulb test and calls the module's `cleanup`.

## Dependencies

KerbalModuleCommon, Adafruit GFX Library, Adafruit ST7735 and ST7789 Library, Adafruit FT6206 Library.
