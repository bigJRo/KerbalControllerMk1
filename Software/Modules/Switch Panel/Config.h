/**
 * @file        Config.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Switch Panel Input Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Pin definitions, constants, and I2C command bytes
 *              for the Switch Panel Input Module.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include <stdint.h>

// ============================================================
//  Module identity
// ============================================================

#define SWP_I2C_ADDRESS         0x2E
#define SWP_MODULE_TYPE_ID      KMC_TYPE_SWITCH_PANEL
#define SWP_FIRMWARE_MAJOR      1
#define SWP_FIRMWARE_MINOR      0
#define SWP_CAP_FLAGS           0x00  // no capabilities

// ============================================================
//  Switch count
// ============================================================

#define SWP_SWITCH_COUNT        10

// ============================================================
//  I2C command bytes — aliases for KMC_CMD_* from KerbalModuleCommon
// ============================================================

#define CMD_GET_IDENTITY        KMC_CMD_GET_IDENTITY
#define CMD_SET_LED_STATE       KMC_CMD_SET_LED_STATE
#define CMD_SET_BRIGHTNESS      KMC_CMD_SET_BRIGHTNESS
#define CMD_BULB_TEST           KMC_CMD_BULB_TEST
#define CMD_SLEEP               KMC_CMD_SLEEP
#define CMD_WAKE                KMC_CMD_WAKE
#define CMD_RESET               KMC_CMD_RESET
#define CMD_ACK_FAULT           KMC_CMD_ACK_FAULT
#define CMD_ENABLE              KMC_CMD_ENABLE
#define CMD_DISABLE             KMC_CMD_DISABLE
// ============================================================

#define CMD_GET_IDENTITY        0x01
#define CMD_SET_LED_STATE       0x02
#define CMD_SET_BRIGHTNESS      0x03
#define CMD_BULB_TEST           0x04
#define CMD_SLEEP               0x05
#define CMD_WAKE                0x06
#define CMD_RESET               0x07
#define CMD_ACK_FAULT           0x08
#define CMD_ENABLE              0x09
#define CMD_DISABLE             0x0A

// ============================================================
//  Packet format
//
//  Standard 4-byte KBC-style packet (module → controller):
//    Byte 0:  Current state HIGH  — bits 15-8  (bits 10-15 always 0)
//    Byte 1:  Current state LOW   — bits 7-0   (SW1-SW8)
//    Byte 2:  Change mask HIGH    — bits 15-8  (bits 10-15 always 0)
//    Byte 3:  Change mask LOW     — bits 7-0
//
//  Switch-to-bit mapping:
//    Bit 0  = SW1
//    Bit 1  = SW2
//    Bit 2  = SW3
//    Bit 3  = SW4
//    Bit 4  = SW5
//    Bit 5  = SW6
//    Bit 6  = SW7
//    Bit 7  = SW8
//    Bit 8  = SW9
//    Bit 9  = SW10
//    Bits 10-15 = unused (always 0)
// ============================================================

#define SWP_PACKET_SIZE         4
#define SWP_IDENTITY_SIZE       KMC_IDENTITY_SIZE

// ============================================================
//  Pin assignments — ATtiny816
// ============================================================

/** @brief SW1  — PB4, active high, 10k pull-down. */
#define SWP_PIN_SW1             PIN_PB4

/** @brief SW2  — PC3, active high, 10k pull-down. */
#define SWP_PIN_SW2             PIN_PC3

/** @brief SW3  — PB5, active high, 10k pull-down. */
#define SWP_PIN_SW3             PIN_PB5

/** @brief SW4  — PC2, active high, 10k pull-down. */
#define SWP_PIN_SW4             PIN_PC2

/** @brief SW5  — PA7, active high, 10k pull-down. */
#define SWP_PIN_SW5             PIN_PA7

/** @brief SW6  — PC1, active high, 10k pull-down. */
#define SWP_PIN_SW6             PIN_PC1

