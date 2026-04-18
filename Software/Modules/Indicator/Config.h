/**
 * @file        Config.h
 * @version     2.0
 * @date        2026-04-09
 * @project     Kerbal Controller Mk1 — Indicator Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Pin definitions, constants, and I2C command bytes
 *              for the Indicator Module.
 *
 *              v2.0 changes:
 *                - NeoPixel count expanded from 16 to 18
 *                - IND_LED_PAYLOAD_SIZE expanded from 8 to 9 bytes
 *                  (18 nibbles, 1 per pixel — module-local override,
 *                  does not affect KMC_LED_PAYLOAD_SIZE globally)
 *                - Pixel map revised: USB ACTIVE removed, SCE AUX,
 *                  AUTO PILOT, and ABORT added
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include <stdint.h>
#include <KerbalModuleCommon.h>

// ============================================================
//  Module identity
// ============================================================

#define IND_I2C_ADDRESS         0x2F
#define IND_MODULE_TYPE_ID      KMC_TYPE_INDICATOR
#define IND_FIRMWARE_MAJOR      2
#define IND_FIRMWARE_MINOR      0
#define IND_CAP_FLAGS           KMC_CAP_EXTENDED_STATES

// ============================================================
//  NeoPixel configuration
//
//  SK6812mini-012 uses RGB color order (not GRBW).
//  NEOPIX_CMD is on PA5 — Port A.
//  tinyNeoPixel IDE setting must be Port A.
// ============================================================

#define IND_NEO_COUNT           18
#define IND_NEO_COLOR_ORDER     NEO_RGB
#define IND_ENABLED_BRIGHTNESS  32   // dim white — scale RGB

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
//  Packet sizes
//
//  IND_LED_PAYLOAD_SIZE is a module-local override — 18 pixels
//  require 9 bytes (18 nibbles, 2 per byte). This intentionally
//  differs from KMC_LED_PAYLOAD_SIZE (8 bytes / 16 pixels).
//  The controller must use 9-byte payloads for this module.
// ============================================================

#define IND_IDENTITY_SIZE       KMC_IDENTITY_SIZE
#define IND_LED_PAYLOAD_SIZE    9    // 9 bytes, 18 nibbles, 1 per pixel

// ============================================================
//  KBC LED state nibble values — aliases for KMC_LED_*
// ============================================================

#define LED_STATE_OFF           KMC_LED_OFF
#define LED_STATE_ENABLED       KMC_LED_ENABLED
#define LED_STATE_ACTIVE        KMC_LED_ACTIVE
#define LED_STATE_WARNING       KMC_LED_WARNING
#define LED_STATE_ALERT         KMC_LED_ALERT
#define LED_STATE_ARMED         KMC_LED_ARMED
#define LED_STATE_PARTIAL       KMC_LED_PARTIAL_DEPLOY

// ============================================================
//  Flash timing (milliseconds)
// ============================================================

#define IND_WARNING_ON_MS       500
#define IND_WARNING_OFF_MS      500
#define IND_ALERT_ON_MS         150
#define IND_ALERT_OFF_MS        150
#define IND_BULB_TEST_MS        2000

// ============================================================
//  Pin assignments — ATtiny816
// ============================================================

/** @brief NeoPixel data output — PA5, Port A.
 *  @note  tinyNeoPixel IDE port setting must be Port A for this module.
 *         All other NeoPixel modules use Port C. */
#define IND_PIN_NEOPIX          PIN_PA5

/** @brief Interrupt output — PC3, active low.
 *  @note  This module uses PIN_PC3 for INT, not PIN_PA1 as on
 *         KC-01-1822-based modules. PA1 is not connected on this PCB.
 *         The master controller INT wiring must account for this
 *         divergence. Verified against Indicator Module schematic v1.0. */
#define IND_PIN_INT             PIN_PC3

/** @brief I2C SCL — PB0. */
#define IND_PIN_SCL             PIN_PB0

/** @brief I2C SDA — PB1. */
#define IND_PIN_SDA             PIN_PB1

// ============================================================
//  Encoder header pins (H1, H2) — not currently installed.
//  Defined here for future expansion reference.
// ============================================================

/** @brief ENC1 channel A — PA6 (H1, not installed). */
#define IND_PIN_ENC1_A          PIN_PA6

/** @brief ENC1 channel B — PA7 (H1, not installed). */
#define IND_PIN_ENC1_B          PIN_PA7

/** @brief ENC1 pushbutton — PB5 (H1, not installed). */
#define IND_PIN_ENC1_SW         PIN_PB5

/** @brief ENC2 channel A — PB4 (H2, not installed). */
#define IND_PIN_ENC2_A          PIN_PB4

/** @brief ENC2 channel B — PB3 (H2, not installed). */
#define IND_PIN_ENC2_B          PIN_PB3

/** @brief ENC2 pushbutton — PB2 (H2, not installed). */
#define IND_PIN_ENC2_SW         PIN_PB2
