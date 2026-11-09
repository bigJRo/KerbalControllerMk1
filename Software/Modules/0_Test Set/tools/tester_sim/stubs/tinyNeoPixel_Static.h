// Host stand-in for megaTinyCore's tinyNeoPixel / tinyNeoPixel_Static. show()
// publishes the logical (r, g, b) values to the simulation's LED state.
#pragma once
#include <Arduino.h>
#define NEO_RGB 0x06
#define NEO_GRB 0x52
#define NEO_GRBW 0xD2
#define NEO_KHZ800 0x0000
typedef uint16_t neoPixelType;
void sim_neo_show(uint8_t pin, const uint8_t* rgb, uint16_t n);
class tinyNeoPixel {
public:
    tinyNeoPixel(uint16_t n, uint8_t p, neoPixelType t = NEO_GRB, uint8_t* px = nullptr) : _n(n), _pin(p) {
        (void)t; (void)px; memset(_rgb, 0, sizeof _rgb);
    }
    void begin() {}
    void show() { sim_neo_show(_pin, _rgb, _n); }
    void setPixelColor(uint16_t i, uint8_t r, uint8_t g, uint8_t b) {
        if (i < _n && i < 24) { _rgb[i * 3] = r; _rgb[i * 3 + 1] = g; _rgb[i * 3 + 2] = b; }
    }
    void setPixelColor(uint16_t i, uint32_t c) { setPixelColor(i, (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c); }
    void clear() { memset(_rgb, 0, sizeof _rgb); }
    void setBrightness(uint8_t) {}
    uint16_t numPixels() const { return _n; }
    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) { return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b; }
    uint32_t getPixelColor(uint16_t i) const {
        return i < _n ? Color(_rgb[i * 3], _rgb[i * 3 + 1], _rgb[i * 3 + 2]) : 0;
    }
private:
    uint16_t _n;
    uint8_t _pin;
    uint8_t _rgb[72];
};
typedef tinyNeoPixel tinyNeoPixel_Static;
