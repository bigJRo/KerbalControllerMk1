#pragma once
#include <Adafruit_GFX.h>
class Adafruit_ST7789 : public Adafruit_GFX {
public:
    Adafruit_ST7789(int8_t cs, int8_t dc, int8_t rst) : Adafruit_GFX(240, 320) { (void)cs; (void)dc; (void)rst; }
    void init(uint16_t w, uint16_t h) { _w = w; _h = h; }
    void invertDisplay(bool) {}
};
