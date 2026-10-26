/***************************************************************************************
   i2c_bus.cpp — bounded, counted I2C on the BUS.

   The Teensy 4 Wire library bounds every transaction itself; what it cannot do is free
   a bus that a device holds low (a reset mid-byte leaves the slave driving SDA). That
   shows as SDA low with no transfer in progress, and is freed by clocking SCL until
   the slave lets go, then a STOP (the standard 9-clock recovery).
****************************************************************************************/
#include "i2c_bus.h"
#include "config.h"
#include "debug.h"
#include <Wire.h>

static BusStats g_stats;

const BusStats &busStats() { return g_stats; }

const char *busResultName(BusResult r) {
  switch (r) {
    case BUS_OK:        return "ok";
    case BUS_TOO_LONG:  return "too long";
    case BUS_NACK_ADDR: return "no answer";
    case BUS_NACK_DATA: return "data NACK";
    case BUS_OTHER:     return "bus error";
    case BUS_SHORT:     return "short read";
  }
  return "?";
}

static void countError(BusResult r, uint8_t addr, const char *what) {
  switch (r) {
    case BUS_NACK_ADDR: g_stats.nackAddr++; break;
    case BUS_NACK_DATA: g_stats.nackData++; break;
    case BUS_SHORT:     g_stats.shortReads++; break;
    default:            g_stats.other++; break;
  }
  dbg(DBG_I2C, "%s 0x%02X: %s", what, addr, busResultName(r));
}

bool busRecover() {
  pinMode(PIN_SDA_BUS, INPUT);
  if (digitalRead(PIN_SDA_BUS) == HIGH) return true;   // nothing to free
  g_stats.recoveries++;
  pinMode(PIN_SCL_BUS, OUTPUT);
  for (uint8_t i = 0; i < 9 && digitalRead(PIN_SDA_BUS) == LOW; i++) {
    digitalWrite(PIN_SCL_BUS, LOW);  delayMicroseconds(5);
    digitalWrite(PIN_SCL_BUS, HIGH); delayMicroseconds(5);
  }
  // STOP: SDA low → high while SCL is high.
  pinMode(PIN_SDA_BUS, OUTPUT);
  digitalWrite(PIN_SDA_BUS, LOW);  delayMicroseconds(5);
  digitalWrite(PIN_SCL_BUS, HIGH); delayMicroseconds(5);
  digitalWrite(PIN_SDA_BUS, HIGH); delayMicroseconds(5);
  pinMode(PIN_SDA_BUS, INPUT);
  pinMode(PIN_SCL_BUS, INPUT);
  bool freed = digitalRead(PIN_SDA_BUS) == HIGH;
  Wire.begin();
  Wire.setClock(I2C_CLOCK_HZ);
  if (freed) dbgErr(DBG_I2C, "bus held low: recovered");
  else       dbgErr(DBG_I2C, "bus held low: recovery failed (SDA still low)");
  return freed;
}

void busInit() {
  memset(&g_stats, 0, sizeof g_stats);
  busRecover();
  Wire.begin();
  Wire.setClock(I2C_CLOCK_HZ);
}

BusResult busWrite(uint8_t addr, const uint8_t *buf, uint8_t n) {
  g_stats.writes++;
  Wire.beginTransmission(addr);
  if (n) Wire.write(buf, n);
  uint8_t r = Wire.endTransmission();
  BusResult res = (r <= BUS_OTHER) ? (BusResult)r : BUS_OTHER;
  if (res != BUS_OK) countError(res, addr, "write");
  return res;
}

BusResult busRead(uint8_t addr, uint8_t *buf, uint8_t n) {
  g_stats.reads++;
  uint8_t got = Wire.requestFrom(addr, n);
  uint8_t i = 0;
  while (Wire.available() && i < n) buf[i++] = (uint8_t)Wire.read();
  while (Wire.available()) Wire.read();
  if (got == 0 && i == 0) { countError(BUS_NACK_ADDR, addr, "read"); return BUS_NACK_ADDR; }
  if (i < n) { countError(BUS_SHORT, addr, "read"); return BUS_SHORT; }
  return BUS_OK;
}

BusResult busReadReg(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n) {
  g_stats.writes++;
  Wire.beginTransmission(addr);
  Wire.write(reg);
  uint8_t r = Wire.endTransmission(false);    // repeated start
  if (r != 0) {
    BusResult res = (r <= BUS_OTHER) ? (BusResult)r : BUS_OTHER;
    countError(res, addr, "reg");
    return res;
  }
  return busRead(addr, buf, n);
}

bool busProbe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}
