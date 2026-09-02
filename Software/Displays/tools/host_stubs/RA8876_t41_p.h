// Host stand-in for wwatson4506/TeensyRA8876-8080's RA8876_t41_p: the drawing and
// page-control surface the KCMk1 panels and KCM_Display use. Nothing draws.
#pragma once
#include <Arduino.h>
#include "kcm_ili9341_font.h"
class RA8876_t41_p {
 public:
  uint32_t currentPage = 0;   // page base the text blit path draws relative to
  RA8876_t41_p(int, int, int) {}
  void setBusWidth(int) {}
  void begin(int = 20) {}
  void setRotation(int) {}
  void fillScreen(uint16_t) {}
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
  void fillRoundRect(int, int, int, int, int, uint16_t) {}
  void drawRoundRect(int, int, int, int, int, uint16_t) {}
  void setFont(const ILI9341_t3_font_t &) {}
  void setTextColor(uint16_t, uint16_t) {}
  void setTextColor(uint16_t) {}
  void setCursor(int, int) {}
  template <class T> void print(const T &) {}
  template <class T> void println(const T &) {}
  void writeRect(int, int, int, int, const uint16_t *) {}
  void activeWindowXY(int, int) {}
  void activeWindowWH(int, int) {}
  int  canvasImageWidth() { return 0; }
  void canvasImageWidth(int) {}
  void canvasImageWidth(int, int) {}
  void canvasImageStartAddress(uint32_t) {}
  void displayImageStartAddress(uint32_t) {}
  void displayImageWidth(int) {}
  void displayWindowStartXY(int, int) {}
  void check2dBusy() {}
  void checkWriteFifoEmpty() {}
  void bteMemoryCopy(uint32_t, int, int, int, uint32_t, int, int, int, int, int) {}
};
