/**
 * @file        ConstructionTest.cpp
 * @version     2.0.0
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Per-board-type construction test sequences. Implemented as a
 *              small step-table runner: each module kind has an ordered list
 *              of steps; each step drives the board's outputs and/or verifies
 *              its inputs, then the operator confirms PASS/FAIL (some steps
 *              auto-pass on detection). See ConstructionTest.h.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */

#include "ConstructionTest.h"
#include "TesterConfig.h"
#include "TesterHW.h"
#include "TesterUI.h"
#include <KerbalModuleCommon.h>
#include <string.h>
#include <stdio.h>

// ============================================================
//  Step model
// ============================================================
enum StepType : uint8_t {
    ST_ENABLE,      // send CMD_ENABLE, auto-advance
    ST_LEDWALK,     // light each LED position in turn, then confirm
    ST_BTNWALK,     // detect every labelled input
    ST_AXIS,        // joystick axis range sweep
    ST_MOTOR,       // throttle motor full-range sweep
    ST_TOUCH,       // throttle touch + manual wiper
    ST_ENC,         // dual-encoder direction test (arg = encoder index)
    ST_VALUEDELTA,  // display encoder via value up/down
    ST_SEG,         // display segment/digit pattern
    ST_BULB,        // CMD_BULB_TEST confirm
    ST_SUMMARY      // results + finish
};
struct Step { StepType type; uint8_t arg; };

// Per-kind step sequences -----------------------------------
static const Step STEPS_BTN[]   = { {ST_ENABLE,0},{ST_LEDWALK,0},{ST_BTNWALK,0},{ST_SUMMARY,0} };
static const Step STEPS_JOY[]   = { {ST_ENABLE,0},{ST_AXIS,0},{ST_BTNWALK,0},{ST_LEDWALK,0},{ST_SUMMARY,0} };
static const Step STEPS_DISP[]  = { {ST_ENABLE,0},{ST_SEG,0},{ST_VALUEDELTA,0},{ST_BTNWALK,0},{ST_LEDWALK,0},{ST_SUMMARY,0} };
static const Step STEPS_THR[]   = { {ST_ENABLE,0},{ST_MOTOR,0},{ST_TOUCH,0},{ST_BULB,0},{ST_BTNWALK,0},{ST_SUMMARY,0} };
static const Step STEPS_DENC[]  = { {ST_ENC,0},{ST_ENC,1},{ST_BTNWALK,0},{ST_SUMMARY,0} };

// ============================================================
//  Runtime state
// ============================================================
static bool              _active = false;
static const ModuleInfo* _info   = nullptr;
static uint8_t           _addr    = 0;
static const Step*       _steps   = nullptr;
static uint8_t           _stepCount = 0;
static uint8_t           _stepIdx   = 0;
static bool              _entered   = false;

static uint32_t _t0 = 0, _tPoll = 0;
static uint8_t  _sub = 0;          // per-step sub-phase
static uint8_t  _ledIdx = 0;

static ModuleState _st;            // last parsed module state (persists across frames)
static uint32_t _seen = 0;         // button-walk detected mask
static int16_t  _amin[3], _amax[3];
static bool     _encCW = false, _encCCW = false;
static uint16_t _vLast = 0; static bool _vFirst = true, _vUp = false, _vDown = false;

static uint8_t  _result[8];        // 0=pending,1=pass,2=fail per step
static const uint8_t LED_ACTIVE_STATE = KMC_LED_ACTIVE;

// status line scratch
static char  _line[6][28];
static const char* _linePtr[6];

// ============================================================
//  Helpers
// ============================================================
static uint8_t ledCount() {
    switch (_info->kind) {
        case MK_BUTTON12:
        case MK_BUTTON24:     return 16;   // 12 NeoPixel + 4 discrete positions
        case MK_JOYSTICK:     return 2;
        case MK_DISPLAY:      return 3;
        default:              return 0;
    }
}

