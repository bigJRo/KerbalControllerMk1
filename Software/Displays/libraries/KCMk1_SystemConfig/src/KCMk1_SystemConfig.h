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
  ER-TFT070A2-6-5633). The physical display controller is the LT7683, which is
  register-compatible with the RA8876; the firmware drives it over the RA8876's
  16-bit 8080 parallel bus via the wwatson4506/TeensyRA8876-8080 driver.

    Old stack (rev 1)            New stack (rev 2, this file)
    ----------------------       ------------------------------------------
    Teensy 4.0                   Teensy 4.1
    RA8875, SPI, 800x480         LT7683 (RA8876-compat), 16-bit 8080, 1024x600
    GSL1680F touch (Wire1)       FT5316 cap touch (software I2C, pins 4/5)
    tone() buzzer (pin 9)        tone() -> PAM8302A amp + speaker (TONE 2) + DFPlayer
    SD over SPI (CS 5)           Teensy 4.1 on-board SD (SDIO / BUILTIN_SDCARD)
    slave I2C on Wire (18/19)    slave I2C on Wire2 (24/25)
    INT-to-master pin 2          INT_BUS pin 0

  Pin assignments below are transcribed directly from schematic
  KC-01-1911 (Drawn: J. Rostoker, V2.0). See PORTING_7inch_TFT.md for the full
  net-by-net mapping and rationale.
***********************************************************************************/
#pragma once

// =============================================================================
// DISPLAY HARDWARE — LT7683 (RA8876-compatible) / 1024x600
// =============================================================================
#define KCM_SCREEN_W              1024
#define KCM_SCREEN_H              600

// --- LT7683 16-bit 8080 parallel data bus (DB0..DB15 -> Teensy 4.1 pins) ---
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

// --- LT7683 control lines ---
#define KCM_TFT_CS      34   // /CS   chip select        (active low)
#define KCM_TFT_RESET   35   // /RST  hardware reset      (active low)
#define KCM_TFT_WR      36   // /WR   write strobe        (active low)
#define KCM_TFT_RD      37   // /RD   read strobe         (active low)
#define KCM_TFT_RS      33   // RS    register/data select (0=cmd/status, 1=data)
#define KCM_TFT_WAIT    32   // WAIT  busy from LT7683     (optional flow control)
#define KCM_TFT_INT     31   // INT   interrupt from LT7683 (unused for now)
#define KCM_TFT_BL      9    // BL_CTRL backlight enable / PWM

// Backlight brightness as a PWM duty cycle (0-100%). Driven via analogWrite() on
// KCM_TFT_BL. Lowered from full-on to save power / reduce heat.
#define KCM_BL_BRIGHTNESS_PCT  70

