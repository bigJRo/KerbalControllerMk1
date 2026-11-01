// Host stand-in for Adafruit_ST7789, for tools/sysinfo_host.py: the pixels pushed land
// in hostPanel(), which the harness writes out as the screen.
#pragma once
#include "Arduino.h"
#include "SPI.h"

inline uint16_t *hostPanel() { static uint16_t p[240 * 320]; return p; }
inline uint32_t &hostPanelPushes() { static uint32_t n = 0; return n; }

class Adafruit_ST7789 {
 public:
  Adafruit_ST7789(SPIClass *, int8_t cs, int8_t dc, int8_t rst) : cs(cs), dc(dc), rst(rst) {}
  void init(uint16_t width, uint16_t height) { w = width; h = height; }
  void setRotation(uint8_t r) { rot = r; }
  void setSPISpeed(uint32_t hz) { spiHz = hz; }
  void fillScreen(uint16_t c) { for (int i = 0; i < 240 * 320; i++) hostPanel()[i] = c; }
  void startWrite() {}
  void endWrite() {}
  void setAddrWindow(uint16_t x, uint16_t y, uint16_t ww, uint16_t hh) { ax = x; ay = y; aw = ww; ah = hh; }
  void writePixels(uint16_t *c, uint32_t n, bool = true, bool = false) {
    if (ax == 0 && ay == 0 && aw == 240 && ah == 320 && n == 240u * 320u) memcpy(hostPanel(), c, n * 2);
    hostPanelPushes()++;
  }
  int8_t cs, dc, rst;
  uint16_t w = 0, h = 0;
  uint8_t rot = 0;
  uint32_t spiHz = 0;
 private:
  uint16_t ax = 0, ay = 0, aw = 0, ah = 0;
};
