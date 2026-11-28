# KCMk1TesterCore

**Version:** 1.1.1
**Date:** 2026-11-26
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
    KTC_KBC_FW_MAJOR, KTC_KBC_FW_MINOR, KTC_KBC_FW_PATCH,   // expected library X.Y.Z
    0x00,                                              // expected caps
    2, 1, 0,                                           // expected sketch X.Y.Z
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

## Identity check

`ktcStepIdentity` asks for the 8-byte identity of I2C spec v2.22 (type, library major, minor, caps, library patch, sketch major, minor, patch) and checks the type, the caps, no fault flag, and both versions as X.Y.Z: each with the same major as the sketch's `KtcModuleDef` (`fw*` for the library, `sk*` for the module sketch), and minor.patch at least the one the tester was written for. A newer firmware passes with "(newer than tester)". A module that sends the 5-byte form (no sketch version) or the old 4-byte form (no patch either) fails: it runs older firmware. For a standalone sketch with no library (Throttle, Dual Encoder) `fw*` and `sk*` are both the sketch's version.

Versions are shown as the master shows them (`ktcFmtVersions()`): sketch then library, `2.1.0 / lib 2.5.0`, or one `X.Y.Z` when the two are the same; X.Y when a patch is unknown, `-` for an unknown sketch version. The power bar shows them compactly (`v2.1.0/2.5.0`).

## Version history

| Version | Date | Notes |
|---|---|---|
| 1.1.1 | 2026-11-27 | Expected firmware: KerbalButtonCore 2.5.1 (`KTC_KBC_FW_PATCH` 1). |
| 1.1.0 | 2026-11-26 | Identity is read as 8 bytes, both versions (library and module sketch) checked and shown as X.Y.Z (I2C spec v2.22); the 5- and 4-byte forms are read and fail the check. `KtcModuleDef` gains `fwPatch` after `fwMinor` and `skMajor` / `skMinor` / `skPatch` after `caps`; `KtcIdentity` gains `fwPatch` and `sk*`; `KTC_IDENTITY_NONE`, `ktcFmtVersion()` and `ktcFmtVersions()` added. Expected firmware: KerbalButtonCore 2.5.0 (`KTC_KBC_FW_*`), KerbalJoystickCore 2.4.0 (`KTC_KJC_FW_*`). Needs KerbalModuleCommon 1.9.0. |
| 1.0.0 | 2026-11-08 | First release. |
