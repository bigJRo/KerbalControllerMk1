/**
 * @file        KTC_Config.h
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Board configuration for the KC-01-9001 v2.0 / KC-01-9002
 *              Module Tester: pin map, on-board I2C addresses, INA228
 *              calibration and runner timing. Same hardware as the universal
 *              KCMk1_Module_Tester sketch (see its TesterConfig.h).
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#pragma once
#include <Arduino.h>

#define KTC_VERSION_STR       "1.0.0"

// ------------------------------------------------------------
//  XIAO RA4M1 pin map (schematic KC-01-9001 V2.0)
// ------------------------------------------------------------
#define KTC_PIN_CTP_INT       D0    // FT6236 touch interrupt (unused: polled)
#define KTC_PIN_INT_BUS       D1    // module INT, active low
#define KTC_PIN_MOD_RST       D2    // module reset, active low
#define KTC_PIN_TFT_DC        D3
#define KTC_PIN_BACKLITE      D6
#define KTC_PIN_TFT_CS        D7

// ------------------------------------------------------------
//  Shared I2C bus
// ------------------------------------------------------------
#define KTC_ADDR_FT6236       0x38
#define KTC_ADDR_INA228       0x40
#define KTC_ADDR_MIN          0x20
#define KTC_ADDR_MAX          0x2E
#define KTC_I2C_HZ            400000UL

// ------------------------------------------------------------
//  INA228 (15 mOhm shunt, 5 A full scale). Check against a bench
//  meter on first bring-up, as for the universal tester.
// ------------------------------------------------------------
#define KTC_INA228_RSHUNT     0.015f
#define KTC_INA228_IMAX       5.0f

// ------------------------------------------------------------
//  Timing
// ------------------------------------------------------------
#define KTC_POWER_POLL_MS     250   // INA228 refresh while a screen is up
#define KTC_PROBE_MS          500   // module presence probe while waiting
#define KTC_READ_GAP_MS       5     // minimum gap between INT-gated reads
#define KTC_RESULT_HOLD_MS    700   // auto steps: show PASS/FAIL this long
#define KTC_CMD_SETTLE_MS     30    // settle after a command before checking