/** @brief SW7  — PA6, active high, 10k pull-down. */
#define SWP_PIN_SW7             PIN_PA6

/** @brief SW8  — PC0, active high, 10k pull-down. */
#define SWP_PIN_SW8             PIN_PC0

/** @brief SW9  — PA5, active high, 10k pull-down. */
#define SWP_PIN_SW9             PIN_PA5

/** @brief SW10 — PB3, active high, 10k pull-down. */
#define SWP_PIN_SW10            PIN_PB3

// ============================================================
//  INT output pin
//
//  IMPORTANT: This module uses PIN_PB2 for INT, not PIN_PA1
//  as on all KC-01-1822-based standard modules.
//
//  Reason: On the KC-01 Switch Panel PCB, the PA port pins are
//  consumed by switch inputs (PA7=SW5, PA6=SW7, PA5=SW9) or are
//  not connected (PA4, PA3, PA2, PA1). PB2 is used for INT as it
//  is the available output-capable pin on this layout.
//
//  The master controller firmware must wire this module's
//  interrupt input to the PB2 net on the Switch Panel PCB,
//  not the PA1 net used by all other modules.
//
//  Verified against KC-01 Switch Panel schematic v1.0.
// ============================================================

/** @brief Interrupt output — PB2, active low.
 *  @note  Uses PB2, not PA1. See comment block above. */
#define SWP_PIN_INT             PIN_PB2

/** @brief I2C SCL — PB0. */
#define SWP_PIN_SCL             PIN_PB0

/** @brief I2C SDA — PB1. */
#define SWP_PIN_SDA             PIN_PB1

// ============================================================
//  Switch function assignments
//
//  Panel layout: 5 columns x 2 rows.
//  SW1 and SW2 are mechanically coupled to a single 3-position
//  latching switch (MODE). All other switches are independent.
//
//  +-----------+-----------+-----------+-----------+-----------+
//  | MODE      | MSTR      | SCE       | AUDIO     | ENGINE    |
//  | CTRL      | OPER      | NORM      | OFF       | SAFE      |
//  | SW1+SW2   | SW3       | SW5       | SW7       | SW9       |
//  | DEMO      | RESET     | AUX       | ON        | ARM       |
//  +-----------+-----------+-----------+-----------+-----------+
//  | (MODE     | DISPLAY   | LTG       | INPUT     | THRTL     |
//  |  lower)   | OPER      | OPER      | NORM      | STD       |
//  |           | SW4       | SW6       | SW8       | SW10      |
//  |           | RESET     | TEST      | FINE      | FINE      |
//  +-----------+-----------+-----------+-----------+-----------+
//
// ============================================================
//  MODE switch (SW1 + SW2) -- 3-position latching
//
//  A single 3-position latching switch drives both SW1 and SW2.
//  The two bits encode three mutually exclusive operating modes:
//
//    Position  SW1(bit0)  SW2(bit1)  Mode
//    Up        1          0          CTRL -- Live Control
//    Center    0          0          DBG  -- Debug
//    Down      0          1          DEMO -- Demo / Simulated
//
//  Both bits low = Debug (center). This is the safe power-on
//  default since all switches open means all bits = 0.
//
//  CTRL: enable live telemetry and control link
//        -> INDICATOR B2 (CTRL) ACTIVE
//  DBG:  debug mode -- live link suspended, local test data
//        -> INDICATOR B5 (DEBUG) ACTIVE
//  DEMO: simulated data mode -- no live link
//        -> INDICATOR B8 (DEMO) ACTIVE
// ============================================================

/** @brief Bit 0 -- MODE switch upper contact (CTRL / live control) */
#define SWP_BIT_MODE_CTRL       0

/** @brief Bit 1 -- MODE switch lower contact (DEMO / simulated) */
#define SWP_BIT_MODE_DEMO       1