// One display frame period in microseconds, used by the double-buffer layer to let
// a page flip take effect (the LT7683 latches the scan-out base at the frame
// boundary) before the just-freed page is drawn again. Panel runs ~58Hz
// (PCLK/(Htotal*Vtotal) ~= 50e6/(1348*635)); 20000us (>=50Hz) gives safe margin.
// Raise if any flicker remains; lower toward ~17000 to allow a higher flip rate.
#define KCM_FRAME_PERIOD_US  20000

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
// AUDIO — master-alarm tone -> PAM8302A amp + speaker + DFPlayer Mini (sampled)
// =============================================================================
// KC-01-1911 V2.1: the earlier S8050 buzzer stage was replaced by a PAM8302A
// Class-D amplifier driving an external 8 ohm speaker (with an input volume trim).
// TONE (pin 2) feeds the amp input via tone(); TONE_EN drives the amp's active-low
// shutdown (/SD) so firmware can power the amp down between cues and mute Class-D
// idle hiss. The board pulls /SD low, so the amp is OFF until firmware enables it.
#define KCM_AUDIO_TONE_PIN  2    // TONE -> PAM8302A amp input -> external speaker (tone())
// KCM_AUDIO_EN_PIN — Teensy GPIO on the TONE_EN net -> PAM8302A /SD (amp enable).
// Enable it in the sketch with:  #define AUDIO_EN_PIN KCM_AUDIO_EN_PIN
// TODO: set to the TONE_EN GPIO from the KC-01-1911 V2.1 schematic (a free pin;
//       10/11/12/13/28/29/30 are candidates, minus whichever carries AUDIO_BUSY).
// #define KCM_AUDIO_EN_PIN  <pin>
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
#define KCM_I2C_ADDR_INFODISP     0x12   // Info Display 1 (INFO_DISP_UNIT == 1)
#define KCM_I2C_ADDR_INFODISP_2   0x13   // Info Display 2 (INFO_DISP_UNIT == 2) — same firmware, compile-time unit select
#define KCM_I2C_ADDR_SYSINFODISP  0x14   // System Info Display — FUTURE WORK (separate hardware board, not yet coded)
#define KCM_I2C_SYNC_ANNUNCIATOR  0xAC
#define KCM_I2C_SYNC_RESDISP      0xAD
#define KCM_I2C_SYNC_INFODISP     0xAE   // was 0xAD — collision fix (item #3); shared by Info Display 1 & 2

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
// Each sketch owns its own debug/demo mode flags in AAA_Config.ino (all default
// to false for production). The former KCM_DEFAULT_DEBUG_MODE / _DEMO_MODE macros
// were unused and defaulted to true, contradicting those per-sketch values, so
// they were removed. Only the display-rotation default is shared (KCM_Display.h
// uses it as the default argument to kcmDisplayBegin()).
#define KCM_DEFAULT_DISPLAY_ROTATION 0

// =============================================================================
// CROSS-PANEL ALIGNED THRESHOLDS
// These values must stay identical between Annunciator C&W logic and InfoDisp
// display thresholds. Edit here only — local constants in each sketch alias these.
// =============================================================================
#define KCM_GROUND_PROX_S         10.0f   // CW_GROUND_PROX_S / LNDG_TGRND_ALARM_S
#define KCM_HIGH_G_ALARM_POS       9.0f   // CW_HIGH_G_ALARM   / G_ALARM_POS  — red (Annunciator warning limit)
#define KCM_HIGH_G_ALARM_NEG      -5.0f   // CW_HIGH_G_WARN    / G_ALARM_NEG  — red (Annunciator warning limit)
#define KCM_HIGH_G_WARN_POS        4.0f   //                    G_WARN_POS   — yellow caution tier (InfoDisp two-tier gauges)
#define KCM_HIGH_G_WARN_NEG       -2.0f   //                    G_WARN_NEG   — yellow caution tier (InfoDisp two-tier gauges)
#define KCM_LOW_DV_MS            150.0f   // CW_LOW_DV_MS      / DV_STG_ALARM_MS
#define KCM_LOW_BURN_S            60.0f   // CW_LOW_BURN_S     / LNCH_BURNTIME_ALARM_S
#define KCM_TEMP_ALARM_PCT          90    // CW_HIGH_TEMP (tempAlarm) — core/skin temp % of limit
#define KCM_EC_LOW_ALARM_FRAC     0.05f   // CW_BUS_VOLTAGE (CW_EC_LOW_FRAC) — electric charge fraction
#define KCM_RES_LOW_WARN_FRAC     0.20f   // CW_PROP_LOW / CW_RCS_LOW yellow tier — generic "resource low" warn

// Chute deployment limits as dynamic pressure q = 0.5*airDensity*v^2 (Pa). A chute's
// structural limit is a q (force) limit and is body-independent; expressing it as q
// makes the safe-deploy SPEED automatically altitude-correct (higher up, lower near
// the ground). Calibrated to KSP's Kerbin sea-level safe speeds (~250 m/s main,
// ~500 m/s drogue) at rho0 ~= 1.225 kg/m^3.  Shared by Annunciator CW_CHUTE_ENV and
// InfoDisp re-entry chute status + deploy-envelope tape.
#define KCM_CHUTE_MAIN_MAX_Q     38300.0f  // main rips above this q (Pa)   ~250 m/s @ Kerbin SL
#define KCM_CHUTE_DROGUE_MAX_Q  153000.0f  // drogue rips above this q (Pa) ~500 m/s @ Kerbin SL
