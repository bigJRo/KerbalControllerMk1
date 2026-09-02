#pragma once
#include "Arduino.h"
#ifndef CTP_MAX_TOUCHES
#define CTP_MAX_TOUCHES 5
#endif
struct TouchPoint { uint16_t x = 0; uint16_t y = 0; uint8_t id = 0; };
struct TouchResult { uint8_t count = 0; TouchPoint points[CTP_MAX_TOUCHES]; };
inline void setupTouch() {}
inline bool probeTouch() { return true; }
inline bool isTouched() { return false; }
inline TouchResult readTouch() { return TouchResult(); }
inline void clearTouchISR() {}
inline uint32_t touchISRCount() { return 0; }
inline void setTouchDebug(bool) {}
