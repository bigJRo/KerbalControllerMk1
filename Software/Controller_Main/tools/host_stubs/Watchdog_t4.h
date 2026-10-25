// Host stand-in for Watchdog_t4: the declarations Controller_Main makes.
#pragma once
#include <Arduino.h>
enum WDT_DEV_TABLE { WDT1, WDT2, WDT3 };
struct WDT_timings_t { uint32_t timeout = 0, trigger = 0, window = 0; void (*callback)() = nullptr; };
template <WDT_DEV_TABLE N> class WDT_T4 {
public:
  void begin(WDT_timings_t) {}
  void feed() {}
  void reset() {}
};
