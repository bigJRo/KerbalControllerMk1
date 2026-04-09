/**
 * @file        K7SC_Config.h
 * @version     1.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Pin definitions, constants, timing parameters, and
 *              threshold values for the Kerbal7SegmentCore library.
 *
 *              Override any #ifndef-guarded constant by defining it
 *              before including Kerbal7SegmentCore.h in the sketch.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware: KC-01-1881/1882 7-Segment Display Module v2.0
 *              Target:   ATtiny816-MNR (megaTinyCore)
 */

#pragma once
#include <stdint.h>
#include <KerbalModuleCommon.h>

// ============================================================
//  Clock speed assertion
// ============================================================

#if F_CPU < 7372800UL
  #error "K7SC: CPU clock too slow for NeoPixel timing. Minimum 7.37 MHz required."
#endif

// ============================================================
//  Firmware version
// ============================================================

#define K7SC_FIRMWARE_MAJOR     1
#define K7SC_FIRMWARE_MINOR     0

// ============================================================
//  Button counts and indices
// ============================================================

/** @brief Total button count (BTN01, BTN02, BTN03, BTN_EN). */
#define K7SC_BUTTON_COUNT       4

/** @brief Number of NeoPixel buttons (SK6812MINI-EA). */
#define K7SC_NEO_COUNT          3

// ============================================================
//  Button bit assignments in event and state packets
// ============================================================

#define K7SC_BIT_BTN01          0
#define K7SC_BIT_BTN02          1
#define K7SC_BIT_BTN03          2
#define K7SC_BIT_BTN_EN         3

// ============================================================
//  State byte bit assignments (Byte 2 of data packet)
//
//  Bits 0-1: BTN01 state (0=off, 1=state1, 2=state2)
//  Bit  2:   BTN02 active
//  Bit  3:   BTN03 active
// ============================================================

#define K7SC_STATE_BTN01_MASK   0x03  // bits 0-1
#define K7SC_STATE_BTN02_BIT    2
#define K7SC_STATE_BTN03_BIT    3

// ============================================================
//  Pin assignments — ATtiny816 KC-01-1881/1882 v2.0
// ============================================================

/** @brief MAX7219 SPI clock — PA7. */
#define K7SC_PIN_CLK            PIN_PA7

/** @brief MAX7219 SPI latch / chip select (active high) — PA5.
 *         Schematic net name: LOAD_DATA. Driven LOW to begin a
 *         16-bit transaction, driven HIGH to latch data into MAX7219. */
#define K7SC_PIN_LOAD           PIN_PA5

/** @brief MAX7219 SPI serial data — PA6.
 *         Schematic net name: DATA_IN. Serial bits shifted MSB first
 *         on rising edge of CLK while LOAD is held LOW. */
#define K7SC_PIN_DATA           PIN_PA6

/** @brief BUTTON01 — PA1, direct GPIO, active high. */
#define K7SC_PIN_BTN01          PIN_PA1

/** @brief BUTTON02 — PC2, direct GPIO, active high. */
#define K7SC_PIN_BTN02          PIN_PC2

/** @brief BUTTON03 — PC1, direct GPIO, active high. */
#define K7SC_PIN_BTN03          PIN_PC1

/** @brief BUTTON_EN — encoder pushbutton, PB3, active high. */
#define K7SC_PIN_BTN_EN         PIN_PB3

/** @brief NeoPixel data output — PC3 (Port C). */
#define K7SC_PIN_NEOPIX         PIN_PC3

/** @brief Rotary encoder A channel — PB4. */
#define K7SC_PIN_ENC_A          PIN_PB4

/** @brief Rotary encoder B channel — PB5. */
#define K7SC_PIN_ENC_B          PIN_PB5

/** @brief Interrupt output — PC0, active low.
 *  @note  This module uses PIN_PC0 for INT, not PIN_PA1 as on
 *         KC-01-1822-based modules. PA1 is used for BUTTON01 on
 *         this PCB. The master controller INT wiring must account
 *         for this divergence. Verified against KC-01-1882 v2.0. */