static uint32_t pressedMask(const ModuleState& st) {
    switch (_info->kind) {
        case MK_JOYSTICK: return st.flags;   // persistent button state
        default:          return st.events;  // current/edge bits
    }
}

static void sendLedSingle(uint8_t idx) {
    uint8_t pl[KMC_LED_PAYLOAD_SIZE];
    memset(pl, 0, sizeof(pl));
    if (idx < 16) kmcLedPackSet(pl, idx, LED_ACTIVE_STATE);
    hwSendCommand(_addr, KMC_CMD_SET_LED_STATE, pl, KMC_LED_PAYLOAD_SIZE);
}

static void sendThrottle(uint16_t v) {
    uint8_t pl[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFF) };
    hwSendCommand(_addr, KMC_CMD_SET_THROTTLE, pl, 2);
}

static void sendValue(uint16_t v) {
    uint8_t pl[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFF) };
    hwSendCommand(_addr, KMC_CMD_SET_VALUE, pl, 2);
}

// Read the module's current state (throttled) into the persistent _st.
// Returns true if _st was refreshed this call; _st always holds the most
// recent successful parse so handlers can read it on throttled frames too.
static bool readState() {
    uint32_t now = millis();
    if (now - _tPoll < INPUT_POLL_MS) return false;
    _tPoll = now;
    uint8_t pkt[16];
    uint8_t n = hwReadPacket(_addr, pkt, kindPacketSize(_info->kind));
    if (n < KMC_HEADER_SIZE) return false;
    hwParsePacket(_info, pkt, n, _st);
    return true;
}

static void buildHeader(char* buf, uint8_t n) {
    snprintf(buf, n, "CONSTR TEST  %s  %u/%u",
             _info->name, (unsigned)(_stepIdx + 1), (unsigned)_stepCount);
}

static void finishStep(bool pass) {
    if (_stepIdx < sizeof(_result)) _result[_stepIdx] = pass ? 1 : 2;
    _stepIdx++;
    _entered = false;
}

static void enterStep() {
    _entered = true;
    _sub = 0; _ledIdx = 0; _seen = 0; _t0 = millis();
    _encCW = _encCCW = false; _vFirst = true; _vUp = _vDown = false;
    for (uint8_t i = 0; i < 3; i++) { _amin[i] = 32767; _amax[i] = -32768; }
    memset(&_st, 0, sizeof(_st));
}

// Poll the control buttons; ABORT ends the whole test from any screen.
static CtButton ctPoll(uint8_t mask) {
    CtButton b = uiCtPoll(mask | CTB_ABORT);
    if (b == CT_ABORT) _active = false;
    return b;
}

// ============================================================
//  ctBegin / ctActive
// ============================================================
void ctBegin(const ModuleInfo* info, uint8_t addr) {
    _info = info; _addr = addr; _active = (info != nullptr);
    switch (info->kind) {
        case MK_BUTTON12:
        case MK_BUTTON24:     _steps = STEPS_BTN;  _stepCount = sizeof(STEPS_BTN)/sizeof(Step);  break;
        case MK_JOYSTICK:     _steps = STEPS_JOY;  _stepCount = sizeof(STEPS_JOY)/sizeof(Step);  break;
        case MK_DISPLAY:      _steps = STEPS_DISP; _stepCount = sizeof(STEPS_DISP)/sizeof(Step); break;
        case MK_THROTTLE:     _steps = STEPS_THR;  _stepCount = sizeof(STEPS_THR)/sizeof(Step);  break;
        case MK_DUAL_ENCODER: _steps = STEPS_DENC; _stepCount = sizeof(STEPS_DENC)/sizeof(Step); break;
        default:              _active = false; return;
    }
    memset(_result, 0, sizeof(_result));
    _stepIdx = 0; _entered = false;
}

bool ctActive() { return _active; }

// ============================================================
//  Step handlers (each renders + advances; ABORT handled centrally)
// ============================================================

