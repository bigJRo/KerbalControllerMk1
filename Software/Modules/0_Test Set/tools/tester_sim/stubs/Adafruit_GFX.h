// Host stand-in for Adafruit_GFX: a 240x320 RGB565 framebuffer with the
// classic 6x8 text cell. Text that is drawn is also recorded so the simulated
// operator can read the screen.
#pragma once
#include <Arduino.h>
class Adafruit_GFX {
public:
    Adafruit_GFX(int16_t w, int16_t h) : _w(w), _h(h) {}
    void fillScreen(uint16_t c) { fillRect(0, 0, _w, _h, c); }
    void drawPixel(int16_t x, int16_t y, uint16_t c);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) { fillRect(x, y, w, 1, c); }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) { fillRect(x, y, 1, h, c); }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c);
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c);
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c);
    void setTextSize(uint8_t s) { _ts = s ? s : 1; }
    void setTextColor(uint16_t fg) { _fg = fg; _bg = fg; }
    void setTextColor(uint16_t fg, uint16_t bg) { _fg = fg; _bg = bg; }
    void setCursor(int16_t x, int16_t y) { _cx = x; _cy = y; }
    void setTextWrap(bool w) { _wrap = w; }
    void setRotation(uint8_t) {}
    size_t print(const char* s);
    size_t print(char c) { char b[2] = {c, 0}; return print(b); }
    size_t print(int v) { char b[16]; snprintf(b, sizeof b, "%d", v); return print(b); }
    int16_t width() const { return _w; }
    int16_t height() const { return _h; }
protected:
    int16_t _w, _h;
    uint8_t _ts = 1;
    uint16_t _fg = 0xFFFF, _bg = 0xFFFF;
    int16_t _cx = 0, _cy = 0;
    bool _wrap = true;
};
