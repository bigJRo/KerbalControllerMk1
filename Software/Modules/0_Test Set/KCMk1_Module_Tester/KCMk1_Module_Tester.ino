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

// Presence cache with hysteresis. A module is identified ONCE when it
// first appears (identity retried within the scan), then only presence-
// probed — the ATtiny targets answer I2C from an ISR and an occasional
// busy/glitched transaction must not drop them from the list. An entry
// is removed only after MISS_LIMIT consecutive failed probes (real
// unplug), keeping the list rock-steady through transient bus noise.
struct SlotCache {
    bool    used;
    uint8_t addr;
    uint8_t typeId;
    uint8_t misses;
};
static SlotCache _cache[MAX_FOUND];
static const uint8_t MISS_LIMIT = 3;   // consecutive misses before removal

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
static bool _cacheHas(uint8_t addr) {
    for (uint8_t i = 0; i < MAX_FOUND; i++)
        if (_cache[i].used && _cache[i].addr == addr) return true;
    return false;
}

static void doScan() {
    uint8_t rawAddrs[MAX_FOUND];
    uint8_t rawCount = hwScanModules(rawAddrs, MAX_FOUND);

    // 1. Update presence for cached entries: reset the miss counter when
    //    the probe saw the address; otherwise count a miss and remove the
    //    entry only after MISS_LIMIT consecutive misses (real unplug).
    for (uint8_t i = 0; i < MAX_FOUND; i++) {
        if (!_cache[i].used) continue;
        bool present = false;
        for (uint8_t j = 0; j < rawCount; j++)
            if (rawAddrs[j] == _cache[i].addr) { present = true; break; }
        if (present) {
            _cache[i].misses = 0;
        } else if (++_cache[i].misses >= MISS_LIMIT) {
            _cache[i].used = false;
        }
    }

    // 2. Identify NEW addresses only (identity is cached afterwards — the
    //    module is not re-interrogated every scan). A real module always
    //    answers CMD_GET_IDENTITY, so requiring a sane reply here blocks
    //    phantom ACKs (no reply -> t:00, floating bus -> t:FF) without
    //    hiding real hardware; the read is retried once within the scan.
    for (uint8_t j = 0; j < rawCount; j++) {
        if (_cacheHas(rawAddrs[j])) continue;
        ModuleIdentity id = hwIdentify(rawAddrs[j]);
        if (!id.valid) id = hwIdentify(rawAddrs[j]);   // one retry
        if (!id.valid || id.typeId == 0x00 || id.typeId == 0xFF) continue;
        for (uint8_t i = 0; i < MAX_FOUND; i++) {
            if (_cache[i].used) continue;
            _cache[i] = { true, rawAddrs[j], id.typeId, 0 };
            break;
        }
    }

    // 3. Build the display list from the cache (ordered by address).
    _foundCount = 0;
    for (uint8_t a = I2C_MODULE_ADDR_MIN; a <= I2C_MODULE_ADDR_MAX; a++) {
        for (uint8_t i = 0; i < MAX_FOUND; i++) {
            if (!_cache[i].used || _cache[i].addr != a) continue;
            _foundAddrs[_foundCount] = _cache[i].addr;
            _foundTypes[_foundCount] = _cache[i].typeId;
            _foundInfos[_foundCount] = catalogByType(_cache[i].typeId);
            _foundCount++;
            break;
        }
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

/** @brief Force a fresh scan: clear the presence cache (full re-identify)
 *         and force the next doScan() to redraw. Call after uiScanBegin()
 *         and on the Rescan button. */
static void scanMarkDirty() {
    _prevCount = 0xFF;
    for (uint8_t i = 0; i < MAX_FOUND; i++) _cache[i].used = false;
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
            if (idx == -2) { scanMarkDirty(); _tScan = 0; }   // Rescan: full re-identify
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
