/**
 * @file        Config.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — Dual Encoder Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Pin definitions, constants, and I2C command bytes
 *              for the Dual Encoder Module standalone sketch.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include <stdint.h>

// ============================================================
//  Module identity
// ============================================================

#define DEC_I2C_ADDRESS         0x2D
#define DEC_MODULE_TYPE_ID      KMC_TYPE_DUAL_ENCODER
#define DEC_FIRMWARE_MAJOR      1
#define DEC_FIRMWARE_MINOR      0

/** @brief Capability flag — encoder delta data in packet (bit 2). */
#define DEC_CAP_ENCODERS        KMC_CAP_ENCODERS

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
//  4 bytes (module → controller):
//    Byte 0:  Button events  (bit0=ENC1_SW, bit1=ENC2_SW)
//    Byte 1:  Change mask    (same bit layout)
//    Byte 2:  ENC1 delta     (signed int8, +CW, -CCW)
//    Byte 3:  ENC2 delta     (signed int8, +CW, -CCW)
// ============================================================

#define DEC_PACKET_SIZE         4
#define DEC_IDENTITY_SIZE       KMC_IDENTITY_SIZE

// ============================================================
//  Button bit assignments
// ============================================================

#define DEC_BIT_ENC1_SW         0
#define DEC_BIT_ENC2_SW         1

// ============================================================
//  Pin assignments — ATtiny816 KC-01-1871/1872 v1.0
// ============================================================

/** @brief ENC1 channel A — PC1, hardware RC debounced. */
#define DEC_PIN_ENC1_A          PIN_PC1

/** @brief ENC1 channel B — PC2, hardware RC debounced. */
#define DEC_PIN_ENC1_B          PIN_PC2

/** @brief ENC1 pushbutton — PC3, active high, 10k pull-down. */
#define DEC_PIN_ENC1_SW         PIN_PC3

/** @brief ENC2 channel A — PA6, hardware RC debounced. */
#define DEC_PIN_ENC2_A          PIN_PA6

/** @brief ENC2 channel B — PA5, hardware RC debounced. */
#define DEC_PIN_ENC2_B          PIN_PA5

/** @brief ENC2 pushbutton — PA4, active high, 10k pull-down. */
#define DEC_PIN_ENC2_SW         PIN_PA4

/** @brief Interrupt output — PA1, active low. */
#define DEC_PIN_INT             PIN_PA1

/** @brief I2C SCL — PB0. */
#define DEC_PIN_SCL             PIN_PB0

/** @brief I2C SDA — PB1. */
#define DEC_PIN_SDA             PIN_PB1

// ============================================================
//  Timing
// ============================================================

/** @brief Poll interval in milliseconds. */
#ifndef DEC_POLL_INTERVAL_MS
  #define DEC_POLL_INTERVAL_MS  5
#endif

/**
 * @brief Minimum milliseconds between encoder state changes.
 *        Hardware RC debounce (10nF) handles most bounce.
 *        This is a software guard against residual glitches.
 */
#ifndef DEC_ENC_DEBOUNCE_MS
  #define DEC_ENC_DEBOUNCE_MS   2
#endif

/** @brief Consecutive matching reads to register a button change. */
#ifndef DEC_BTN_DEBOUNCE_COUNT
  #define DEC_BTN_DEBOUNCE_COUNT 4
#endif

/** @brief Poll interval during sleep mode. */
#ifndef DEC_SLEEP_POLL_MS
  #define DEC_SLEEP_POLL_MS     50
#endif
