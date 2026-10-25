// File-backed stand-in for the Teensy SD library, for tools/render_screen.py: the
// card "root" is a host directory (KCM_SD_ROOT, default ../assets), so drawBMP()
// reads the real panel art.
#pragma once
#include "Arduino.h"
#include <cstdio>
#include <string>
#define BUILTIN_SDCARD 254
#define FILE_READ 0
inline std::string sdRoot() { const char *r = getenv("KCM_SD_ROOT"); return r ? r : "assets"; }
class File {
 public:
  FILE *f = nullptr;
  File() {}
  explicit File(FILE *fp) : f(fp) {}
  operator bool() const { return f != nullptr; }
  int read() { if (!f) return -1; int c = fgetc(f); return c == EOF ? -1 : c; }
  size_t read(void *buf, size_t n) { return f ? fread(buf, 1, n, f) : 0; }
  bool seek(uint32_t pos) { return f && fseek(f, (long)pos, SEEK_SET) == 0; }
  uint32_t position() { return f ? (uint32_t)ftell(f) : 0; }
  uint32_t size() { if (!f) return 0; long p = ftell(f); fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, p, SEEK_SET); return (uint32_t)s; }
  int available() { return f ? (int)(size() - position()) : 0; }
  void close() { if (f) fclose(f); f = nullptr; }
};
class SDClass {
 public:
  bool begin(int = 0) { return true; }
  bool exists(const char *p) { FILE *f = fopen((sdRoot() + p).c_str(), "rb"); if (f) fclose(f); return f != nullptr; }
  File open(const char *p, int = FILE_READ) { return File(fopen((sdRoot() + p).c_str(), "rb")); }
};
static SDClass SD;
