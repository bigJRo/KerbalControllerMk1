// Host stand-in for arduino-pico's TwoWire (slave side), for tools/sysinfo_host.py.
// The harness plays the master: hostWrite() is a master write (the receive callback
// runs at once, as the IRQ would), hostRead() a master read (the request callback).
#pragma once
#include "Arduino.h"
#include <vector>

class TwoWire {
 public:
  void setSDA(int p) { sda = p; }
  void setSCL(int p) { scl = p; }
  void begin(uint8_t a) { addr = a; started = true; }
  void onReceive(void (*cb)(int)) { rx = cb; }
  void onRequest(void (*cb)()) { req = cb; }
  int available() { return (int)(in.size() - pos); }
  int read() { return pos < in.size() ? in[pos++] : -1; }
  size_t write(const uint8_t *b, size_t n) { out.insert(out.end(), b, b + n); return n; }
  size_t write(uint8_t b) { out.push_back(b); return 1; }

  // harness side
  void hostWrite(const std::vector<uint8_t> &b) { in = b; pos = 0; if (rx) rx((int)b.size()); }
  std::vector<uint8_t> hostRead() { out.clear(); if (req) req(); return out; }

  int sda = -1, scl = -1;
  uint8_t addr = 0;
  bool started = false;
 private:
  std::vector<uint8_t> in, out;
  size_t pos = 0;
  void (*rx)(int) = nullptr;
  void (*req)() = nullptr;
};
inline TwoWire Wire;
inline TwoWire Wire1;
