// Host-side stand-in for the Arduino/Teensy core, for tools/host_compile.py.
// Only what the KCMk1 display sketches and their libraries touch; nothing here runs.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <string>
#include <algorithm>

typedef unsigned char byte;
typedef bool boolean;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2
#define HEX 16
#define DEC 10
#define BIN 2
#define F(x) (x)
#define PROGMEM
#define PI 3.1415926535897932384626433832795
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)
#define sq(x) ((x) * (x))
#define bitRead(v, b) (((v) >> (b)) & 0x01)
#define bitSet(v, b) ((v) |= (1UL << (b)))
#define bitClear(v, b) ((v) &= ~(1UL << (b)))
#define bitWrite(v, b, x) ((x) ? bitSet(v, b) : bitClear(v, b))
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))
// Teensy 4.x specifics the boot screens touch: clock, and the DWT cycle counter the
// InfoDisp seeds its randomness from. Plain lvalues here.
#define F_CPU 600000000
extern volatile uint32_t ARM_DWT_CYCCNT, ARM_DWT_CTRL, ARM_DEMCR;
#define ARM_DWT_CTRL_CYCCNTENA 1
#define ARM_DEMCR_TRCENA 0x01000000
// Arduino's min/max/abs/constrain are macros, and mixed-type arguments are legal
// there; matching that is the point, so they are macros here too.
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define abs(x) ((x) > 0 ? (x) : -(x))
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

// Clock the host harnesses can drive: stubMillis() is the settable value millis() reads.
inline uint32_t &stubMillis() { static uint32_t m = 0; return m; }
inline uint32_t millis() { return stubMillis(); }
inline uint32_t micros() { return 0; }
inline void delay(uint32_t) {}
inline void delayMicroseconds(uint32_t) {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline void digitalWriteFast(int, int) {}
inline int  digitalRead(int) { return 0; }
inline int  digitalReadFast(int) { return 0; }
inline void analogWrite(int, int) {}
inline int  analogRead(int) { return 0; }
inline void noInterrupts() {}
inline void interrupts() {}
inline void yield() {}
inline long random(long) { return 0; }
inline long random(long, long) { return 0; }
inline void randomSeed(unsigned long) {}
inline long map(long x, long a, long b, long c, long d) { return (x - a) * (d - c) / (b - a) + c; }
inline char *dtostrf(double v, int w, int p, char *b) { sprintf(b, "%*.*f", w, p, v); return b; }
inline size_t strlcpy(char *d, const char *s, size_t n) { snprintf(d, n, "%s", s); return strlen(s); }
inline size_t strlcat(char *d, const char *s, size_t n) { size_t l = strlen(d); if (l < n) snprintf(d + l, n - l, "%s", s); return l + strlen(s); }
inline char *itoa(int v, char *b, int) { sprintf(b, "%d", v); return b; }
inline char *ultoa(unsigned long v, char *b, int) { sprintf(b, "%lu", v); return b; }

struct String {
  std::string s;
  String() {}
  String(const char *c) : s(c ? c : "") {}
  String(const std::string &x) : s(x) {}
  String(char c) : s(1, c) {}
  String(int v) : s(std::to_string(v)) {}
  String(unsigned v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}
  String(unsigned long v) : s(std::to_string(v)) {}
  String(long long v) : s(std::to_string(v)) {}
  String(float v, int d = 2) { char b[32]; snprintf(b, 32, "%.*f", d, (double)v); s = b; }
  String(double v, int d = 2) { char b[32]; snprintf(b, 32, "%.*f", d, v); s = b; }
  String operator+(const String &o) const { return String(s + o.s); }
  String operator+(const char *o) const { return String(s + o); }
  String operator+(char c) const { return String(s + c); }
  String &operator+=(const String &o) { s += o.s; return *this; }
  String &operator+=(const char *o) { s += o; return *this; }
  String &operator+=(char c) { s += c; return *this; }
  bool operator==(const String &o) const { return s == o.s; }
  bool operator!=(const String &o) const { return s != o.s; }
  bool operator==(const char *o) const { return s == o; }
  bool operator!=(const char *o) const { return s != o; }
  size_t length() const { return s.size(); }
  const char *c_str() const { return s.c_str(); }
  char charAt(size_t i) const { return s[i]; }
  char operator[](size_t i) const { return s[i]; }
  String substring(size_t a) const { return String(s.substr(a)); }
  String substring(size_t a, size_t b) const { return String(s.substr(a, b - a)); }
  int indexOf(char c) const { auto p = s.find(c); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(const char *c) const { auto p = s.find(c); return p == std::string::npos ? -1 : (int)p; }
  bool startsWith(const char *c) const { return s.rfind(c, 0) == 0; }
  void toCharArray(char *b, size_t n) const { snprintf(b, n, "%s", s.c_str()); }
  void toUpperCase() {}
  void trim() {}
  float toFloat() const { return atof(s.c_str()); }
  int toInt() const { return atoi(s.c_str()); }
  void reserve(size_t) {}
};
inline String operator+(const char *a, const String &b) { return String(std::string(a) + b.s); }

// Serial ports. HardwareSerial is the type KCM_DFPlayer takes by reference.
struct Stream {
  int available() { return 0; }
  int read() { return -1; }
  int peek() { return -1; }
  void flush() {}
  size_t write(uint8_t) { return 1; }
  size_t write(const uint8_t *, size_t n) { return n; }
  template <class T> size_t print(const T &) { return 0; }
  template <class T> size_t print(const T &, int) { return 0; }
  template <class T> size_t println(const T &) { return 0; }
  template <class T> size_t println(const T &, int) { return 0; }
  size_t println() { return 0; }
  size_t printf(const char *, ...) { return 0; }
};
struct HardwareSerial : Stream {
  void begin(long) {}
  void begin(long, int) {}
  void end() {}
  operator bool() const { return true; }
  void setTimeout(long) {}
};
typedef HardwareSerial usb_serial_class;
extern HardwareSerial Serial, SerialUSB1, Serial1, Serial2, Serial3;

struct TwoWire {
  void begin() {}
  void begin(uint8_t) {}
  void setClock(uint32_t) {}
  void setSDA(int) {}
  void setSCL(int) {}
  void onRequest(void (*)()) {}
  void onReceive(void (*)(int)) {}
  int  available() { return 0; }
  int  read() { return 0; }
  size_t write(uint8_t) { return 1; }
  size_t write(const uint8_t *, size_t n) { return n; }
  void beginTransmission(uint8_t) {}
  uint8_t endTransmission(bool = true) { return 0; }
  uint8_t requestFrom(uint8_t, uint8_t, bool = true) { return 0; }
};
extern TwoWire Wire, Wire1, Wire2;