static void stepEnable() {
    hwSendCommand(_addr, KMC_CMD_ENABLE, nullptr, 0);
    finishStep(true);   // nothing to confirm
}

static void stepLedWalk() {
    char hdr[40]; buildHeader(hdr, sizeof(hdr));
    uint8_t total = ledCount();
    if (_sub == 0) {
        // Walk: light one LED at a time, ~450ms each.
        if (millis() - _t0 >= 450) { _t0 = millis(); _ledIdx++; }
        if (_ledIdx >= total) { _sub = 1; sendLedSingle(255); /* none */ }
        else sendLedSingle(_ledIdx);
        snprintf(_line[0], 28, "Lighting LED %u of %u", (unsigned)(_ledIdx + 1), (unsigned)total);
        _linePtr[0] = _line[0];
        uiCtRender(hdr, "Watch each LED light in turn", _linePtr, 1, CTB_ABORT);
        ctPoll(CTB_ABORT);
    } else {
        uiCtRender(hdr, "All LEDs lit in order/colour?", nullptr, 0,
                   CTB_PASS | CTB_FAIL | CTB_RETRY | CTB_ABORT);
        CtButton b = ctPoll(CTB_PASS | CTB_FAIL | CTB_RETRY | CTB_ABORT);
        if (b == CT_PASS) finishStep(true);
        else if (b == CT_FAIL) finishStep(false);
        else if (b == CT_RETRY) { _sub = 0; _ledIdx = 0; _t0 = millis(); }
    }
}

static void stepBtnWalk() {
    char hdr[40]; buildHeader(hdr, sizeof(hdr));
    if (readState()) _seen |= pressedMask(_st);

    // Count labelled inputs and how many seen.
    uint8_t need = 0, got = 0;
    for (uint8_t i = 0; i < _info->inputCount; i++) {
        if (_info->labels[i] && _info->labels[i][0]) {
            need++;
            if (_seen & (1UL << i)) got++;
        }
    }
    snprintf(_line[0], 28, "Detected %u / %u inputs", got, need);
    _linePtr[0] = _line[0];
    // Show up to 3 not-yet-seen labels as hints.
    uint8_t hintRow = 1, shown = 0;
    for (uint8_t i = 0; i < _info->inputCount && shown < 3; i++) {
        if (_info->labels[i] && _info->labels[i][0] && !(_seen & (1UL << i))) {
            snprintf(_line[hintRow], 28, "press: %s", _info->labels[i]);
            _linePtr[hintRow] = _line[hintRow]; hintRow++; shown++;
        }
    }
    uiCtRender(hdr, "Press every input once", _linePtr, hintRow,
               CTB_PASS | CTB_FAIL | CTB_ABORT);

    if (need > 0 && got >= need) { finishStep(true); return; }   // auto-pass
    CtButton b = ctPoll(CTB_PASS | CTB_FAIL | CTB_ABORT);
    if (b == CT_PASS) finishStep(true);
    else if (b == CT_FAIL) finishStep(false);
}

static void stepAxis() {
    char hdr[40]; buildHeader(hdr, sizeof(hdr));
    if (readState()) {
        for (uint8_t i = 0; i < 3; i++) {
            if (_st.axis[i] < _amin[i]) _amin[i] = _st.axis[i];
            if (_st.axis[i] > _amax[i]) _amax[i] = _st.axis[i];
        }
    }
    bool rangedAll = true;
    for (uint8_t i = 0; i < 3; i++) {
        int32_t span = (int32_t)_amax[i] - (int32_t)_amin[i];
        snprintf(_line[i], 28, "AX%u %6d  [%d..%d]", (unsigned)(i + 1),
                 _st.axis[i], _amin[i], _amax[i]);
        _linePtr[i] = _line[i];
        if (span < 16000) rangedAll = false;
    }
    uiCtRender(hdr, "Move stick full range + twist", _linePtr, 3,
               CTB_PASS | CTB_FAIL | CTB_ABORT);
    if (rangedAll) { finishStep(true); return; }
    CtButton b = ctPoll(CTB_PASS | CTB_FAIL | CTB_ABORT);
    if (b == CT_PASS) finishStep(true);
    else if (b == CT_FAIL) finishStep(false);
}

