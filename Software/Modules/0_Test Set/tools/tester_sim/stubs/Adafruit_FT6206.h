#pragma once
#include <Arduino.h>
struct TS_Point { int16_t x, y, z; };
bool sim_touch(int16_t& x, int16_t& y);
class Adafruit_FT6206 {
public:
    bool begin(uint8_t = 128) { return true; }
    bool touched() { int16_t x, y; return sim_touch(x, y); }
    TS_Point getPoint() { TS_Point p = {0, 0, 1}; sim_touch(p.x, p.y); return p; }
};
