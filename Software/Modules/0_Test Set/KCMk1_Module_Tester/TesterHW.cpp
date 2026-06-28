/**
 * @file        TesterHW.cpp
 * @version     2.0.0
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Hardware I/O implementation: shared-bus I2C controller ops,
 *              packet parsing to the current protocol (v2.9), and the INA228
 *              power monitor.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */

#include "TesterHW.h"
#include "TesterConfig.h"
#include <Wire.h>

// ============================================================
//  INA228 register map (subset)
// ============================================================
#define INA228_REG_CONFIG      0x00
#define INA228_REG_ADC_CONFIG  0x01
#define INA228_REG_SHUNT_CAL   0x02
#define INA228_REG_VBUS        0x05
#define INA228_REG_CURRENT     0x07
#define INA228_REG_POWER       0x08

static float _currentLSB = 0.0f;   // amps per LSB, set in _ina228Begin()

// ------------------------------------------------------------
//  Low-level INA228 register access (big-endian)
// ------------------------------------------------------------
static bool _ina228Write16(uint8_t reg, uint16_t val) {
    Wire.beginTransmission(I2C_ADDR_INA228);
    Wire.write(reg);
    Wire.write((uint8_t)(val >> 8));
    Wire.write((uint8_t)(val & 0xFF));
    return Wire.endTransmission() == 0;
}

static uint32_t _ina228Read(uint8_t reg, uint8_t nbytes) {
    Wire.beginTransmission(I2C_ADDR_INA228);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0;
    uint32_t v = 0;
    uint8_t got = Wire.requestFrom((int)I2C_ADDR_INA228, (int)nbytes);
    for (uint8_t i = 0; i < got; i++) v = (v << 8) | (uint8_t)Wire.read();
    return v;
}

static void _ina228Begin() {
    // CURRENT_LSB = max current / 2^19; SHUNT_CAL per datasheet (ADCRANGE=0).
    _currentLSB = INA228_MAX_CURRENT_A / 524288.0f;
    float calF = 13107200000.0f * _currentLSB * INA228_RSHUNT_OHMS;
    uint16_t cal = (uint16_t)(calF + 0.5f);
    // Default CONFIG (ADCRANGE=0) and continuous ADC_CONFIG are fine.
    _ina228Write16(INA228_REG_SHUNT_CAL, cal);
}

// ============================================================
//  hwBegin()
// ============================================================
void hwBegin() {
    pinMode(PIN_INT_BUS, INPUT_PULLUP);
    pinMode(PIN_MOD_RST, OUTPUT);
    digitalWrite(PIN_MOD_RST, HIGH);   // de-asserted (active low)

    Wire.begin();
    Wire.setClock(I2C_BUS_HZ);

    _ina228Begin();
}

// ============================================================
//  hwPulseModuleReset()
// ============================================================
void hwPulseModuleReset() {
    digitalWrite(PIN_MOD_RST, LOW);
    delay(5);
    digitalWrite(PIN_MOD_RST, HIGH);
    delay(5);
}

// ============================================================
//  hwModuleIntAsserted()
// ============================================================
bool hwModuleIntAsserted() {
    return digitalRead(PIN_INT_BUS) == LOW;
}

// ============================================================
//  hwScanModules()
// ============================================================
uint8_t hwScanModules(uint8_t* outAddrs, uint8_t maxAddrs) {
    uint8_t n = 0;
    for (uint8_t a = I2C_MODULE_ADDR_MIN; a <= I2C_MODULE_ADDR_MAX && n < maxAddrs; a++) {
        if (a == I2C_ADDR_FT6236 || a == I2C_ADDR_INA228) continue; // on-board
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) outAddrs[n++] = a;
    }
    return n;
}

// ============================================================
//  hwIdentify()
// ============================================================
ModuleIdentity hwIdentify(uint8_t addr) {
    ModuleIdentity id = {0, 0, 0, 0, false};
    Wire.beginTransmission(addr);
    Wire.write(KMC_CMD_GET_IDENTITY);
    if (Wire.endTransmission() != 0) return id;
    delayMicroseconds(200);
    uint8_t got = Wire.requestFrom((int)addr, (int)KMC_IDENTITY_SIZE);
    if (got >= KMC_IDENTITY_SIZE) {
        id.typeId  = Wire.read();
        id.fwMajor = Wire.read();
        id.fwMinor = Wire.read();
        id.caps    = Wire.read();
        id.valid   = true;
    }
    return id;
}