static void stepMotor() {
    char hdr[40]; buildHeader(hdr, sizeof(hdr));
    // sub 0..2 = drive 0% / 100% / 50% with 1.2s dwell each, then confirm.
    static const uint16_t targets[3] = { 0, 32767, 16384 };
    static const char*    tnames[3]  = { "0%", "100%", "50%" };
    if (_sub < 3) {
        if (millis() - _t0 >= 1200) { _sub++; _t0 = millis(); if (_sub < 3) sendThrottle(targets[_sub]); }
        else if (_t0 == millis() || _sub == 0) { sendThrottle(targets[_sub]); }
        if (_sub < 3) {
            snprintf(_line[0], 28, "Driving to %s", tnames[_sub]);
            _linePtr[0] = _line[0];
            uiCtRender(hdr, "Slider should move full range", _linePtr, 1, CTB_ABORT);
            ctPoll(CTB_ABORT);
        }
    } else {
        uiCtRender(hdr, "Slider moved smoothly 0-100%?", nullptr, 0,
                   CTB_PASS | CTB_FAIL | CTB_RETRY | CTB_ABORT);
        CtButton b = ctPoll(CTB_PASS | CTB_FAIL | CTB_RETRY | CTB_ABORT);
        if (b == CT_PASS) finishStep(true);
        else if (b == CT_FAIL) finishStep(false);
        else if (b == CT_RETRY) { _sub = 0; _t0 = millis(); sendThrottle(0); }
    }
}

static void stepTouch() {
    char hdr[40]; buildHeader(hdr, sizeof(hdr));
    readState();
    bool touch = (_st.flags & 0x04) != 0;         // THR_FLAG_TOUCH (bit 2)
    snprintf(_line[0], 28, "Touch: %s", touch ? "YES" : "no");
    snprintf(_line[1], 28, "Wiper value: %u", _st.value);
    _linePtr[0] = _line[0]; _linePtr[1] = _line[1];
    uiCtRender(hdr, "Grab & slide by hand", _linePtr, 2,
               CTB_PASS | CTB_FAIL | CTB_ABORT);
    CtButton b = ctPoll(CTB_PASS | CTB_FAIL | CTB_ABORT);
    if (b == CT_PASS) finishStep(true);
    else if (b == CT_FAIL) finishStep(false);
}

static void stepEnc(uint8_t idx) {
    char hdr[40]; buildHeader(hdr, sizeof(hdr));
    if (readState()) {
        if (_st.enc[idx] > 0) _encCW = true;
        if (_st.enc[idx] < 0) _encCCW = true;
    }
    snprintf(_line[0], 28, "ENC%u  CW:%s  CCW:%s", (unsigned)(idx + 1),
             _encCW ? "ok" : "--", _encCCW ? "ok" : "--");
    _linePtr[0] = _line[0];
    uiCtRender(hdr, "Turn the encoder both ways", _linePtr, 1,
               CTB_FAIL | CTB_ABORT);
    if (_encCW && _encCCW) { finishStep(true); return; }
    CtButton b = ctPoll(CTB_FAIL | CTB_ABORT);
    if (b == CT_FAIL) finishStep(false);
}

