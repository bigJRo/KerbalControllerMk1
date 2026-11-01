// Host stand-in for arduino-pico's SD, for tools/sysinfo_host.py. The "card" is the
// directory named by KCM_SD_ROOT; unset (or missing) = no card.
#pragma once
#include "Arduino.h"
#include <string>
#define FILE_READ 0

class File {
 public:
  File() {}
  explicit File(FILE *f) : fp(f) {}
  explicit operator bool() const { return fp != nullptr; }
  int read(void *b, size_t n) { return fp ? (int)fread(b, 1, n, fp) : -1; }
  bool seek(uint32_t pos) { return fp && fseek(fp, (long)pos, SEEK_SET) == 0; }
  void close() { if (fp) fclose(fp); fp = nullptr; }
 private:
  FILE *fp = nullptr;
};

class SDClass {
 public:
  bool begin(int) {
    const char *r = getenv("KCM_SD_ROOT");
    if (!r || !*r) return false;
    root = r;
    return true;
  }
  File open(const char *path, int) { return File(fopen((root + path).c_str(), "rb")); }
 private:
  std::string root;
};
inline SDClass SD;
