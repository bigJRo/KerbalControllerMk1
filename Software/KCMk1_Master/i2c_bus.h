/***************************************************************************************
   i2c_bus.h — every master I2C transaction on the BUS (Wire, pins 18/19) goes through
   here, so errors are counted and logged in one place (debug category I2C) and a bus
   held low can be recovered.
****************************************************************************************/
#pragma once
#include <Arduino.h>

enum BusResult : uint8_t {
  BUS_OK = 0,
  BUS_TOO_LONG = 1,       // Wire: data too long for the buffer
  BUS_NACK_ADDR = 2,      // nobody answered the address
  BUS_NACK_DATA = 3,
  BUS_OTHER = 4,          // Wire: other error / timeout
  BUS_SHORT = 5           // read returned fewer bytes than asked
};

struct BusStats {
  uint32_t writes, reads, nackAddr, nackData, other, shortReads, recoveries;
};

void        busInit();                                         // Wire.begin + clock; recovers a stuck bus first
BusResult   busWrite(uint8_t addr, const uint8_t *buf, uint8_t n);
BusResult   busRead(uint8_t addr, uint8_t *buf, uint8_t n);    // exactly n bytes, or BUS_SHORT / NACK
BusResult   busReadReg(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n);   // pointer write, repeated start, read
bool        busProbe(uint8_t addr);                            // an empty write the address ACKs
bool        busRecover();                                      // SDA held low: clock SCL free, re-init
const BusStats &busStats();
const char *busResultName(BusResult r);
