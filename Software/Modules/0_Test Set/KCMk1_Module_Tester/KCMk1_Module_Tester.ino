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
 *              (Seeed XIAO RA4M1, 2.8" ILI9341 + FT6236 capacitive touch,
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
//  Scan + identify the bus into the _found* tables
// ============================================================
static void doScan() {
    _foundCount = hwScanModules(_foundAddrs, MAX_FOUND);
    for (uint8_t i = 0; i < _foundCount; i++) {
        ModuleIdentity id = hwIdentify(_foundAddrs[i]);
        _foundTypes[i] = id.valid ? id.typeId : 0x00;
        _foundInfos[i] = id.valid ? catalogByType(id.typeId) : nullptr;
    }
    uiScanList(_foundInfos, _foundAddrs, _foundTypes, _foundCount);
}

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
                _tScan = 0;
            }
            break;

        case ST_SCAN: {
            if (now - _tScan >= SCAN_INTERVAL_MS) { _tScan = now; doScan(); }
            if (now - _tPower >= POWER_POLL_MS)   { _tPower = now; uiPowerBar(hwReadPower()); }

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
            if (now - _tPower >= POWER_POLL_MS) { _tPower = now; uiPowerBar(hwReadPower()); }

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