/** @brief MODE decoded values -- use SWP_GET_MODE() to extract */
#define SWP_MODE_DBG            0   // center: SW1=0, SW2=0
#define SWP_MODE_CTRL           1   // up:     SW1=1, SW2=0
#define SWP_MODE_DEMO           2   // down:   SW1=0, SW2=1

/** @brief Extract mode from 16-bit state word (reads bits 1:0) */
#define SWP_GET_MODE(w)         ((w) & 0x03)

/**
 * @brief Bit 2 -- MSTR (Master Reset)
 *        Up=OPER, Down=RESET.
 *        Rising edge only: CMD_RESET all modules, re-enumerate.
 *        Falling edge ignored.
 */
#define SWP_BIT_MASTER_RESET    2

/**
 * @brief Bit 3 -- DISPLAY (Display Reset)
 *        Up=OPER, Down=RESET.
 *        Rising edge only: CMD_SET_VALUE 0 to GPWS (0x2A)
 *        and Pre-Warp (0x2B). Falling edge ignored.
 */
#define SWP_BIT_DISPLAY_RESET   3

/**
 * @brief Bit 4 -- SCE (SCE to Auxiliary)
 *        Up=NORM, Down=AUX.
 *        Toggle: activate/deactivate SCE auxiliary power routing.
 *        -> INDICATOR B12 (SCE AUX) ACTIVE while AUX.
 */
#define SWP_BIT_SCE_AUX         4

/**
 * @brief Bit 5 -- LTG (Lighting / Bulb Test)
 *        Up=OPER, Down=TEST.
 *        Rising edge: CMD_BULB_TEST (start) to all modules.
 *        Falling edge: CMD_BULB_TEST 0x00 (stop) to all modules.
 */
#define SWP_BIT_BULB_TEST       5

/**
 * @brief Bit 6 -- AUDIO
 *        Up=OFF, Down=ON.
 *        Toggle: enable/disable audio system.
 *        -> INDICATOR B9 (AUDIO) ACTIVE while ON.
 */
#define SWP_BIT_AUDIO           6

/**
 * @brief Bit 7 -- INPUT (Precision Input)
 *        Up=NORM, Down=FINE.
 *        Toggle: enable/disable system-wide precision input mode
 *        (joystick axis scaling and other input scaling).
 *        -> INDICATOR B6 (PREC INPUT) ACTIVE while FINE.
 */
#define SWP_BIT_PREC_INPUT      7

/**
 * @brief Bit 8 -- ENGINE (Engine Arm)
 *        Up=SAFE, Down=ARM.
 *        Toggle: arm/disarm engine ignition system.
 *        Controller requires ARM before accepting throttle-up.
 */
#define SWP_BIT_ENGINE_ARM      8

/**
 * @brief Bit 9 -- THRTL (Throttle Fine Control)
 *        Up=STD, Down=FINE.
 *        Rising edge: CMD_SET_PRECISION 0x01 -> Throttle (0x2C).
 *        Falling edge: CMD_SET_PRECISION 0x00 -> Throttle (0x2C).
 *        -> INDICATOR B3 (THRTL PREC) ACTIVE while FINE.
 */
#define SWP_BIT_THRTL_FINE      9

/** @brief Bitmask helper.
 *  Usage: bool on = (stateWord >> SWP_BIT_xxx) & 0x01; */
#define SWP_MASK(bit)           (1u << (bit))

// ============================================================
//  Timing
// ============================================================

/** @brief Switch poll interval in milliseconds. */
#ifndef SWP_POLL_INTERVAL_MS
  #define SWP_POLL_INTERVAL_MS  5
#endif

/**
 * @brief Consecutive matching reads to register a state change.
 *        At 5ms poll rate this gives 20ms effective debounce.
 */
#ifndef SWP_DEBOUNCE_COUNT
  #define SWP_DEBOUNCE_COUNT    4
#endif

/** @brief Poll interval during sleep mode. */
#ifndef SWP_SLEEP_POLL_MS
  #define SWP_SLEEP_POLL_MS     50
#endif
