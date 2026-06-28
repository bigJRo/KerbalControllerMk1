/***********************************************************************************
  KCMk1_SystemConfig.h — Master hardware and cross-panel configuration
  for Kerbal Controller Mk1 display panels.
  Include from each sketch's main .h file.
  Contains only #defines — no variables, no functions.
  Panel-specific constants remain in each sketch's AAA_Config.ino.

  ─────────────────────────────────────────────────────────────────────────────
  HARDWARE REVISION 2 — KC-01-1911/1912 "7\" TFT Display Driver"
  ─────────────────────────────────────────────────────────────────────────────
  The Annunciator, Info, and Resource panels now share a single carrier board
  built around a Teensy 4.1 driving a 7" 1024x600 IPS TFT (BuyDisplay
  ER-TFTM070-6) over the RA8876 controller's 16-bit 8080 parallel bus.

    Old stack (rev 1)            New stack (rev 2, this file)
    ----------------------       ------------------------------------------
    Teensy 4.0                   Teensy 4.1
    RA8875, SPI, 800x480         RA8876, 16-bit parallel 8080, 1024x600
    GSL1680F touch (Wire1)       FT5316 cap touch (software I2C, pins 4/5)
    tone() buzzer (pin 9)        tone() buzzer (TONE, pin 2) + DFPlayer Mini
    SD over SPI (CS 5)           Teensy 4.1 on-board SD (SDIO / BUILTIN_SDCARD)
    slave I2C on Wire (18/19)    slave I2C on Wire2 (24/25)
    INT-to-master pin 2          INT_BUS pin 0

  Pin assignments below are transcribed directly from schematic
  KC-01-1911 (Drawn: J. Rostoker, V2.0). See PORTING_7inch_TFT.md for the full
  net-by-net mapping and rationale.
***********************************************************************************/
#pragma once

// =============================================================================
// DISPLAY HARDWARE — RA8876 / 1024x600
// =============================================================================
#define KCM_SCREEN_W              1024
#define KCM_SCREEN_H              600

// --- RA8876 16-bit 8080 parallel data bus (DB0..DB15 -> Teensy 4.1 pins) ---
// These ARE the Teensy 4.1 FlexIO3 parallel pin set used by the
// wwatson4506/TeensyRA8876-8080 driver (D0=19, /WR=36, /RD=37, ...). The board
// was laid out around that driver, so the bus is driven by FlexIO3 hardware —
// these defines are documentation; the driver owns the data/WR/RD lines.
// Only /CS, RS, /RESET below are passed to the driver (they are plain GPIO and
// were remapped off the library defaults 11/13/12 to 34/33/35).
#define KCM_TFT_DB0    19
#define KCM_TFT_DB1    18
#define KCM_TFT_DB2    14
#define KCM_TFT_DB3    15
#define KCM_TFT_DB4    40
#define KCM_TFT_DB5    41
#define KCM_TFT_DB6    17
#define KCM_TFT_DB7    16
#define KCM_TFT_DB8    22
#define KCM_TFT_DB9    23
#define KCM_TFT_DB10   20
#define KCM_TFT_DB11   21
#define KCM_TFT_DB12   38
#define KCM_TFT_DB13   39
#define KCM_TFT_DB14   26
#define KCM_TFT_DB15   27

// --- RA8876 control lines ---
#define KCM_TFT_CS      34   // /CS   chip select        (active low)
#define KCM_TFT_RESET   35   // /RST  hardware reset      (active low)
#define KCM_TFT_WR      36   // /WR   write strobe        (active low)
#define KCM_TFT_RD      37   // /RD   read strobe         (active low)
#define KCM_TFT_RS      33   // RS    register/data select (0=cmd/status, 1=data)
#define KCM_TFT_WAIT    32   // WAIT  busy from RA8876     (optional flow control)
#define KCM_TFT_INT     31   // INT   interrupt from RA8876 (unused for now)
#define KCM_TFT_BL      9    // BL_CTRL backlight enable / PWM

// FlexIO 8080 bus speed (MHz). Must be one of the driver's allowed steps:
// 2,4,8,12,20,24,30,40,60,120. Start conservative and raise once stable.
#define KCM_TFT_BUS_SPEED_MHZ  20
#define KCM_TFT_BUS_WIDTH      16

