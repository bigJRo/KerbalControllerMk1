/**
 * @file        KTC_Runner.cpp
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Runner implementation: app states, step sequencing, results,
 *              serial report, LIVE view.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include "KTC_Runner.h"
#include "KTC_Config.h"
#include <stdarg.h>

enum AppState : uint8_t { AS_SPLASH, AS_WAIT, AS_MENU, AS_RUN, AS_RESULT, AS_SUMMARY, AS_LIVE };

static const KtcModuleDef* _mod = nullptr;
static KtcCtx   _c;
static AppState _state = AS_SPLASH;
static uint32_t _tState = 0, _tPower = 0, _tProbe = 0, _tRead = 0;
static uint8_t  _probeMiss = 0;
static KtcIdentity _id = {0, 0, 0, 0, false};

// Run bookkeeping
static uint8_t    _cur = 0;            // current step index
static bool       _runAll = false;
static KtcVerdict _res[KTC_MAX_STEPS];
static char       _resNote[KTC_MAX_STEPS][40];
static bool       _phaseSet = false;
static bool       _bulbLive = false;   // LIVE screen bulb toggle
static uint8_t    _waitShown = 0xFF;   // which WAIT message is on screen

// ------------------------------------------------------------
//  Small helpers
// ------------------------------------------------------------
static uint8_t _stepCount() {
    return (_mod->stepCount > KTC_MAX_STEPS) ? KTC_MAX_STEPS : _mod->stepCount;
}

static const char* _verdictText(KtcVerdict v) {
    switch (v) {
        case KTC_PASS: return "PASS";
        case KTC_FAIL: return "FAIL";
        case KTC_SKIP: return "SKIP";
        default:       return "";
    }
}

static uint16_t _verdictColor(KtcVerdict v) {
    switch (v) {
        case KTC_PASS: return KTC_C_PASS;
        case KTC_FAIL: return KTC_C_FAIL;
        case KTC_SKIP: return KTC_C_WARN;
        default:       return KTC_C_DIM;
    }
}

static void _powerLeft(char* buf, uint8_t n) {
    if (_id.valid) snprintf(buf, n, "0x%02X fw%u.%u", _mod->addr, _id.fwMajor, _id.fwMinor);
    else           snprintf(buf, n, "0x%02X", _mod->addr);
}

static void _pollPower(bool draw) {
    KtcPower p = ktcReadPower();
    if (p.ok) _c.power = p;
    if (draw) {
        char l[24];
        _powerLeft(l, sizeof(l));
        ktcUiPowerBar(l, _c.power.ok ? _c.power : p);
    }
}

static void _parseHeader() {
    uint8_t st = _c.pkt[0];
    _c.lifecycle   = st & KMC_STATUS_LIFECYCLE_MASK;
    _c.fault       = (st & KMC_STATUS_FAULT) != 0;
    _c.dataChanged = (st & KMC_STATUS_DATA_CHANGED) != 0;
}

static bool _readPacket() {
    uint8_t buf[16];
    uint8_t n = _mod->pktSize > sizeof(buf) ? sizeof(buf) : _mod->pktSize;
    uint8_t got = ktcRead(_mod->addr, buf, n);
    _tRead = millis();
    if (got < n || buf[1] != _mod->typeId) { _c.badReads++; return false; }
    memcpy(_c.pkt, buf, n);
    _c.pktLen = n;
    _c.pktCount++;
    _parseHeader();
    _c.fresh = true;
    return true;
}

static void _autoRead() {
    if (!_c.autoRead) return;
    if (!ktcIntAsserted()) return;
    if (millis() - _tRead < KTC_READ_GAP_MS) return;
    _readPacket();
}

// ------------------------------------------------------------
//  Step helpers (public)
// ------------------------------------------------------------
void ktcPhase(KtcCtx& c, uint8_t p) {
    c.phase = p;
    c.t0 = millis();
    _phaseSet = true;
}

uint32_t ktcPhaseMs(const KtcCtx& c) { return millis() - c.t0; }

static void _vnote(KtcCtx& c, const char* fmt, va_list ap) {
    if (!fmt) { c.note[0] = '\0'; return; }
    vsnprintf(c.note, sizeof(c.note), fmt, ap);
}

KtcVerdict ktcPass(KtcCtx& c, const char* fmt, ...) {
    if (fmt) { va_list ap; va_start(ap, fmt); _vnote(c, fmt, ap); va_end(ap); }
    return KTC_PASS;
}

KtcVerdict ktcFail(KtcCtx& c, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); _vnote(c, fmt, ap); va_end(ap);
    return KTC_FAIL;
}

void ktcNote(KtcCtx& c, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); _vnote(c, fmt, ap); va_end(ap);
}

void ktcShow(KtcCtx& c, const char* instr, uint16_t buttons) {
    (void)c;
    bool noSkip = (buttons & KTC_SHOW_NOSKIP) != 0;
    buttons &= ~KTC_SHOW_NOSKIP;
    if (!noSkip) buttons |= KTC_BTN_SKIP;
    buttons |= KTC_BTN_ABORT;
    ktcUiInstr(instr);
    ktcUiLiveClear();
    ktcUiToast(nullptr);
    ktcUiButtons(buttons);
}

uint8_t ktcSend(KtcCtx& c, uint8_t cmd, const uint8_t* payload, uint8_t len) {
    return ktcCommand(c.addr, cmd, payload, len);
}

uint8_t ktcSend1(KtcCtx& c, uint8_t cmd, uint8_t b) {
    return ktcCommand1(c.addr, cmd, b);
}

uint8_t ktcLedsAll(KtcCtx& c, uint8_t state) {
    uint8_t p[KMC_LED_PAYLOAD_SIZE];
    ktcLedFill(p, state);
    return ktcCommand(c.addr, KMC_CMD_SET_LED_STATE, p, KMC_LED_PAYLOAD_SIZE);
}

uint8_t ktcLeds(KtcCtx& c, const uint8_t* states16) {
    uint8_t p[KMC_LED_PAYLOAD_SIZE] = {0};
    for (uint8_t i = 0; i < 16; i++) kmcLedPackSet(p, i, states16[i]);
    return ktcCommand(c.addr, KMC_CMD_SET_LED_STATE, p, KMC_LED_PAYLOAD_SIZE);
}

bool ktcReadNow(KtcCtx& c) {
    (void)c;
    return _readPacket();
}

void ktcDrain(KtcCtx& c) {
    (void)c;
    for (uint8_t i = 0; i < 10 && ktcIntAsserted(); i++) {
        _readPacket();
        delay(10);
    }
    _c.fresh = false;
}

uint32_t ktcBits(const KtcCtx& c, uint8_t off, uint8_t nBytes) {
    uint32_t v = 0;
    for (uint8_t i = 0; i < nBytes && i < 4; i++)
        v |= (uint32_t)c.pkt[KMC_HEADER_SIZE + off + i] << (8 * i);
    return v;
}

uint8_t ktcByte(const KtcCtx& c, uint8_t off) { return c.pkt[KMC_HEADER_SIZE + off]; }

int16_t ktcS16(const KtcCtx& c, uint8_t off) {
    return (int16_t)(((uint16_t)c.pkt[KMC_HEADER_SIZE + off] << 8) | c.pkt[KMC_HEADER_SIZE + off + 1]);
}

uint16_t ktcU16(const KtcCtx& c, uint8_t off) {
    return (uint16_t)(((uint16_t)c.pkt[KMC_HEADER_SIZE + off] << 8) | c.pkt[KMC_HEADER_SIZE + off + 1]);
}

const char* ktcLifecycleName(uint8_t lc) {
    switch (lc & KMC_STATUS_LIFECYCLE_MASK) {
        case KMC_STATUS_ACTIVE:     return "ACTIVE";
        case KMC_STATUS_SLEEPING:   return "SLEEPING";
        case KMC_STATUS_DISABLED:   return "DISABLED";
        default:                    return "BOOT_READY";
    }
}

const char* ktcTypeName(uint8_t t) {
    switch (t) {
        case KMC_TYPE_UI_CONTROL:        return "UI Control";
        case KMC_TYPE_FUNCTION_CONTROL:  return "Function Control";
        case KMC_TYPE_ACTION_CONTROL:    return "Action Control";
        case KMC_TYPE_STABILITY_CONTROL: return "Stability Control";
        case KMC_TYPE_VEHICLE_CONTROL:   return "Vehicle Control";
        case KMC_TYPE_TIME_CONTROL:      return "Time Control";
        case KMC_TYPE_AUX_CTRL:          return "Auxiliary Control";
        case KMC_TYPE_JOYSTICK_ROTATION: return "Joystick Rotation";
        case KMC_TYPE_JOYSTICK_TRANS:    return "Joystick Translation";
        case KMC_TYPE_GPWS_INPUT:        return "GPWS";
        case KMC_TYPE_PRE_WARP_TIME:     return "Pre-Warp Time";
        case KMC_TYPE_THROTTLE:          return "Throttle";
        case KMC_TYPE_DUAL_ENCODER:      return "Dual Encoder";
        default:                         return "unknown module";
    }
}

// ------------------------------------------------------------
//  Serial report
// ------------------------------------------------------------
static void _reportHeader() {
    Serial.println();
    Serial.println(F("=================================================="));
    Serial.print(F("KCMk1 tester: ")); Serial.print(_mod->name);
    Serial.print(F("  (tester core ")); Serial.print(F(KTC_VERSION_STR)); Serial.println(F(")"));
    Serial.print(F("Address 0x")); Serial.print(_mod->addr, HEX);
    if (_id.valid) {
        Serial.print(F("  type 0x")); Serial.print(_id.typeId, HEX);
        Serial.print(F("  fw ")); Serial.print(_id.fwMajor); Serial.print('.'); Serial.print(_id.fwMinor);
        Serial.print(F("  caps 0x")); Serial.print(_id.caps, HEX);
    }
    Serial.println();
    Serial.print(F("Run started at ")); Serial.print(millis() / 1000); Serial.println(F(" s after tester power-on"));
    Serial.println(F("--------------------------------------------------"));
}

static void _reportStep(uint8_t i) {
    char line[96];
    snprintf(line, sizeof(line), "[%2u] %-22s %-4s %s", (unsigned)(i + 1), _mod->steps[i].name,
             _verdictText(_res[i]), _resNote[i]);
    Serial.println(line);
}

static void _reportSummary() {
    uint8_t pass = 0, fail = 0, skip = 0;
    for (uint8_t i = 0; i < _stepCount(); i++) {
        if (_res[i] == KTC_PASS) pass++;
        else if (_res[i] == KTC_FAIL) fail++;
        else if (_res[i] == KTC_SKIP) skip++;
    }
    Serial.println(F("--------------------------------------------------"));
    char line[80];
    snprintf(line, sizeof(line), "Result: %s   pass %u  fail %u  skip %u",
             fail ? "FAIL" : (skip ? "INCOMPLETE" : "PASS"), pass, fail, skip);
    Serial.println(line);
    if (_c.power.ok) {
        snprintf(line, sizeof(line), "Supply at end: %.2f V  %.0f mA", (double)_c.power.volts,
                 (double)(_c.power.amps * 1000.0f));
        Serial.println(line);
    }
    Serial.println(F("=================================================="));
}

// ------------------------------------------------------------
//  Screens
// ------------------------------------------------------------
static void _drawTop(const char* right) {
    ktcUiTitle(_mod->name, right);
    _pollPower(true);
}

static void _enterWait() {
    _state = AS_WAIT;
    _waitShown = 0xFF;
    _id.valid = false;
    _drawTop("waiting");
    ktcUiClearBody();
    ktcUiButtons(0);
    _tProbe = 0;
}

static void _drawMenu() {
    _drawTop("menu");
    ktcUiClearBody();
    for (uint8_t i = 0; i < _stepCount(); i++) {
        char lbl[34];
        snprintf(lbl, sizeof(lbl), "%2u %s", (unsigned)(i + 1), _mod->steps[i].name);
        ktcUiMenuRow(i, lbl, _verdictText(_res[i]), _verdictColor(_res[i]));
    }
    ktcUiButtons(KTC_BTN_RUNALL | KTC_BTN_LIVE);
}

static void _enterMenu() {
    _state = AS_MENU;
    _probeMiss = 0;
    _drawMenu();
}

static void _startStep(uint8_t i) {
    _cur = i;
    _state = AS_RUN;
    memset(_c.v, 0, sizeof(_c.v));
    memset(_c.m, 0, sizeof(_c.m));
    _c.arg = _mod->steps[i].arg;
    _c.entry = true;
    _c.phase = 0;
    _c.t0 = millis();
    _c.phaseEntry = true;
    _c.autoRead = true;
    _c.fresh = false;
    _c.note[0] = '\0';
    char right[12];
    snprintf(right, sizeof(right), "%u/%u", (unsigned)(i + 1), (unsigned)_stepCount());
    _drawTop(right);
    ktcUiClearBody();
    ktcUiHeading(_mod->steps[i].name);
    ktcShow(_c, nullptr, 0);
}

static void _cleanup() {
    ktcCommand1(_mod->addr, KMC_CMD_BULB_TEST, 0x00);
    if (_mod->cleanup) _mod->cleanup(_c);
}

static void _drawSummary() {
    uint8_t fail = 0, skip = 0;
    for (uint8_t i = 0; i < _stepCount(); i++) {
        if (_res[i] == KTC_FAIL) fail++;
        if (_res[i] == KTC_SKIP) skip++;
    }
    _drawTop(fail ? "FAIL" : (skip ? "INCOMPLETE" : "PASS"));
    ktcUiClearBody();
    for (uint8_t i = 0; i < _stepCount(); i++) {
        char lbl[34];
        if (_res[i] == KTC_FAIL && _resNote[i][0])
            snprintf(lbl, sizeof(lbl), "%2u %s", (unsigned)(i + 1), _resNote[i]);
        else
            snprintf(lbl, sizeof(lbl), "%2u %s", (unsigned)(i + 1), _mod->steps[i].name);
        ktcUiMenuRow(i, lbl, _verdictText(_res[i]), _verdictColor(_res[i]));
    }
    ktcUiButtons(KTC_BTN_MENU | KTC_BTN_RUNALL);
}

static void _finishRun() {
    _cleanup();
    _reportSummary();
    _state = AS_SUMMARY;
    _drawSummary();
}

static void _showResult(KtcVerdict v) {
    _res[_cur] = v;
    strncpy(_resNote[_cur], _c.note, sizeof(_resNote[_cur]) - 1);
    _resNote[_cur][sizeof(_resNote[_cur]) - 1] = '\0';
    _reportStep(_cur);
    ktcUiHeading(_mod->steps[_cur].name, _verdictColor(v));
    char msg[48];
    snprintf(msg, sizeof(msg), "%s%s%s", _verdictText(v), _c.note[0] ? ": " : "", _c.note);
    ktcUiToast(msg, _verdictColor(v));
    _state = AS_RESULT;
    _tState = millis();
    if (v == KTC_FAIL) {
        ktcUiButtons(_runAll ? (KTC_BTN_NEXT | KTC_BTN_RETRY | KTC_BTN_ABORT)
                             : (KTC_BTN_MENU | KTC_BTN_RETRY));
    } else {
        ktcUiButtons(_runAll ? KTC_BTN_ABORT : (KTC_BTN_MENU | KTC_BTN_RETRY));
    }
}

static void _advance() {
    if (_runAll && _cur + 1 < _stepCount()) _startStep(_cur + 1);
    else if (_runAll) _finishRun();
    else { _cleanup(); _enterMenu(); }
}

static void _drawLiveStatic() {
    _drawTop("live");
    ktcUiClearBody();
    ktcUiHeading("Live view");
    ktcUiInstr("Packets are read when the module asserts INT. ENABLE sends CMD_ENABLE.");
    ktcUiButtons(KTC_BTN_ENABLE | KTC_BTN_BULB | KTC_BTN_BACK);
}

static void _drawLiveData() {
    ktcUiRowf(0, KTC_C_TEXT, "INT %s   packets %lu   rejected %lu",
              ktcIntAsserted() ? "LOW " : "high", (unsigned long)_c.pktCount,
              (unsigned long)_c.badReads);
    if (_c.pktLen) {
        char hex[48];
        int o = 0;
        for (uint8_t i = 0; i < _c.pktLen && o < 44; i++) o += snprintf(hex + o, sizeof(hex) - o, "%02X ", _c.pkt[i]);
        ktcUiRow(1, hex, KTC_C_ACCENT);
        ktcUiRowf(2, KTC_C_TEXT, "%s%s  tx %u", ktcLifecycleName(_c.lifecycle),
                  _c.fault ? "  FAULT" : "", _c.pkt[2]);
        if (_mod->liveDecode) _mod->liveDecode(_c);
    }
}

// ------------------------------------------------------------
//  Entry points
// ------------------------------------------------------------
void ktcBegin(const KtcModuleDef& def) {
    _mod = &def;
    memset(&_c, 0, sizeof(_c));
    _c.mod = _mod;
    _c.addr = _mod->addr;
    _c.autoRead = true;
    for (uint8_t i = 0; i < KTC_MAX_STEPS; i++) { _res[i] = KTC_RUNNING; _resNote[i][0] = '\0'; }
    Serial.begin(115200);
    ktcBusBegin();
    ktcUiBegin();
    ktcUiTitle(_mod->name, "tester");
    ktcUiTextC(KTC_W / 2, 110, _mod->name, KTC_C_TEXT, 2);
    ktcUiTextC(KTC_W / 2, 136, "Module Tester", KTC_C_DIM, 1);
    char l[40];
    snprintf(l, sizeof(l), "address 0x%02X  type 0x%02X", _mod->addr, _mod->typeId);
    ktcUiTextC(KTC_W / 2, 160, l, KTC_C_ACCENT, 1);
    if (_mod->board) ktcUiTextC(KTC_W / 2, 176, _mod->board, KTC_C_DIM, 1);
    ktcUiTextC(KTC_W / 2, 200, "tester core v" KTC_VERSION_STR, KTC_C_DIM, 1);
    _state = AS_SPLASH;
    _tState = millis();
}

void ktcLoop() {
    uint32_t now = millis();
    _c.now = now;
    bool drawPower = (now - _tPower >= KTC_POWER_POLL_MS);
    if (drawPower) { _tPower = now; _pollPower(_state != AS_SPLASH); }

    switch (_state) {
    case AS_SPLASH:
        if (now - _tState >= 1500) _enterWait();
        break;

    case AS_WAIT: {
        if (now - _tProbe < KTC_PROBE_MS && _tProbe != 0) break;
        _tProbe = now;
        uint8_t msg = 0;   // 0 nothing, 1 wrong module, 2 bad identity
        char other[40] = "";
        if (ktcProbe(_mod->addr)) {
            _id = ktcIdentify(_mod->addr);
            if (_id.valid && _id.typeId == _mod->typeId) {
                Serial.print(F("Module found: ")); Serial.print(_mod->name);
                Serial.print(F(" fw ")); Serial.print(_id.fwMajor); Serial.print('.');
                Serial.println(_id.fwMinor);
                _enterMenu();
                break;
            }
            msg = 2;
            if (_id.valid) {
                msg = 1;
                snprintf(other, sizeof(other), "%s at 0x%02X", ktcTypeName(_id.typeId), _mod->addr);
            }
        } else {
            uint8_t found[15];
            uint8_t n = ktcScan(found, sizeof(found));
            if (n) {
                KtcIdentity o = ktcIdentify(found[0]);
                if (o.valid) {
                    msg = 1;
                    snprintf(other, sizeof(other), "%s at 0x%02X", ktcTypeName(o.typeId), found[0]);
                }
            }
        }
        if (msg != _waitShown || msg == 1) {
            _waitShown = msg;
            ktcUiClearBody();
            ktcUiHeading("Connect module");
            char l[40];
            snprintf(l, sizeof(l), "Waiting for %s at address 0x%02X.", _mod->name, _mod->addr);
            ktcUiInstr(l);
            if (msg == 1) {
                ktcUiRow(1, "Found a different module:", KTC_C_WARN);
                ktcUiRow(2, other, KTC_C_WARN);
                ktcUiRow(4, "Load the tester sketch for that module,", KTC_C_DIM);
                ktcUiRow(5, "or connect the right one.", KTC_C_DIM);
            } else if (msg == 2) {
                ktcUiRow(1, "Address answers but identity is bad.", KTC_C_FAIL);
                ktcUiRow(2, "Check SDA/SCL and the module firmware.", KTC_C_DIM);
            } else {
                ktcUiRow(1, "No module on the bus.", KTC_C_DIM);
            }
        }
        break;
    }

    case AS_MENU: {
        uint16_t b = 0;
        int row = ktcUiPollMenu(_stepCount(), b);
        if (row >= 0) {
            _runAll = false;
            Serial.print(F("Single step: ")); Serial.println(_mod->steps[row].name);
            _startStep((uint8_t)row);
            break;
        }
        if (b == KTC_BTN_RUNALL) {
            _runAll = true;
            for (uint8_t i = 0; i < KTC_MAX_STEPS; i++) { _res[i] = KTC_RUNNING; _resNote[i][0] = '\0'; }
            _reportHeader();
            _startStep(0);
            break;
        }
        if (b == KTC_BTN_LIVE) {
            _state = AS_LIVE;
            _bulbLive = false;
            _drawLiveStatic();
            _drawLiveData();
            break;
        }
        // Presence check once a second; back to WAIT after 3 misses.
        if (now - _tProbe >= 1000) {
            _tProbe = now;
            if (ktcProbe(_mod->addr)) _probeMiss = 0;
            else if (++_probeMiss >= 3) _enterWait();
        }
        break;
    }

    case AS_RUN: {
        _c.btn = ktcUiPollButtons();
        _c.fresh = false;
        if (_c.btn == KTC_BTN_ABORT) {
            _cleanup();
            Serial.println(F("Run aborted by operator."));
            _c.note[0] = '\0';
            if (_runAll) { _res[_cur] = KTC_RUNNING; _finishRun(); }
            else _enterMenu();
            break;
        }
        if (_c.btn == KTC_BTN_SKIP) {
            _cleanup();
            _c.note[0] = '\0';
            ktcNote(_c, "skipped by operator");
            _showResult(KTC_SKIP);
            break;
        }
        _autoRead();
        _phaseSet = false;
        KtcVerdict v = _mod->steps[_cur].fn(_c);
        _c.entry = false;
        _c.phaseEntry = _phaseSet;
        if (v != KTC_RUNNING) _showResult(v);
        break;
    }

    case AS_RESULT: {
        uint16_t b = ktcUiPollButtons();
        KtcVerdict v = _res[_cur];
        if (b == KTC_BTN_RETRY) { _startStep(_cur); break; }
        if (b == KTC_BTN_ABORT) { if (_runAll) _finishRun(); else { _cleanup(); _enterMenu(); } break; }
        if (b == KTC_BTN_MENU) { _cleanup(); _enterMenu(); break; }
        if (b == KTC_BTN_NEXT) { _advance(); break; }
        if (_runAll && v != KTC_FAIL && now - _tState >= KTC_RESULT_HOLD_MS) _advance();
        break;
    }

    case AS_SUMMARY: {
        uint16_t b = ktcUiPollButtons();
        if (b == KTC_BTN_MENU) _enterMenu();
        else if (b == KTC_BTN_RUNALL) {
            _runAll = true;
            for (uint8_t i = 0; i < KTC_MAX_STEPS; i++) { _res[i] = KTC_RUNNING; _resNote[i][0] = '\0'; }
            _reportHeader();
            _startStep(0);
        }
        break;
    }

    case AS_LIVE: {
        uint16_t b = ktcUiPollButtons();
        if (b == KTC_BTN_BACK) {
            if (_bulbLive) ktcCommand1(_mod->addr, KMC_CMD_BULB_TEST, 0x00);
            _enterMenu();
            break;
        }
        if (b == KTC_BTN_ENABLE) { ktcCommand(_mod->addr, KMC_CMD_ENABLE); ktcUiToast("CMD_ENABLE sent"); }
        if (b == KTC_BTN_BULB) {
            _bulbLive = !_bulbLive;
            ktcCommand1(_mod->addr, KMC_CMD_BULB_TEST, _bulbLive ? 0x01 : 0x00);
            ktcUiToast(_bulbLive ? "Bulb test on" : "Bulb test off");
        }
        _c.fresh = false;
        _autoRead();
        if (_c.fresh || drawPower) _drawLiveData();
        break;
    }
    }
}
