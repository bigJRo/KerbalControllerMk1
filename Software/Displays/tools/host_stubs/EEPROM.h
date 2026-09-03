// Host stub for the Teensy EEPROM library, backed by a byte array so a harness can
// round-trip an image. stubEepromWrites() counts bytes actually changed, the way the
// Teensy core only programs a byte whose value differs.
#pragma once
#include <cstdint>
#include <cstring>
static const int STUB_EEPROM_SIZE = 4284;
inline uint8_t  *stubEeprom()       { static uint8_t m[STUB_EEPROM_SIZE]; return m; }
inline uint32_t &stubEepromWrites() { static uint32_t n = 0; return n; }
inline void stubEepromReset(uint8_t fill = 0xFF) { memset(stubEeprom(), fill, STUB_EEPROM_SIZE); stubEepromWrites() = 0; }
class EEPROMClass {
 public:
  uint8_t read(int a) { return (a >= 0 && a < STUB_EEPROM_SIZE) ? stubEeprom()[a] : 0xFF; }
  void    write(int a, uint8_t v) { if (a < 0 || a >= STUB_EEPROM_SIZE) return; if (stubEeprom()[a] != v) { stubEeprom()[a] = v; stubEepromWrites()++; } }
  void    update(int a, uint8_t v) { write(a, v); }
  uint16_t length() { return STUB_EEPROM_SIZE; }
  template <typename T> T &get(int a, T &t) { memcpy(&t, stubEeprom() + a, sizeof(T)); return t; }
  template <typename T> const T &put(int a, const T &t) { const uint8_t *p = (const uint8_t *)&t; for (size_t i = 0; i < sizeof(T); i++) write(a + (int)i, p[i]); return t; }
};
static EEPROMClass EEPROM;