static void stepValueDelta() {
    char hdr[40]; buildHeader(hdr, sizeof(hdr));
    if (readState()) {
        if (_vFirst) { _vLast = _st.value; _vFirst = false; }
        else {
            if (_st.value > _vLast) _vUp = true;
            if (_st.value < _vLast) _vDown = true;
            _vLast = _st.value;
        }
    }
    snprintf(_line[0], 28, "Value: %u", _st.value);
    snprintf(_line[1], 28, "up:%s  down:%s", _vUp ? "ok" : "--", _vDown ? "ok" : "--");
    _linePtr[0] = _line[0]; _linePtr[1] = _line[1];
    uiCtRender(hdr, "Turn encoder up then down", _linePtr, 2, CTB_FAIL | CTB_ABORT);
    if (_vUp && _vDown) { finishStep(true); return; }
    CtButton b = ctPoll(CTB_FAIL | CTB_ABORT);
    if (b == CT_FAIL) finishStep(false);
}

static void stepSeg() {
    char hdr[40]; buildHeader(hdr, sizeof(hdr));
    // Alternate 8888 (all segments) and 1234 every second.
    uint16_t v = ((millis() / 1000) & 1) ? 1234 : 8888;
    if (millis() - _t0 >= 1000 || _sub == 0) { _t0 = millis(); _sub = 1; sendValue(v); }
    snprintf(_line[0], 28, "Showing %u", v);
    _linePtr[0] = _line[0];
    uiCtRender(hdr, "All digits & segments OK?", _linePtr, 1,
               CTB_PASS | CTB_FAIL | CTB_ABORT);
    CtButton b = ctPoll(CTB_PASS | CTB_FAIL | CTB_ABORT);
    if (b == CT_PASS) finishStep(true);
    else if (b == CT_FAIL) finishStep(false);
}

static void stepBulb() {
    char hdr[40]; buildHeader(hdr, sizeof(hdr));
    if (_sub == 0) { hwSendCommand(_addr, KMC_CMD_BULB_TEST, nullptr, 0); _sub = 1; }
    uiCtRender(hdr, "All discrete LEDs lit?", nullptr, 0,
               CTB_PASS | CTB_FAIL | CTB_RETRY | CTB_ABORT);
    CtButton b = ctPoll(CTB_PASS | CTB_FAIL | CTB_RETRY | CTB_ABORT);
    if (b == CT_PASS) finishStep(true);
    else if (b == CT_FAIL) finishStep(false);
    else if (b == CT_RETRY) { _sub = 0; }
}

static void stepSummary() {
    char hdr[40]; buildHeader(hdr, sizeof(hdr));
    uint8_t rows = 0;
    for (uint8_t i = 0; i < _stepIdx && rows < 6; i++) {
        if (_result[i] == 0) continue;
        snprintf(_line[rows], 28, "step %u: %s", (unsigned)(i + 1),
                 _result[i] == 1 ? "PASS" : "FAIL");
        _linePtr[rows] = _line[rows]; rows++;
    }
    uiCtRender(hdr, "Construction test complete", _linePtr, rows, CTB_NEXT | CTB_ABORT);
    CtButton b = ctPoll(CTB_NEXT | CTB_ABORT);
    if (b == CT_NEXT || b == CT_ABORT) _active = false;
}

// ============================================================
//  ctUpdate() — dispatch the current step
// ============================================================
void ctUpdate() {
    if (!_active) return;
    if (_stepIdx >= _stepCount) { _stepIdx = _stepCount - 1; }  // clamp to summary

    const Step& s = _steps[_stepIdx];
    if (!_entered) enterStep();

    // Each handler polls via ctPoll(), which ends the test on ABORT.
    switch (s.type) {
        case ST_ENABLE:     stepEnable();         break;
        case ST_LEDWALK:    stepLedWalk();        break;
        case ST_BTNWALK:    stepBtnWalk();        break;
        case ST_AXIS:       stepAxis();           break;
        case ST_MOTOR:      stepMotor();          break;
        case ST_TOUCH:      stepTouch();          break;
        case ST_ENC:        stepEnc(s.arg);       break;
        case ST_VALUEDELTA: stepValueDelta();     break;
        case ST_SEG:        stepSeg();            break;
        case ST_BULB:       stepBulb();           break;
        case ST_SUMMARY:    stepSummary();        break;
    }
}