// =============================================================================
// CAPACITIVE TOUCH — FT5316 (5-point) on a local software-I2C bus
// =============================================================================
// SCL_LOCAL / SDA_LOCAL land on pins 4/5, which are NOT a hardware I2C bus on
// the Teensy 4.1 (Wire=18/19, Wire1=16/17, Wire2=24/25 are all consumed by the
// data bus and module bus). The touch driver therefore bit-bangs I2C here.
#define KCM_CTP_SCL     4    // SCL_LOCAL (software I2C clock)
#define KCM_CTP_SDA     5    // SDA_LOCAL (software I2C data)
#define KCM_CTP_RST     3    // CTP_/RST  (active low)
#define KCM_CTP_INT     6    // CTP_INT   (data-ready, active per FT5316 mode)
#define KCM_CTP_I2C_ADDR 0x38  // FT5x06/FT5316 fixed 7-bit address

// =============================================================================
// AUDIO — master-alarm buzzer (tone) + DFPlayer Mini (sampled)
// =============================================================================
#define KCM_AUDIO_TONE_PIN  2    // TONE -> Q1/S8050 -> 4kHz buzzer (tone())
#define KCM_DFPLAYER_SERIAL Serial2  // Teensy 4.1 Serial2 = RX2(7)/TX2(8)
#define KCM_DFPLAYER_BAUD   9600

// =============================================================================
// MODULE / SLAVE I2C BUS — Wire2 (to the master controller)
// =============================================================================
#define KCM_I2C_BUS               Wire2  // SCL_BUS=24(SCL2), SDA_BUS=25(SDA2)
#define KCM_I2C_INT_PIN           0      // INT_BUS — assert to signal the master
#define KCM_I2C_RST_PIN           1      // RST — shared reset line from the master
#define KCM_I2C_ADDR_ANNUNCIATOR  0x10
#define KCM_I2C_ADDR_RESDISP      0x11
#define KCM_I2C_ADDR_INFODISP     0x12
#define KCM_I2C_SYNC_ANNUNCIATOR  0xAC
#define KCM_I2C_SYNC_RESDISP      0xAD
#define KCM_I2C_SYNC_INFODISP     0xAE   // was 0xAD — collision fix (item #3)

// =============================================================================
// SD CARD — Teensy 4.1 on-board socket (SDIO, not SPI)
// =============================================================================
// The carrier board exposes no SPI/SD lines to the TFT module, so panel BMP
// assets load from the Teensy 4.1's built-in microSD slot via SDIO.
//   SD.begin(KCM_SD_CS)  with  BUILTIN_SDCARD
#define KCM_SD_CS                 BUILTIN_SDCARD

// =============================================================================
// SERIAL
// =============================================================================
#define KCM_SERIAL_BAUD           115200

// =============================================================================
// TOUCH FILTER
// =============================================================================
#define KCM_TOUCH_DEBOUNCE_MS       500
#define KCM_TOUCH_DEAD_ZONE_PX       12
#define KCM_TOUCH_JITTER_MAX_PX      20
#define KCM_TOUCH_TITLE_DEBOUNCE_MS 200  // InfoDisp title-bar shorter window

// =============================================================================
// DEFAULT OPERATING MODES
// =============================================================================
#define KCM_DEFAULT_DEBUG_MODE       true
#define KCM_DEFAULT_DEMO_MODE        true
#define KCM_DEFAULT_DISPLAY_ROTATION 0

// =============================================================================
// CROSS-PANEL ALIGNED THRESHOLDS
// These values must stay identical between Annunciator C&W logic and InfoDisp
// display thresholds. Edit here only — local constants in each sketch alias these.
// =============================================================================
#define KCM_GROUND_PROX_S         10.0f   // CW_GROUND_PROX_S / LNDG_TGRND_ALARM_S
#define KCM_HIGH_G_ALARM_POS       9.0f   // CW_HIGH_G_ALARM   / G_ALARM_POS
#define KCM_HIGH_G_ALARM_NEG      -5.0f   // CW_HIGH_G_WARN    / G_ALARM_NEG
#define KCM_LOW_DV_MS            150.0f   // CW_LOW_DV_MS      / DV_STG_ALARM_MS
#define KCM_LOW_BURN_S            60.0f   // CW_LOW_BURN_S     / LNCH_BURNTIME_ALARM_S
