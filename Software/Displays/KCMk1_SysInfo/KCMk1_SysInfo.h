#pragma once
/***************************************************************************************
   KCMk1_SysInfo.h -- shared declarations for the Kerbal Controller Mk1 Sys Info Display
   (I2C 0x14, Panel A2 slot P2). Included by every .ino tab.

   Board: 2" TFT Display Driver KC-01-1901 / 1902 V2.0 -- Seeed XIAO RP2350, ER-TFTM020-2
   240x320 IPS (ST7789) in portrait, microSD on the same SPI bus. Core: arduino-pico.
   Design: Documents/Developer/Sys_Info_Display_Design.md. Protocol: I2C spec §15.4.

   This sketch does NOT use KerbalDisplayCommon (Teensy / RA8876 only). Its two fonts are
   byte-identical copies of the shared ones in src/fonts/ (tools/sysinfo_host.py checks).
****************************************************************************************/
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_ST7789.h>
#include <KCMk1_SystemConfig.h>     // header-only: backlight levels, addresses, sync bytes
#include "src/fonts/kcm_ili9341_font.h"

extern "C" {
  extern const ILI9341_t3_font_t Roboto_Black_32;
  extern const ILI9341_t3_font_t Roboto_Black_48;
}

/***************************************************************************************
   SKETCH VERSION (MAJOR.MINOR.PATCH), reported in the status read
****************************************************************************************/
#define SYSINFO_VERSION "1.0.0"
static const uint8_t SYSINFO_VERSION_MAJOR = 1;
static const uint8_t SYSINFO_VERSION_MINOR = 0;

/***************************************************************************************
   SCREEN
****************************************************************************************/
static const int16_t SI_W = 240, SI_H = 320;   // portrait (design D5)
static const int16_t SI_MARGIN = 2;            // room for the +/-2 px anti-sticking shift
static const uint8_t SI_MIN_CAP_PX = 22;       // 2.8 mm caps: 16 arcmin at 600 mm (D4, D6)
static const uint32_t RENDER_MS = 100;         // redraw at most 10 times a second

// RGB565, the same values as KerbalDisplayCommon's TFT_* names (given in brackets).
static const uint16_t C_BLACK  = 0x0000;   // TFT_BLACK
static const uint16_t C_DGREY  = 0x39E7;   // TFT_DARK_GREY
static const uint16_t C_GREY   = 0x8410;   // TFT_GREY
static const uint16_t C_LGREY  = 0xBDF7;   // TFT_LIGHT_GREY
static const uint16_t C_WHITE  = 0xFFFF;   // TFT_WHITE
static const uint16_t C_GREEN  = 0x07E0;   // TFT_GREEN
static const uint16_t C_RED    = 0xF800;   // TFT_RED
static const uint16_t C_YELLOW = 0xFDC2;   // TFT_YELLOW
static const uint16_t C_ORANGE = 0xFBE0;   // TFT_ORANGE
static const uint16_t C_SKY    = 0x761F;   // TFT_SKY
static const uint16_t C_NAVY   = 0x000F;   // TFT_NAVY
static const uint16_t C_BLUE   = 0x001F;   // TFT_BLUE: the Annunciator's lit DEMO tile (D11)

enum Align : uint8_t { AL_LEFT, AL_RIGHT, AL_CENTRE };
typedef const char *const Icon[16];   // 16 x 16 text art, '#' lit (AAA_Config.ino)
struct Glyph { const uint8_t *d; uint16_t w, h, delta; uint32_t bmp; };   // a decoded font glyph (Gfx.ino)

/***************************************************************************************
   PROTOCOL (I2C spec §15.4)
****************************************************************************************/
enum : uint8_t {
  SI_FRAME_CONTROL = 0xB0, SI_FRAME_BOOT_LINE = 0xB1, SI_FRAME_BOOT_DONE = 0xB2,
  SI_FRAME_STATUS = 0xB3, SI_FRAME_SWITCH_ERR = 0xB4, SI_FRAME_BACKLIGHT = 0xB8,
  SI_SYNC = 0xAF
};
enum : uint8_t { CTRL_DEBUG = 0x01, CTRL_DEMO = 0x02, CTRL_FLIGHT = 0x04 };
enum : uint8_t { ATTR_SEV = 0x03, ATTR_REPLAY = 0x40, ATTR_REPLACE = 0x80 };
enum : uint8_t { VALID_POWER = 0x01, VALID_TEMP = 0x02, VALID_FAN = 0x04 };
enum BootSev : uint8_t { SEV_INFO = 0, SEV_OK = 1, SEV_WARN = 2, SEV_ERROR = 3 };

static const uint8_t SI_TEXT_MAX      = 40;
static const uint8_t SI_SWERR_MAX     = 10;
static const uint8_t SI_STATUS_LEN    = 13;   // the original STATUS frame
static const uint8_t SI_STATUS_EXT    = 27;   // with fan duty, modules, I2C errors, uptime, version
static const uint8_t SI_FRAME_MAX     = 48;
static const uint8_t SI_LINK_COUNT    = 5;    // master, Annunciator, Info 1, Info 2, Resource
static const uint8_t SI_MOD_MAX       = 16;

/***************************************************************************************
   MODEL -- what the master has told this display
****************************************************************************************/
struct BootModel {
  char     last[SI_TEXT_MAX + 1];   // the newest line
  uint8_t  lastSev;
  char     prob[SI_TEXT_MAX + 1];   // the newest warning or error line
  uint8_t  probSev;
  bool     hasProb;
  uint16_t count[4];                // lines on show, by severity
  uint16_t lines;
};

struct StatusModel {
  bool     have, ext;
  uint8_t  valid;
  uint16_t cV;          // bus voltage, 0.01 V
  int16_t  mA;          // current
  uint16_t dW;          // power, 0.1 W
  int8_t   tempC;
  uint16_t rpm;
  uint8_t  links, known;
  // the extended frame (27 bytes)
  uint8_t  fanDuty, modCount;
  uint16_t present, fault, i2cErr;
  uint32_t uptimeS;
  uint8_t  fwMajor, fwMinor;
};

struct SwErr { uint8_t id, expected, actual; };

struct Model {
  bool        bootDone;
  bool        firstLine;            // a BOOT_LINE has arrived (ends the splash)
  uint8_t     ctrl;
  BootModel   boot;
  StatusModel st;
  SwErr       sw[SI_SWERR_MAX];
  uint8_t     swN;
  bool        heard;                // any frame since power-up
  uint32_t    lastFrameMs;
  uint32_t    i2cRiseMs;            // when the master's I2C error count last went up
  uint32_t    frames, dropped, bad; // diagnostics
};
extern Model g_m;

/***************************************************************************************
   STATUS MESSAGES (design §4.3)
****************************************************************************************/
enum MsgRank : uint8_t { MR_RED, MR_ORANGE, MR_YELLOW, MR_MODE, MR_NOMINAL, MR_SERVICE };
struct Msg {
  char     l1[16], l2[16];
  uint16_t fg, bg;         // bg C_BLACK = no block
  uint8_t  rank;
};
static const uint8_t SI_MSG_MAX = 40;