// ============================================================
//  hwSendCommand()
// ============================================================
uint8_t hwSendCommand(uint8_t addr, uint8_t cmd, const uint8_t* payload, uint8_t len) {
    Wire.beginTransmission(addr);
    Wire.write(cmd);
    for (uint8_t i = 0; i < len; i++) Wire.write(payload[i]);
    return Wire.endTransmission();
}

// ============================================================
//  hwReadPacket()
// ============================================================
uint8_t hwReadPacket(uint8_t addr, uint8_t* buf, uint8_t n) {
    uint8_t got = Wire.requestFrom((int)addr, (int)n);
    uint8_t i = 0;
    while (Wire.available() && i < n) buf[i++] = Wire.read();
    return i;
}

// ============================================================
//  hwParsePacket() — header + kind-specific payload
// ============================================================
static int16_t be16(const uint8_t* p) { return (int16_t)((p[0] << 8) | p[1]); }

void hwParsePacket(const ModuleInfo* info, const uint8_t* pkt, uint8_t n, ModuleState& out) {
    memset(&out, 0, sizeof(out));
    if (n < KMC_HEADER_SIZE) return;

    uint8_t status  = pkt[0];
    out.lifecycle   = status & KMC_STATUS_LIFECYCLE_MASK;
    out.fault       = (status & KMC_STATUS_FAULT) != 0;
    out.dataChanged = (status & KMC_STATUS_DATA_CHANGED) != 0;
    out.txCounter   = pkt[2];

    const uint8_t* p = pkt + KMC_HEADER_SIZE;   // payload start
    uint8_t avail = (n > KMC_HEADER_SIZE) ? (n - KMC_HEADER_SIZE) : 0;
    if (!info) return;

    switch (info->kind) {
        case MK_BUTTON12:
            if (avail >= 4) {
                out.events = (uint32_t)p[0] | ((uint32_t)p[1] << 8);
                out.change = (uint32_t)p[2] | ((uint32_t)p[3] << 8);
            }
            break;

        case MK_BUTTON24:
            if (avail >= 6) {
                out.events = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
                out.change = (uint32_t)p[3] | ((uint32_t)p[4] << 8) | ((uint32_t)p[5] << 16);
            }
            break;

        case MK_JOYSTICK:
            if (avail >= 9) {
                out.events  = p[0];          // bit0 BTN_JOY, bit1 BTN01, bit2 BTN02
                out.change  = p[1];
                out.flags   = p[2];          // persistent button state
                out.axis[0] = be16(&p[3]);
                out.axis[1] = be16(&p[5]);
                out.axis[2] = be16(&p[7]);
            }
            break;

        case MK_DISPLAY:
            if (avail >= 5) {
                out.events = p[0];           // BTN01-03 events
                out.change = p[1];
                out.flags  = p[2];           // state byte (cycle/toggle bits)
                out.value  = ((uint16_t)p[3] << 8) | p[4];
            }
            break;

        case MK_THROTTLE:
            if (avail >= 4) {
                out.flags  = p[0];           // enabled/precision/touch/motor
                out.events = p[1];           // 4 button events
                out.value  = ((uint16_t)p[2] << 8) | p[3];
            }
            break;

        case MK_DUAL_ENCODER:
            if (avail >= 4) {
                out.events = p[0];           // 2 button events
                out.change = p[1];
                out.enc[0] = (int8_t)p[2];
                out.enc[1] = (int8_t)p[3];
            }
            break;
    }
}

// ============================================================
//  hwReadPower() — INA228
// ============================================================
PowerReading hwReadPower() {
    PowerReading r = {0, 0, 0, false};

    // VBUS: 24-bit, bits 23..4 are data, LSB = 195.3125 uV.
    uint32_t vbusRaw = _ina228Read(INA228_REG_VBUS, 3) >> 4;
    r.volts = (float)vbusRaw * 195.3125e-6f;

    // CURRENT: 24-bit, bits 23..4 data, signed 20-bit, LSB = _currentLSB.
    uint32_t curRaw = _ina228Read(INA228_REG_CURRENT, 3) >> 4;
    int32_t  curS   = (curRaw & 0x80000) ? (int32_t)(curRaw | 0xFFF00000) : (int32_t)curRaw;
    r.amps = (float)curS * _currentLSB;

    // POWER: 24-bit unsigned, LSB = 3.2 * _currentLSB.
    uint32_t pwrRaw = _ina228Read(INA228_REG_POWER, 3);
    r.watts = (float)pwrRaw * 3.2f * _currentLSB;

    r.ok = true;
    return r;
}
