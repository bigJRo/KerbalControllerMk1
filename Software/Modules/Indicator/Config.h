/**
 * @file        Config.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Indicator Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Pin definitions, constants, and I2C command bytes
 *              for the Indicator Module.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include <stdint.h>

// ============================================================
//  Module identity
// ============================================================

#define IND_I2C_ADDRESS         0x2F
#define IND_MODULE_TYPE_ID      0x10
#define IND_FIRMWARE_MAJOR      1
#define IND_FIRMWARE_MINOR      0
#define IND_CAP_FLAGS           0x00

// ============================================================
//  NeoPixel configuration
//
//  SK6812mini-012 uses RGB color order (not GRBW).
//  NEOPIX_CMD is on PA5 — Port A.
//  tinyNeoPixel IDE setting must be Port A.
// ============================================================

#define IND_NEO_COUNT           16
#define IND_NEO_COLOR_ORDER     NEO_RGB
#define IND_ENABLED_BRIGHTNESS  32   // dim white W channel not used — scale RGB

// ============================================================
//  I2C command bytes
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
//  Packet sizes
// ============================================================

#define IND_IDENTITY_SIZE       4
#define IND_LED_PAYLOAD_SIZE    8    // 8 bytes, 16 nibbles, 1 per pixel

// ============================================================
//  KBC LED state nibble values
// ============================================================

#define LED_STATE_OFF           0x0
#define LED_STATE_ENABLED       0x1
#define LED_STATE_ACTIVE        0x2
#define LED_STATE_WARNING       0x3
#define LED_STATE_ALERT         0x4
#define LED_STATE_ARMED         0x5
#define LED_STATE_PARTIAL       0x6

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

/** @brief NeoPixel data output — PA5, Port A. */
#define IND_PIN_NEOPIX          PIN_PA5

/** @brief Interrupt output — PA1, active low. */
#define IND_PIN_INT             PIN_PA1

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
