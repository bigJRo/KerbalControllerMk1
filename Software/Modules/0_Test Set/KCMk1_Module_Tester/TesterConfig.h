/**
 * @file        TesterConfig.h
 * @version     2.0.0
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Board configuration for the KC-01-9001 v2.0 / KC-01-9002
 *              Module Tester: pin map, I2C addresses, and app constants.
 *
 *              Hardware (per schematic KC-01-9001 V2.0):
 *                MCU      : Seeed XIAO RA4M1 (Renesas RA4M1, Arduino UNO R4 core)
 *                Display  : ER-TFT028A3-4 — 2.8" 240x320 ILI9341, 4-wire SPI
 *                Touch    : FT6236 capacitive, I2C + CTP_INT
 *                Power    : 12V in -> MPM3610 (5V) -> AP2112K (3V3),
 *                           soft-latching power switch
 *                Telemetry: INA228 high-side V/I sensor (15 mOhm shunt) on
 *                           the module-under-test power feed
 *                Module   : connected via P1 (12V, 3V3, GND, I2C, INT, RST);
 *                           3.3V logic, direct (no level shifter)
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */

#pragma once
#include <Arduino.h>

// ============================================================
//  Firmware version
// ============================================================
#define TESTER_VERSION_MAJOR   2
#define TESTER_VERSION_MINOR   0
#define TESTER_VERSION_PATCH   0
#define TESTER_VERSION_STR     "2.0.0"

// ============================================================
//  XIAO RA4M1 pin map (see schematic page P1, "Microcontroller")
// ============================================================
#define PIN_CTP_INT     D0    // Capacitive-touch interrupt (FT6236), active low
#define PIN_INT_BUS     D1    // Module-under-test INT, active low (pulled to 3V3)
#define PIN_MOD_RST     D2    // Module-under-test reset, active low
#define PIN_TFT_DC      D3    // TFT data/command
// D4 = SDA, D5 = SCL  -> hardware I2C (Wire), shared bus
#define PIN_BACKLITE    D6    // TFT backlight enable (active high)
#define PIN_TFT_CS      D7    // TFT chip select
#define PIN_TFT_SCK     D8    // SPI clock
#define PIN_TFT_MISO    D9    // SPI MISO (touch/SD unused here; TFT read)
#define PIN_TFT_MOSI    D10   // SPI MOSI
// The ILI9341 reset pin is not broken out on the panel connector — the
// driver uses software reset (panel rst = -1).

// ============================================================
//  I2C addresses on the shared bus
//
//  Module-under-test occupies 0x20-0x2E. The on-board touch controller
//  and power monitor also sit on this bus and MUST be excluded from the
//  module scan.
// ============================================================
#define I2C_MODULE_ADDR_MIN   0x20
#define I2C_MODULE_ADDR_MAX   0x2E
#define I2C_ADDR_FT6236       0x38   // capacitive touch controller
#define I2C_ADDR_INA228       0x40   // power monitor (A0/A1 = GND)
#define I2C_BUS_HZ            400000UL

// ============================================================
//  Power monitor calibration (INA228, U3)
//    Shunt R3 = 15 mOhm. Max expected module current ~5 A.
//    CURRENT_LSB = MaxCurrent / 2^19; SHUNT_CAL = 13107.2e6 * LSB * Rshunt.
//    Verify against bench readings on first bring-up.
// ============================================================
#define INA228_RSHUNT_OHMS    0.015f
#define INA228_MAX_CURRENT_A  5.0f

// ============================================================
//  Display geometry (landscape — rotation set in TesterUI)
// ============================================================
#define TFT_NATIVE_W   240
#define TFT_NATIVE_H   320
#define UI_W           320     // landscape
#define UI_H           240

// ============================================================
//  Timing
// ============================================================
#define SCAN_INTERVAL_MS     750    // bus rescan cadence on the scan screen
#define POWER_POLL_MS        250    // INA228 refresh
#define INPUT_POLL_MS        20     // module packet poll on the dashboard
#define SPLASH_MS            1200
