// Host stand-in for KerbalDisplayCommon: every helper the KCMk1 display sketches call,
// with the real palette (gen_colors.h) and real font metrics (gen_fonts.h) generated
// from the library by tools/host_compile.py. Nothing here draws.
#pragma once
#include "Arduino.h"
#include <SD.h>
#include <KCM_Display.h>
#include "gen_colors.h"
#include "gen_fonts.h"
#define KDC_VERSION_MAJOR 3
#define KDC_VERSION_MINOR 7
#define KDC_VERSION_PATCH 2
#define NO_BORDER 0x0001
#ifndef KCM_SCREEN_W
#define KCM_SCREEN_W 1024
#endif
#ifndef KCM_SCREEN_H
#define KCM_SCREEN_H 600
#endif
struct ButtonLabel { const char *text; uint16_t fontColorOff, fontColorOn, backgroundColorOff, backgroundColorOn, borderColorOff, borderColorOn; };
extern const byte TEXT_BORDER;
struct DispCache { String param, value; uint16_t paramColor = 0xFFFF, valColor = 0xFFFF, valBack = 0, backColor = 0, borderColor = 1; uint16_t x0 = 0, y0 = 0, w = 0, h = 0; bool valid = false; };
struct PrintState { uint16_t prevWidth = 0; uint16_t prevBg = 1; uint16_t prevHeight = 0; };
inline void setupDisplay(KCM_TFT &, uint16_t) {}
inline int16_t getFontCharWidth(const ILI9341_t3_font_t *, char) { return 8; }
inline int16_t getFontStringWidth(const ILI9341_t3_font_t *, const char *s) { return 8 * (int16_t)strlen(s); }
inline void drawButton(KCM_TFT &, int16_t, int16_t, int16_t, int16_t, const ButtonLabel &, const ILI9341_t3_font_t *, bool) {}
inline void textLeft(KCM_TFT &, const ILI9341_t3_font_t *, uint16_t, uint16_t, uint16_t, uint16_t, const String &, uint16_t, uint16_t) {}
inline void textRight(KCM_TFT &, const ILI9341_t3_font_t *, uint16_t, uint16_t, uint16_t, uint16_t, const String &, uint16_t, uint16_t) {}
inline void textCenter(KCM_TFT &, const ILI9341_t3_font_t *, uint16_t, uint16_t, uint16_t, uint16_t, const String &, uint16_t, uint16_t) {}
inline void eraseCenteredValue(KCM_TFT &, const ILI9341_t3_font_t *, int16_t, int16_t, int16_t, int16_t, const char *, uint16_t) {}
inline void drawDiamondMarker(KCM_TFT &, int16_t, int16_t, int16_t, uint16_t) {}
inline void drawThickLine(KCM_TFT &, int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t, bool = true) {}
inline void drawRoundRectOutline(KCM_TFT &, int16_t, int16_t, int16_t, int16_t, int16_t, uint16_t) {}
inline String formatInt(uint16_t v) { return String((int)v); }
inline String formatFloat(float v, uint8_t d) { return String(v, d); }
inline String formatPerc(uint16_t v) { return String((int)v) + "%"; }
inline String formatUnits(uint16_t v, String u) { return String((int)v) + u; }
inline String formatFloatUnits(float v, uint8_t d, String u) { return String(v, d) + u; }
inline String formatSep(float v) { return String(v, 2); }
inline String formatSepI64(int64_t v) { return String((long)v); }
inline String formatTime(float v) { return String(v, 0); }
inline String formatTimeCompact(float v) { return String(v, 0); }
inline String formatAlt(float v) { return String(v, 0); }
inline String twString(uint8_t, bool) { return String("1x"); }
inline void thresholdColor(uint16_t v, uint16_t lo, uint16_t loC, uint16_t loB, uint16_t mid, uint16_t midC, uint16_t midB, uint16_t hiC, uint16_t hiB, uint16_t &f, uint16_t &b) { if (v < lo) { f = loC; b = loB; } else if (v < mid) { f = midC; b = midB; } else { f = hiC; b = hiB; } }
inline void thresholdColor(float v, float lo, uint16_t loC, uint16_t loB, float mid, uint16_t midC, uint16_t midB, uint16_t hiC, uint16_t hiB, uint16_t &f, uint16_t &b) { if (v < lo) { f = loC; b = loB; } else if (v < mid) { f = midC; b = midB; } else { f = hiC; b = hiB; } }
inline void printDisp(KCM_TFT &, const ILI9341_t3_font_t *, uint16_t, uint16_t, uint16_t, uint16_t, const String &, const String &, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, PrintState &) {}
inline void printDisp(KCM_TFT &, const ILI9341_t3_font_t *, uint16_t, uint16_t, uint16_t, uint16_t, const String &, const String &, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, DispCache &, PrintState &) {}
inline void printDispChrome(KCM_TFT &, const ILI9341_t3_font_t *, uint16_t, uint16_t, uint16_t, uint16_t, String, uint16_t, uint16_t, uint16_t) {}
inline void printValue(KCM_TFT &, const ILI9341_t3_font_t *, uint16_t, uint16_t, uint16_t, uint16_t, const String &, const String &, uint16_t, uint16_t, uint16_t, PrintState &) {}
inline void printName(KCM_TFT &, const ILI9341_t3_font_t *, uint16_t, uint16_t, uint16_t, uint16_t, const String &, uint16_t, uint16_t, uint16_t, byte = 30) {}
inline void printTitle(KCM_TFT &, const ILI9341_t3_font_t *, uint16_t, uint16_t, uint16_t, uint16_t, const String &, uint16_t, uint16_t, uint16_t) {}
inline void setKDCDebugMode(bool) {}
inline void drawVertBarGraph(KCM_TFT &, uint16_t, uint16_t, uint16_t, uint16_t, int32_t, int32_t, uint16_t, bool, int32_t = 1000) {}
inline void drawArcDisplay(KCM_TFT &, int16_t, int16_t, uint16_t, uint16_t, float, float, float, float, uint16_t) {}
inline void drawLabelledAxis(KCM_TFT &, uint16_t, uint16_t, uint16_t, uint16_t, const ILI9341_t3_font_t *, uint16_t, uint16_t) {}
inline void bsPrint(KCM_TFT &, const ILI9341_t3_font_t *, uint16_t, uint16_t, const char *, uint16_t) {}
inline uint16_t bsLine(KCM_TFT &, const ILI9341_t3_font_t *, uint16_t, uint16_t y, uint16_t h, const char *, uint16_t) { return y + h; }
inline uint16_t bsBig(KCM_TFT &, const ILI9341_t3_font_t *, uint16_t, uint16_t y, const char *, uint16_t) { return y + 40; }
inline uint16_t bsBlank(uint16_t y, uint16_t h) { return y + h; }
inline uint16_t bsWrap(KCM_TFT &, const ILI9341_t3_font_t *, uint16_t, uint16_t y, uint16_t h, const char *, uint16_t, uint16_t) { return y + h; }
inline void bsShuffle(uint8_t *, uint8_t) {}
inline void drawVerticalText(KCM_TFT &, uint16_t, uint16_t, uint16_t, uint16_t, const ILI9341_t3_font_t *, const char *, uint16_t, uint16_t) {}
inline void drawStandbySplash(KCM_TFT &) {}
inline bool setupSD() { return true; }
struct BMPResult { bool ok = true; };
inline BMPResult drawBMP(KCM_TFT &, const char *, uint16_t, uint16_t) { return BMPResult(); }
inline void executeReboot() {}
inline void disconnectUSB() {}
