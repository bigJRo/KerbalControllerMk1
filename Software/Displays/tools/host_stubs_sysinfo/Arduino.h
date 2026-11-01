// Host stand-in for the arduino-pico core, for tools/sysinfo_host.py (KCMk1_SysInfo).
// Only what the sketch touches. The clock, the backlight PWM and the watchdog are
// visible to the test harness.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>

typedef unsigned char byte;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0

// ArduinoCore-API's min / max are templates in C++ (mixed types allowed).
template <class T, class L> auto min(const T &a, const L &b) -> decltype((b < a) ? b : a) { return (b < a) ? b : a; }
template <class T, class L> auto max(const T &a, const L &b) -> decltype((b < a) ? b : a) { return (a < b) ? b : a; }

inline uint32_t &stubMillis() { static uint32_t m = 0; return m; }
inline uint32_t millis() { return stubMillis(); }
inline void delay(uint32_t ms) { stubMillis() += ms; }
inline int &stubPinLevel(int p) { static int v[64]; return v[p & 63]; }
inline void pinMode(int, int) {}
inline void digitalWrite(int p, int v) { stubPinLevel(p) = v; }
inline int &stubPwm(int p) { static int v[64]; return v[p & 63]; }
inline void analogWrite(int p, int v) { stubPwm(p) = v; }
inline uint32_t &stubPwmFreq() { static uint32_t f = 0; return f; }
inline uint32_t &stubPwmRange() { static uint32_t r = 255; return r; }
inline void analogWriteFreq(uint32_t f) { stubPwmFreq() = f; }
inline void analogWriteRange(uint32_t r) { stubPwmRange() = r; }

struct StubRp2040 {
  uint32_t wdtMs = 0, feeds = 0;
  void wdt_begin(uint32_t ms) { wdtMs = ms; }
  void wdt_reset() { feeds++; }
};
inline StubRp2040 rp2040;

struct StubSerial {
  bool quiet = true;
  void begin(unsigned long) {}
  int printf(const char *fmt, ...) {
    if (quiet) return 0;
    va_list a; va_start(a, fmt); int n = vfprintf(stderr, fmt, a); va_end(a); return n;
  }
  void println(const char *s) { if (!quiet) fprintf(stderr, "%s\n", s); }
  void print(const char *s) { if (!quiet) fputs(s, stderr); }
};
inline StubSerial Serial;