#define K7SC_PIN_INT            PIN_PC0

/** @brief I2C SCL — PB0. */
#define K7SC_PIN_SCL            PIN_PB0

/** @brief I2C SDA — PB1. */
#define K7SC_PIN_SDA            PIN_PB1

// ============================================================
//  NeoPixel configuration
//
//  SK6812MINI-EA uses GRBW color order (4 bytes per pixel).
//  White channel is used for ENABLED dim state.
// ============================================================

/** @brief SK6812MINI-EA color order — GRBW. */
#define K7SC_NEO_COLOR_ORDER    NEO_GRBW

// ============================================================
//  Display value range
// ============================================================

/** @brief Minimum displayable value. */
#define K7SC_VALUE_MIN          0

/** @brief Maximum displayable value. */
#define K7SC_VALUE_MAX          9999

// ============================================================
//  Encoder acceleration thresholds
//
//  Time between encoder clicks determines step size.
//  All thresholds in milliseconds.
// ============================================================

/**
 * @brief Slow scroll threshold in ms.
 *        Clicks arriving slower than this use step size 1.
 */
#ifndef K7SC_ENC_SLOW_MS
  #define K7SC_ENC_SLOW_MS      150
#endif

/**
 * @brief Fast scroll threshold in ms.
 *        Clicks arriving faster than this use step size 100.
 *        Clicks between K7SC_ENC_FAST_MS and K7SC_ENC_SLOW_MS
 *        use step size 10.
 */
#ifndef K7SC_ENC_FAST_MS
  #define K7SC_ENC_FAST_MS      50
#endif

/** @brief Step size for slow scrolling (>K7SC_ENC_SLOW_MS). */
#ifndef K7SC_STEP_SLOW
  #define K7SC_STEP_SLOW        1
#endif

/** @brief Step size for medium scrolling. */
#ifndef K7SC_STEP_MEDIUM
  #define K7SC_STEP_MEDIUM      10
#endif

/** @brief Step size for fast scrolling (<K7SC_ENC_FAST_MS). */
#ifndef K7SC_STEP_FAST
  #define K7SC_STEP_FAST        100
#endif

// ============================================================
//  Encoder hardware debounce
//
//  The KC-01-1881 PCB has hardware RC debounce (10nF caps) on
//  ENC_A and ENC_B. Software debounce delay is minimal.
// ============================================================

/** @brief Minimum ms between encoder state changes (software guard). */
#ifndef K7SC_ENC_DEBOUNCE_MS
  #define K7SC_ENC_DEBOUNCE_MS  2
#endif

// ============================================================
//  Button debounce
// ============================================================

/** @brief Consecutive matching reads to register a button change. */
#ifndef K7SC_BTN_DEBOUNCE_COUNT
  #define K7SC_BTN_DEBOUNCE_COUNT 4
#endif

// ============================================================
//  Button flash duration
//
//  Used for preset buttons (Pre-Warp module) that flash on press.
// ============================================================

/** @brief Duration of button flash in milliseconds. */
#ifndef K7SC_FLASH_DURATION_MS
  #define K7SC_FLASH_DURATION_MS  150
#endif

// ============================================================
//  LED brightness
// ============================================================

/**
 * @brief ENABLED state white channel brightness (0-255).
 *        Applied to the W channel of the SK6812MINI-EA only.
 *        RGB channels are 0 in ENABLED state.
 */
#ifndef K7SC_ENABLED_BRIGHTNESS
  #define K7SC_ENABLED_BRIGHTNESS 32
#endif

// ============================================================
//  Poll interval
// ============================================================

/** @brief Button and encoder poll interval in milliseconds. */
#ifndef K7SC_POLL_INTERVAL_MS
  #define K7SC_POLL_INTERVAL_MS   5
#endif

/** @brief Poll interval during sleep mode. */
#ifndef K7SC_SLEEP_POLL_MS
  #define K7SC_SLEEP_POLL_MS      50
