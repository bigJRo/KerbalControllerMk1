/**
 * @file        KTC_Bus.cpp
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Bus and power-monitor implementation. The I2C details (double
 *              probe against phantom ACKs, STOP between the INA228 pointer
 *              write and read, identity retry) follow the universal tester,
 *              where they were worked out on the Renesas Wire core.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include "KTC_Bus.h"
#include "KTC_Config.h"
#include <Wire.h>

#define INA228_REG_SHUNT_CAL  0x02
#define INA228_REG_VBUS       0x05
#define INA228_REG_CURRENT    0x07
#define INA228_REG_POWER      0x08

static float _currentLSB = 0.0f;

static bool _inaWrite16(uint8_t reg, uint16_t v) {
    Wire.beginTransmission(KTC_ADDR_INA228);
    Wire.write(reg);
    Wire.write((uint8_t)(v >> 8));
    Wire.write((uint8_t)(v & 0xFF));
    return Wire.endTransmission() == 0;
}

static bool _inaRead(uint8_t reg, uint8_t n, uint32_t& out) {
    Wire.beginTransmission(KTC_ADDR_INA228);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    uint8_t got = Wire.requestFrom((int)KTC_ADDR_INA228, (int)n);
    if (got != n) { while (Wire.available()) Wire.read(); return false; }
    uint32_t v = 0;
    for (uint8_t i = 0; i < n; i++) v = (v << 8) | (uint8_t)Wire.read();
    out = v;
    return true;
}

void ktcBusBegin() {
    pinMode(KTC_PIN_INT_BUS, INPUT_PULLUP);
    pinMode(KTC_PIN_MOD_RST, OUTPUT);
    digitalWrite(KTC_PIN_MOD_RST, HIGH);
    Wire.begin();
    Wire.setClock(KTC_I2C_HZ);
    _currentLSB = KTC_INA228_IMAX / 524288.0f;
    float cal = 13107200000.0f * _currentLSB * KTC_INA228_RSHUNT;
    _inaWrite16(INA228_REG_SHUNT_CAL, (uint16_t)(cal + 0.5f));
}

void ktcResetPulse(uint16_t lowMs) {
    digitalWrite(KTC_PIN_MOD_RST, LOW);
    delay(lowMs);
    digitalWrite(KTC_PIN_MOD_RST, HIGH);
}

bool ktcIntAsserted() {
    return digitalRead(KTC_PIN_INT_BUS) == LOW;
}

bool ktcProbe(uint8_t addr) {
    for (uint8_t i = 0; i < 2; i++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() != 0) return false;
    }
    return true;
}

uint8_t ktcScan(uint8_t* out, uint8_t maxAddrs) {
    uint8_t n = 0;
    for (uint8_t a = KTC_ADDR_MIN; a <= KTC_ADDR_MAX && n < maxAddrs; a++) {
        if (a == KTC_ADDR_FT6236 || a == KTC_ADDR_INA228) continue;
        if (ktcProbe(a)) out[n++] = a;
    }
    return n;
}

static KtcIdentity _identifyOnce(uint8_t addr) {
    KtcIdentity id = {0, 0, 0, 0, false};
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
        // A floating bus reads 0xFF, a silent target 0x00: neither is a module.
        id.valid   = (id.typeId != 0x00 && id.typeId != 0xFF);
    }
    while (Wire.available()) Wire.read();
    return id;
}

KtcIdentity ktcIdentify(uint8_t addr) {
    KtcIdentity id = _identifyOnce(addr);
    if (!id.valid) id = _identifyOnce(addr);
    return id;
}

uint8_t ktcCommand(uint8_t addr, uint8_t cmd, const uint8_t* payload, uint8_t len) {
    Wire.beginTransmission(addr);
    Wire.write(cmd);
    for (uint8_t i = 0; i < len; i++) Wire.write(payload[i]);
    return Wire.endTransmission();
}

uint8_t ktcCommand1(uint8_t addr, uint8_t cmd, uint8_t b) {
    return ktcCommand(addr, cmd, &b, 1);
}

uint8_t ktcRead(uint8_t addr, uint8_t* buf, uint8_t n) {
    uint8_t got = Wire.requestFrom((int)addr, (int)n);
    uint8_t i = 0;
    while (Wire.available() && i < n) buf[i++] = Wire.read();
    while (Wire.available()) Wire.read();
    (void)got;
    return i;
}

void ktcLedFill(uint8_t* payload, uint8_t state) {
    for (uint8_t i = 0; i < KMC_LED_PAYLOAD_SIZE; i++) payload[i] = 0;
    for (uint8_t b = 0; b < 16; b++) kmcLedPackSet(payload, b, state);
}

KtcPower ktcReadPower() {
    KtcPower r = {0, 0, 0, false};
    for (uint8_t attempt = 0; attempt < 2 && !r.ok; attempt++) {
        uint32_t vb, cu, pw;
        if (!_inaRead(INA228_REG_VBUS, 3, vb)) continue;
        if (!_inaRead(INA228_REG_CURRENT, 3, cu)) continue;
        if (!_inaRead(INA228_REG_POWER, 3, pw)) continue;
        r.volts = (float)(vb >> 4) * 195.3125e-6f;
        uint32_t c = cu >> 4;
        int32_t cs = (c & 0x80000) ? (int32_t)(c | 0xFFF00000) : (int32_t)c;
        r.amps  = (float)cs * _currentLSB;
        r.watts = (float)pw * 3.2f * _currentLSB;
        r.ok = true;
    }
    return r;
}
