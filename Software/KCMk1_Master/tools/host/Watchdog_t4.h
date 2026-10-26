// Host stand-in for WDT_T4 (tonton81, shipped with Teensyduino): same WDT_timings_t
// fields and types as the library. Records the configuration and feeds so a
// selftest can check them, and can fire the warning callback itself.
#pragma once
#include <Arduino.h>
enum WDT_DEV_TABLE { WDT1, WDT2, WDT3 };
struct WDT_timings_t { double trigger = 5; double timeout = 10; double window = 0; uint8_t pin = 0;
                       void (*callback)() = nullptr; };
struct StubWdt { bool started = false; double timeout = 0, trigger = 0; void (*callback)() = nullptr;
                 uint32_t feeds = 0; };
inline StubWdt &stubWdt() { static StubWdt w; return w; }
template <WDT_DEV_TABLE N> class WDT_T4 {
 public:
  void begin(WDT_timings_t c) { StubWdt &w = stubWdt(); w.started = true; w.timeout = c.timeout;
                                w.trigger = c.trigger; w.callback = c.callback; }
  void feed() { stubWdt().feeds++; }
  void reset() {}
};
