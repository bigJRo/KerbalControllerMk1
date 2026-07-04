/**
 * @file        KCMk1_Module_Tester.ino
 * @version     2.0.0
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Touchscreen field-validation tester for all Kerbal Controller
 *              Mk1 I2C target modules. Runs on the KC-01-9001 v2.0 board
 *              (Seeed XIAO RA4M1, 2.8" ST7789V + FT6236 capacitive touch,
 *              INA228 power monitor).
 *
 *              Flow:
 *                SPLASH  -> brief boot screen
 *                SCAN    -> rescan 0x20-0x2E, identify, list modules; the
 *                           top bar shows live module-supply V / I / W.
 *                           Touch a module to open its dashboard.
 *                DASHBOARD-> live input view (buttons / axes / encoders /
 *                           value), lifecycle/fault/tx header, and control
 *                           buttons (Enable, Disable, Sleep, Wake, Reset,
 *                           Bulb, LED-cycle, Back).
 *
 *              All protocol constants come from the shared KerbalModuleCommon
 *              library so the tester cannot drift from the modules. Hardware
 *              I/O is in TesterHW; UI in TesterUI; module metadata in
 *              ModuleCatalog.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */

#include <Arduino.h>
#include <KerbalModuleCommon.h>
#include "TesterConfig.h"
#include "ModuleCatalog.h"
#include "TesterHW.h"
#include "TesterUI.h"
#include "ConstructionTest.h"

// ============================================================
//  App state
// ============================================================
enum AppState : uint8_t { ST_SPLASH, ST_SCAN, ST_DASHBOARD, ST_CONSTRUCTION };
static AppState _state = ST_SPLASH;

// Scan results
static const uint8_t MAX_FOUND = (I2C_MODULE_ADDR_MAX - I2C_MODULE_ADDR_MIN + 1);
static uint8_t            _foundAddrs[MAX_FOUND];
static uint8_t            _foundTypes[MAX_FOUND];
static const ModuleInfo*  _foundInfos[MAX_FOUND];
static uint8_t            _foundCount = 0;

// Selected module
static const ModuleInfo*  _sel     = nullptr;
static uint8_t            _selAddr = 0;
static uint8_t            _selPkt  = 0;   // expected packet size

// Timers
static uint32_t _tScan = 0, _tPower = 0, _tInput = 0, _tSplash = 0;

// Last good power reading — a transient INA228 read failure keeps showing
// the previous values instead of flashing zeros/dashes.
static PowerReading _lastPower = {0, 0, 0, false};

// Previous scan result signature — the scan list only redraws when the
// result set changes (stops the empty-state message flashing every rescan).
static uint8_t _prevCount = 0xFF;   // 0xFF = force first draw
static uint8_t _prevAddrs[MAX_FOUND];
static uint8_t _prevTypes[MAX_FOUND];

// LED-cycle test state
static uint8_t _ledCycleState = 0;   // current KMC_LED_* being applied

// ============================================================
//  setup()
// ============================================================
void setup() {
    hwBegin();
    uiBegin();
    uiSplash();
    _tSplash = millis();
    _state   = ST_SPLASH;
}

// ============================================================
//  Power poll shared by scan + dashboard: keep the last good
//  reading on transient INA228 failures.
// ============================================================
static void pollPower() {
    PowerReading p = hwReadPower();
    if (p.ok) _lastPower = p;
    uiPowerBar(_lastPower.ok ? _lastPower : p);
}

// ============================================================
//  Scan + identify the bus into the _found* tables.
//  Redraws the list only when the result set changed.
// ============================================================
static void doScan() {
    uint8_t rawAddrs[MAX_FOUND];
    uint8_t rawCount = hwScanModules(rawAddrs, MAX_FOUND);

    // Keep only devices with a sane identity reply. A real module always
    // answers CMD_GET_IDENTITY (in every lifecycle state), so this drops
    // phantom ACKs (no reply -> t:00, floating-bus reads -> t:FF) without
    // hiding real hardware. An identified-but-uncatalogued type is kept
    // and shown as Unknown — that's genuinely attached hardware.
    _foundCount = 0;
    for (uint8_t i = 0; i < rawCount; i++) {
        ModuleIdentity id = hwIdentify(rawAddrs[i]);
        if (!id.valid || id.typeId == 0x00 || id.typeId == 0xFF) continue;
        _foundAddrs[_foundCount] = rawAddrs[i];
        _foundTypes[_foundCount] = id.typeId;
        _foundInfos[_foundCount] = catalogByType(id.typeId);
        _foundCount++;
    }

    bool changed = (_foundCount != _prevCount);
    for (uint8_t i = 0; !changed && i < _foundCount; i++) {
        changed = (_foundAddrs[i] != _prevAddrs[i]) ||
                  (_foundTypes[i] != _prevTypes[i]);
    }
    if (!changed) return;

    _prevCount = _foundCount;
    memcpy(_prevAddrs, _foundAddrs, _foundCount);
    memcpy(_prevTypes, _foundTypes, _foundCount);
    uiScanList(_foundInfos, _foundAddrs, _foundTypes, _foundCount);
}

