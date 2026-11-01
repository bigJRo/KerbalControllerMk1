/***************************************************************************************
   sysinfo.h — master side of the Sys Info Display link (0x14, A2 Panel Hub P2).
   Design §14; protocol in Documents/Developer/I2C_Protocol_Specification.md §15.4.

   Master → Sys Info Display: one frame per write, byte 0 = frame id.
     0xB0 CONTROL     [B0, ctrl]                          heartbeat, every 500 ms
                      ctrl: bit 0 debug, bit 1 demo, bit 2 flight scene (bits 7:3 0)
     0xB1 BOOT_LINE   [B1, seq, attr, len, text[len]]     len ≤ 40, ASCII
                      attr: bits 1:0 severity (0 info, 1 ok, 2 warning, 3 error);
                            bit 6 REPLAY — the first line of a replay: clear first;
                            bit 7 REPLACE — overwrite the previous line
                      text: the line's short form (boot.h bootLineS())
     0xB2 BOOT_DONE   [B2]                                leave the boot screen
     0xB3 STATUS      27 bytes, little-endian:
                      [B3, valid, vbus u16 cV, current i16 mA, power u16 dW,
                       temp i8 °C, fan u16 rpm, links u8, known u8,
                       fan duty %, module count, present u16, FAULT u16,
                       I2C errors u16, uptime u32 s, version major, minor]
                      valid: bit 0 power, bit 1 temperature, bit 2 fan
                      links / known: bit per board with its own KSP link (SI_LINK_*)
                      present / FAULT: bit per module in ModId order
     0xB4 SWITCH_ERR  [B4, n, {id, expected, actual} × n]  n ≤ 10; n = 0 closes the window
     0xB8 BACKLIGHT   [B8, flight %, standby %, 0]        as the 7" displays (carriers.cpp)

   Sys Info Display → master (4-byte read):
     [0xAF, flags, fwMajor, fwMinor]   flags: bit 0 ready, bit 1 SD card OK,
                                        bit 2 boot screen showing
   The Sys Info Display shows MASTER NOT RESPONDING if no frame arrives for 5 s while
   its boot screen is up; the CONTROL heartbeat keeps that from tripping.
****************************************************************************************/
#pragma once
#include <Arduino.h>

enum : uint8_t {
  SI_FRAME_CONTROL = 0xB0, SI_FRAME_BOOT_LINE = 0xB1, SI_FRAME_BOOT_DONE = 0xB2,
  SI_FRAME_STATUS = 0xB3, SI_FRAME_SWITCH_ERR = 0xB4
};
enum : uint8_t { SI_CTRL_DEBUG = 0x01, SI_CTRL_DEMO = 0x02, SI_CTRL_FLIGHT = 0x04 };
enum : uint8_t { SI_ATTR_REPLAY = 0x40, SI_ATTR_REPLACE = 0x80 };
enum : uint8_t {   // link bits
  SI_LINK_MASTER = 0x01, SI_LINK_ANNUNCIATOR = 0x02, SI_LINK_INFODISP_1 = 0x04,
  SI_LINK_INFODISP_2 = 0x08, SI_LINK_RESDISP = 0x10
};
enum BootSev : uint8_t { SEV_INFO = 0, SEV_OK = 1, SEV_WARN = 2, SEV_ERROR = 3 };

static const uint8_t SI_TEXT_MAX      = 40;
static const uint8_t SI_SWERR_MAX     = 10;
static const uint8_t SI_STATUS_LEN    = 27;   // 13 before master v0.10.0
static const uint8_t SI_READ_LEN      = 4;

struct SiStatus {
  bool powerValid, tempValid, fanValid;
  float busV, currentA, powerW;
  int8_t tempC;
  uint16_t fanRpm;
  uint8_t fanDuty;      // %
};

struct SiSwitchErr { uint8_t id, expected, actual; };

void siInit();
bool siProbeBoot(uint32_t timeoutMs);                     // boot step 2: wait for it to answer
void siBootLine(BootSev sev, bool replace, const char *text);
void siBootDone();
void siService();                                         // heartbeat, status, re-probe
void siSetStatus(const SiStatus &s);                      // board services (stage 8)
void siSetSwitchErrors(const SiSwitchErr *list, uint8_t n);   // interlocks (stage 3)
void siSendBacklight(const uint8_t *frame, uint8_t n);   // carriers.cpp's BACKLIGHT frame, to 0x14 too
bool siPresent();
