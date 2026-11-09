// Host stand-in for InfectedBytes/ArduinoShiftIn: read() returns the raw
// 74HC165 chain value the simulation builds from the pressed KBC inputs.
#pragma once
#include <Arduino.h>
uint32_t sim_shiftin_raw(uint8_t chips);
template <uint8_t CHIPS>
class ShiftIn {
public:
    void begin(int, int, int, int) {}
    void setPulseWidth(uint8_t) {}
    uint32_t read() { _last = sim_shiftin_raw(CHIPS); return _last; }
    uint32_t getCurrent() const { return _last; }
    bool update() { uint32_t o = _last; read(); return o != _last; }
private:
    uint32_t _last = 0;
};
