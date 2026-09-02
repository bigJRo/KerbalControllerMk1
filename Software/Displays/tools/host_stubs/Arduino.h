// Host-side stand-in for the Arduino/Teensy core, for tools/host_compile.py.
// Only what the KCMk1 display sketches touch; nothing here runs.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <string>

typedef unsigned char byte;
typedef bool boolean;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2
#define HEX 16
#define DEC 10
#define F(x) (x)
#define PROGMEM
#define __IMXRT1062__ 1

inline uint32_t millis() { return 0; }
inline uint32_t micros() { return 0; }
inline void delay(uint32_t) {}
inline void delayMicroseconds(uint32_t) {}
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline void digitalWriteFast(int, int) {}
inline int  digitalRead(int) { return 0; }
inline void analogWrite(int, int) {}
inline void noInterrupts() {}
inline void interrupts() {}
template <class T> T constrain(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }
template <class T> T sq(T x) { return x * x; }
inline long map(long x, long a, long b, long c, long d) { return (x - a) * (d - c) / (b - a) + c; }
inline char *dtostrf(double v, int w, int p, char *b) { sprintf(b, "%*.*f", w, p, v); return b; }
inline size_t strlcpy(char *d, const char *s, size_t n) { snprintf(d, n, "%s", s); return strlen(s); }
inline size_t strlcat(char *d, const char *s, size_t n) { size_t l = strlen(d); if (l < n) snprintf(d + l, n - l, "%s", s); return l + strlen(s); }

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
  String(float v, int d = 2) { char b[32]; snprintf(b, 32, "%.*f", d, (double)v); s = b; }
  String(double v, int d = 2) { char b[32]; snprintf(b, 32, "%.*f", d, v); s = b; }
  String operator+(const String &o) const { return String(s + o.s); }
  String operator+(const char *o) const { return String(s + o); }
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
  void toCharArray(char *b, size_t n) const { snprintf(b, n, "%s", s.c_str()); }
  float toFloat() const { return atof(s.c_str()); }
  int toInt() const { return atoi(s.c_str()); }
};
inline String operator+(const char *a, const String &b) { return String(std::string(a) + b.s); }

struct HostSerial {
  void begin(long) {}
  void end() {}
  void flush() {}
  operator bool() const { return true; }
  int available() { return 0; }
  int read() { return -1; }
  template <class T> void print(const T &) {}
  template <class T> void print(const T &, int) {}
  template <class T> void println(const T &) {}
  template <class T> void println(const T &, int) {}
  void println() {}
  void write(uint8_t) {}
};
extern HostSerial Serial, SerialUSB1, Serial2;

struct HostWire {
  void begin() {}
  void begin(uint8_t) {}
  void setClock(uint32_t) {}
  void onRequest(void (*)()) {}
  void onReceive(void (*)(int)) {}
  int  available() { return 0; }
  int  read() { return 0; }
  void write(uint8_t) {}
  void write(const uint8_t *, size_t) {}
  void beginTransmission(uint8_t) {}
  uint8_t endTransmission() { return 0; }
  uint8_t requestFrom(uint8_t, uint8_t) { return 0; }
};
extern HostWire Wire, Wire1, Wire2;