#endif

// ============================================================
//  Packet format
// ============================================================

/**
 * @brief Data packet size in bytes.
 *        Byte 0:   Button events  (bit0=BTN01, bit1=BTN02,
 *                                  bit2=BTN03, bit3=BTN_EN)
 *        Byte 1:   Change mask    (same bit layout)
 *        Byte 2:   Module state   (bits 0-1 = BTN01 cycle state,
 *                                  bit 2 = BTN02 active,
 *                                  bit 3 = BTN03 active)
 *        Byte 3:   Reserved
 *        Byte 4-5: Display value  (uint16, big-endian, 0-9999)
 */
#define K7SC_PACKET_SIZE        6

/** @brief Identity response packet size. */
#define K7SC_IDENTITY_SIZE      KMC_IDENTITY_SIZE

/** @brief LED state payload size (nibble-packed). */
#define K7SC_LED_PAYLOAD_SIZE   8

// ============================================================
//  LED state nibble values — aliases for KMC_LED_* from KerbalModuleCommon
// ============================================================

#define K7SC_LED_OFF            KMC_LED_OFF
#define K7SC_LED_ENABLED        KMC_LED_ENABLED
#define K7SC_LED_ACTIVE         KMC_LED_ACTIVE
#define K7SC_LED_WARNING        KMC_LED_WARNING
#define K7SC_LED_ALERT          KMC_LED_ALERT
#define K7SC_LED_ARMED          KMC_LED_ARMED
#define K7SC_LED_PARTIAL_DEPLOY KMC_LED_PARTIAL_DEPLOY

// ============================================================
//  I2C command bytes — aliases for KMC_CMD_* from KerbalModuleCommon
// ============================================================

#define K7SC_CMD_GET_IDENTITY   KMC_CMD_GET_IDENTITY
#define K7SC_CMD_SET_LED_STATE  KMC_CMD_SET_LED_STATE
#define K7SC_CMD_SET_BRIGHTNESS KMC_CMD_SET_BRIGHTNESS
#define K7SC_CMD_BULB_TEST      KMC_CMD_BULB_TEST
#define K7SC_CMD_SLEEP          KMC_CMD_SLEEP
#define K7SC_CMD_WAKE           KMC_CMD_WAKE
#define K7SC_CMD_RESET          KMC_CMD_RESET
#define K7SC_CMD_ACK_FAULT      KMC_CMD_ACK_FAULT
#define K7SC_CMD_ENABLE         KMC_CMD_ENABLE
#define K7SC_CMD_DISABLE        KMC_CMD_DISABLE

/** @brief Set the display value from the controller (display modules only). */
#define K7SC_CMD_SET_VALUE      KMC_CMD_SET_VALUE

// ============================================================
//  Capability flags
// ============================================================

/** @brief This module has a 7-segment display and encoder. */
#define K7SC_CAP_DISPLAY        KMC_CAP_DISPLAY

// ============================================================
//  MAX7219 register addresses
// ============================================================

#define K7SC_MAX_REG_NOOP       0x00
#define K7SC_MAX_REG_DIGIT0     0x01  // rightmost digit
#define K7SC_MAX_REG_DIGIT1     0x02
#define K7SC_MAX_REG_DIGIT2     0x03
#define K7SC_MAX_REG_DIGIT3     0x04  // leftmost digit
#define K7SC_MAX_REG_DECODE     0x09
#define K7SC_MAX_REG_INTENSITY  0x0A
#define K7SC_MAX_REG_SCANLIMIT  0x0B
#define K7SC_MAX_REG_SHUTDOWN   0x0C
#define K7SC_MAX_REG_DISPLAYTEST 0x0F

/** @brief MAX7219 BCD blank digit code (no leading zeros). */
#define K7SC_MAX_BLANK          0x0F

/** @brief MAX7219 display intensity (0-15). */
#ifndef K7SC_MAX_INTENSITY
  #define K7SC_MAX_INTENSITY    8
#endif
