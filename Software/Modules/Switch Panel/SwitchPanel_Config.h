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
//  Each switch occupies one bit in the 16-bit state word.
//  The controller reads these bits and executes the assigned
//  function. All switches are toggle-type (latching or
//  momentary flip) — the controller acts on state transitions
//  (rising/falling edges via the change mask), not held state,
//  unless noted otherwise.
//
//  Controller behavior notes per switch:
//
//  SW1  — LIVE CONTROL MODE
//         Rising edge: enable live telemetry/control link.
//         Falling edge: suspend. State held by controller.
//
//  SW2  — DEMO MODE
//         Rising edge: enter demo mode (simulated data, no live link).
//         Falling edge: exit demo mode.
//         Sets INDICATOR B8 (DEMO) pixel ACTIVE while on.
//
//  SW3  — MASTER RESET
//         Rising edge only: send CMD_RESET to all modules on bus.
//         Controller should re-enumerate and re-enable all modules.
//         Falling edge ignored — reset is a momentary action
//         regardless of switch type.
//
//  SW4  — DISPLAY RESET
//         Rising edge: send CMD_SET_VALUE 0x0000 to all display
//         modules (GPWS 0x2A, Pre-Warp 0x2B). Falling edge ignored.
//
//  SW5  — SCE TO AUXILIARY
//         Rising edge: activate SCE auxiliary power routing.
//         Falling edge: deactivate.
//         Sets INDICATOR B12 (SCE AUX) pixel ACTIVE while on.
//
//  SW6  — BULB TEST
//         Rising edge: send CMD_BULB_TEST (start) to all modules.
//         Falling edge: send CMD_BULB_TEST 0x00 (stop) to all modules.
//
//  SW7  — AUDIO ON
//         Rising edge: enable audio system.
//         Falling edge: disable audio system.
//         Sets INDICATOR B9 (AUDIO) pixel ACTIVE while on.
//
//  SW8  — PRECISION INPUT
//         Rising edge: enable precision input mode system-wide
//         (affects joystick axis scaling, other input scaling).
//         Falling edge: exit precision input mode.
//         Sets INDICATOR B6 (PREC INPUT) pixel ACTIVE while on.
//
//  SW9  — ENGINE ARM
//         Rising edge: arm engine ignition system.
//         Falling edge: disarm. Controller should require SW9 ON
//         before accepting throttle-up commands.
//
//  SW10 — THROTTLE FINE CONTROL
//         Rising edge: send CMD_SET_PRECISION 0x01 to Throttle (0x2C).
//         Falling edge: send CMD_SET_PRECISION 0x00 to Throttle (0x2C).
//         Sets INDICATOR B3 (THRTL PREC) pixel ACTIVE while on.
// ============================================================

/** @brief Bit 0 — Live Control Mode */
#define SWP_BIT_LIVE_CTRL       0

/** @brief Bit 1 — Demo Mode */
#define SWP_BIT_DEMO            1

/** @brief Bit 2 — Master Reset */
#define SWP_BIT_MASTER_RESET    2

/** @brief Bit 3 — Display Reset */
#define SWP_BIT_DISPLAY_RESET   3

/** @brief Bit 4 — SCE to Auxiliary */
#define SWP_BIT_SCE_AUX         4

/** @brief Bit 5 — Bulb Test */
#define SWP_BIT_BULB_TEST       5

/** @brief Bit 6 — Audio On */
#define SWP_BIT_AUDIO           6

/** @brief Bit 7 — Precision Input */
#define SWP_BIT_PREC_INPUT      7

/** @brief Bit 8 — Engine Arm */
#define SWP_BIT_ENGINE_ARM      8

/** @brief Bit 9 — Throttle Fine Control */
#define SWP_BIT_THRTL_FINE      9

/** @brief Bitmask helper — extract switch state from packet word.
 *  Usage: bool on = (stateWord >> SWP_BIT_xxx) & 0x01; */
#define SWP_MASK(bit)           (1u << (bit))

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