/** @brief Force the next doScan() to redraw (call after uiScanBegin()). */
static void scanMarkDirty() { _prevCount = 0xFF; }

// ============================================================
//  Build and send a uniform LED-state payload to all positions
// ============================================================
static void sendLedAll(uint8_t ledState) {
    uint8_t payload[KMC_LED_PAYLOAD_SIZE];
    memset(payload, 0, sizeof(payload));
    for (uint8_t b = 0; b < 16; b++) kmcLedPackSet(payload, b, ledState);
    hwSendCommand(_selAddr, KMC_CMD_SET_LED_STATE, payload, KMC_LED_PAYLOAD_SIZE);
}

// ============================================================
//  Dashboard control action handler
// ============================================================
static void handleAction(UIAction a) {
    switch (a) {
        case UI_ENABLE:   hwSendCommand(_selAddr, KMC_CMD_ENABLE,  nullptr, 0); uiToast("ENABLE");  break;
        case UI_DISABLE:  hwSendCommand(_selAddr, KMC_CMD_DISABLE, nullptr, 0); uiToast("DISABLE"); break;
        case UI_SLEEP:    hwSendCommand(_selAddr, KMC_CMD_SLEEP,   nullptr, 0); uiToast("SLEEP");   break;
        case UI_WAKE:     hwSendCommand(_selAddr, KMC_CMD_WAKE,    nullptr, 0); uiToast("WAKE");    break;
        case UI_RESET:    hwSendCommand(_selAddr, KMC_CMD_RESET,   nullptr, 0); uiToast("RESET");   break;
        case UI_BULB:     hwSendCommand(_selAddr, KMC_CMD_BULB_TEST, nullptr, 0); uiToast("BULB");  break;
        case UI_LEDCYCLE: {
            // Step ENABLED -> ACTIVE -> WARNING -> ALERT -> ARMED ->
            // PARTIAL_DEPLOY -> CUT -> ACTIVE_ALT -> (wrap)
            static const uint8_t seq[] = {
                KMC_LED_ENABLED, KMC_LED_ACTIVE, KMC_LED_WARNING, KMC_LED_ALERT,
                KMC_LED_ARMED, KMC_LED_PARTIAL_DEPLOY, KMC_LED_CUT, KMC_LED_ACTIVE_ALT
            };
            _ledCycleState = (_ledCycleState + 1) % (sizeof(seq) / sizeof(seq[0]));
            sendLedAll(seq[_ledCycleState]);
            uiToast("LED CYCLE");
            break;
        }
        case UI_TEST:
            ctBegin(_sel, _selAddr);
            _state = ST_CONSTRUCTION;
            break;
        case UI_BACK:
            _sel = nullptr;
            _state = ST_SCAN;
            uiScanBegin();
            scanMarkDirty();
            _tScan = 0;   // force immediate rescan
            break;
        default: break;
    }
}

// ============================================================
//  loop()
// ============================================================
void loop() {
    uint32_t now = millis();

    switch (_state) {

        case ST_SPLASH:
            if (now - _tSplash >= SPLASH_MS) {
                _state = ST_SCAN;
                uiScanBegin();
                scanMarkDirty();
                _tScan = 0;
            }
            break;

        case ST_SCAN: {
            if (now - _tScan >= SCAN_INTERVAL_MS) { _tScan = now; doScan(); }
            if (now - _tPower >= POWER_POLL_MS)   { _tPower = now; pollPower(); }

            int idx = uiScanTouch(_foundCount);
            if (idx == -2) { _tScan = 0; }                 // Rescan
            else if (idx >= 0 && idx < (int)_foundCount) {
                if (_foundInfos[idx]) {
                    _sel     = _foundInfos[idx];
                    _selAddr = _foundAddrs[idx];
                    _selPkt  = kindPacketSize(_sel->kind);
                    _ledCycleState = 0;
                    _state   = ST_DASHBOARD;
                    uiDashboardBegin(_sel, _selAddr);
                    _tInput = 0;
                } else {
                    uiToast("Unknown module type");
                }
            }
            break;
        }

        case ST_DASHBOARD: {
            // Poll the module packet on INT or on the input interval.
            if (hwModuleIntAsserted() || (now - _tInput >= INPUT_POLL_MS)) {
                _tInput = now;
                uint8_t pkt[16];
                uint8_t got = hwReadPacket(_selAddr, pkt, _selPkt);
                if (got >= KMC_HEADER_SIZE) {
                    ModuleState st;
                    hwParsePacket(_sel, pkt, got, st);
                    uiDashboardHeader(st);
                    uiDashboardInputs(_sel, st);
                }
            }
            if (now - _tPower >= POWER_POLL_MS) { _tPower = now; pollPower(); }

            handleAction(uiDashboardTouch());
            break;
        }

        case ST_CONSTRUCTION: {
            ctUpdate();
            if (!ctActive()) {
                // Test finished or aborted — return to the dashboard.
                _state = ST_DASHBOARD;
                uiDashboardBegin(_sel, _selAddr);
                _tInput = 0;
                _ledCycleState = 0;
            }
            break;
        }
    }
}
