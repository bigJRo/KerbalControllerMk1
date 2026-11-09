// Host stand-in for Wire. One object serves both sides of the bus in the
// simulation: calls made while the tester runs act as the bus controller,
// calls made inside the module firmware act as the target.
#pragma once
#include <Arduino.h>
class TwoWire {
public:
    void begin();
    void begin(uint8_t addr);
    void begin(int addr) { begin((uint8_t)addr); }
    void setClock(uint32_t) {}
    void beginTransmission(uint8_t addr);
    void beginTransmission(int addr) { beginTransmission((uint8_t)addr); }
    size_t write(uint8_t b);
    size_t write(const uint8_t* p, size_t n) { for (size_t i = 0; i < n; i++) write(p[i]); return n; }
    uint8_t endTransmission(bool stop = true);
    uint8_t requestFrom(int addr, int n);
    uint8_t requestFrom(uint8_t addr, uint8_t n) { return requestFrom((int)addr, (int)n); }
    int available();
    int read();
    void onReceive(void (*fn)(int));
    void onRequest(void (*fn)());
};
extern TwoWire Wire;
