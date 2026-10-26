// Host stand-in for the Arduino / Teensy 4.1 core, for the KCMk1_Master host build and
// selftest (tools/host_build.py). Unlike the displays' stub, this one RUNS: time, pins,
// Serial and the I2C bus are simulated, and a test can attach simulated devices to the
// bus and read back everything the firmware sent.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <strings.h>
#include <string>
#include <vector>

#define KCM_HOST 1

typedef unsigned char byte;
typedef bool boolean;
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3
#define F(x) (x)
#define PROGMEM
#define DMAMEM
#define bitRead(v, b) (((v) >> (b)) & 0x01)
#define bitSet(v, b) ((v) |= (1UL << (b)))
#define bitClear(v, b) ((v) &= ~(1UL << (b)))
#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define constrain(x, l, h) ((x) < (l) ? (l) : ((x) > (h) ? (h) : (x)))

/*------------------------------------------------------------------------------------
  Time. delay() advances the clock, so a firmware wait loop finishes in a test.
------------------------------------------------------------------------------------*/
inline uint32_t &stubMillis() { static uint32_t m = 0; return m; }
inline uint32_t &stubMicroExtra() { static uint32_t u = 0; return u; }
inline uint32_t millis() { return stubMillis(); }
inline uint32_t micros() { return stubMillis() * 1000u + stubMicroExtra(); }
inline void delay(uint32_t ms) { stubMillis() += ms; }
inline void delayMicroseconds(uint32_t us) { stubMicroExtra() += us; }
inline void yield() {}

/*------------------------------------------------------------------------------------
  Pins. stubPin(p) is the level digitalRead sees; writes are recorded per pin.
------------------------------------------------------------------------------------*/
struct StubPins { int level[64]; int mode[64]; std::vector<std::pair<int, int>> writes; };
inline StubPins &stubPins() { static StubPins p; return p; }
inline int &stubPin(int p) { return stubPins().level[p & 63]; }
inline void pinMode(int p, int m) { stubPins().mode[p & 63] = m; }
inline void digitalWrite(int p, int v) { stubPins().level[p & 63] = v; stubPins().writes.push_back({p, v}); }
inline void digitalWriteFast(int p, int v) { digitalWrite(p, v); }
inline int  digitalRead(int p) { return stubPins().level[p & 63]; }
inline int  digitalReadFast(int p) { return stubPins().level[p & 63]; }
inline int  analogRead(int) { return 0; }
inline void analogWrite(int, int) {}
inline void noInterrupts() {}
inline void interrupts() {}

/*------------------------------------------------------------------------------------
  Teensy 4.1 specifics the board code touches.
------------------------------------------------------------------------------------*/
inline uint32_t &stubSrsr() { static uint32_t v = 1; return v; }   // power-on by default
// SRC_SRSR is write-1-to-clear on the chip; the stub keeps the last value written for
// inspection and lets the test set the next boot's value directly.
#define SRC_SRSR (stubSrsr())
inline uint32_t *stubRetain() { static uint32_t r[4]; return r; }  // SNVS_LPGPR0..3
inline uint32_t &stubRtc() { static uint32_t s = 1000; return s; }
inline unsigned long rtc_get() { return stubRtc(); }

/*------------------------------------------------------------------------------------
  String (only what the KerbalSimpit header needs).
------------------------------------------------------------------------------------*/
struct String {
  std::string s;
  String() {}
  String(const char *c) : s(c ? c : "") {}
  String(int v) : s(std::to_string(v)) {}
  String(unsigned v) : s(std::to_string(v)) {}
  String(long v) : s(std::to_string(v)) {}
  String(unsigned long v) : s(std::to_string(v)) {}
  String operator+(const String &o) const { String r; r.s = s + o.s; return r; }
  const char *c_str() const { return s.c_str(); }
  size_t length() const { return s.size(); }
};
inline String operator+(const char *a, const String &b) { String r; r.s = std::string(a) + b.s; return r; }

/*------------------------------------------------------------------------------------
  Serial. Output is captured in `out`; a test queues console input with stubType().
------------------------------------------------------------------------------------*/
struct Stream {
  std::string out, in;
  size_t inPos = 0;
  int writeRoom = 4096;                        // what availableForWrite() reports
  virtual ~Stream() {}
  virtual int available() { return (int)(in.size() - inPos); }
  virtual int read() { return inPos < in.size() ? (uint8_t)in[inPos++] : -1; }
  virtual int peek() { return inPos < in.size() ? (uint8_t)in[inPos] : -1; }
  virtual size_t write(uint8_t c) { out.push_back((char)c); return 1; }
  virtual size_t write(const uint8_t *b, size_t n) { out.append((const char *)b, n); return n; }
  int availableForWrite() { return writeRoom; }
  void flush() {}
  size_t print(const char *s) { return write((const uint8_t *)s, strlen(s)); }
  size_t println(const char *s) { size_t n = print(s); return n + write((const uint8_t *)"\r\n", 2); }
  void stubType(const char *s) { in += s; }
};
struct HardwareSerial : Stream {
  void begin(long) {}
  void end() {}
  operator bool() const { return true; }
};
typedef HardwareSerial usb_serial_class;
extern HardwareSerial Serial, SerialUSB1;

/*------------------------------------------------------------------------------------
  I2C. A test attaches a SimDevice per address; an address with none NACKs.
  Every transaction is logged (address, direction, bytes).
------------------------------------------------------------------------------------*/
struct SimDevice {
  virtual ~SimDevice() {}
  virtual bool onWrite(const uint8_t *b, int n, bool stop) = 0;   // false = NACK the data
  virtual int  onRead(uint8_t *b, int n) = 0;                      // bytes supplied
};
struct WireRec { uint8_t addr; bool read; std::vector<uint8_t> b; uint32_t ms; };

class TwoWire {
 public:
  SimDevice *dev[128] = {};
  std::vector<WireRec> log;
  uint8_t txAddr = 0, tx[256]; int txN = 0;
  uint8_t rx[256]; int rxN = 0, rxI = 0;
  bool begun = false; uint32_t clock = 0;
  void attach(uint8_t a, SimDevice *d) { dev[a & 127] = d; }
  void detach(uint8_t a) { dev[a & 127] = nullptr; }
  void begin() { begun = true; }
  void setClock(uint32_t c) { clock = c; }
  void beginTransmission(uint8_t a) { txAddr = a; txN = 0; }
  size_t write(uint8_t v) { if (txN < 256) tx[txN++] = v; return 1; }
  size_t write(const uint8_t *b, size_t n) { for (size_t i = 0; i < n; i++) write(b[i]); return n; }
  uint8_t endTransmission(bool stop = true) {
    log.push_back({txAddr, false, std::vector<uint8_t>(tx, tx + txN), millis()});
    SimDevice *d = dev[txAddr & 127];
    if (!d) return 2;
    return d->onWrite(tx, txN, stop) ? 0 : 3;
  }
  uint8_t requestFrom(uint8_t a, uint8_t n, bool = true) {
    rxN = rxI = 0;
    SimDevice *d = dev[a & 127];
    if (d) rxN = d->onRead(rx, n);
    log.push_back({a, true, std::vector<uint8_t>(rx, rx + rxN), millis()});
    return (uint8_t)rxN;
  }
  int available() { return rxN - rxI; }
  int read() { return rxI < rxN ? rx[rxI++] : -1; }
};
extern TwoWire Wire, Wire1;
