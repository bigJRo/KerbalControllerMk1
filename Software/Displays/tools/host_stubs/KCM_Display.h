// Host stand-in for KCM_Display: KCM_TFT with the drawing surface the sketches use.
#pragma once
#include "Arduino.h"
#include <KCMk1_SystemConfig.h>
#include "gen_fonts.h"
struct KCM_TFT {
  KCM_TFT(int, int, int) {}
  void fillRect(int, int, int, int, uint16_t) {}
  void drawRect(int, int, int, int, uint16_t) {}
  void drawLine(int, int, int, int, uint16_t) {}
  void drawFastHLine(int, int, int, uint16_t) {}
  void drawFastVLine(int, int, int, uint16_t) {}
  void drawPixel(int, int, uint16_t) {}
  void fillTriangle(int, int, int, int, int, int, uint16_t) {}
  void drawTriangle(int, int, int, int, int, int, uint16_t) {}
  void fillCircle(int, int, int, uint16_t) {}
  void drawCircle(int, int, int, uint16_t) {}
  void fillScreen(uint16_t) {}
  void setFont(const ILI9341_t3_font_t &) {}
  void setTextColor(uint16_t, uint16_t) {}
  void setTextColor(uint16_t) {}
  void setCursor(int, int) {}
  template <class T> void print(const T &) {}
  template <class T> void println(const T &) {}
  void setRotation(int) {}
  void setActiveWindow() {}
  void setActiveWindow(int, int, int, int) {}
  void writeRect(int, int, int, int, const uint16_t *) {}
};
inline void kcmDisplayBegin(KCM_TFT &, uint16_t) {}
