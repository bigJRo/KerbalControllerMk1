/**
 * @file        K7SC_Config.h
 * @version     2.0.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Pin definitions, constants, and timing parameters for
 *              the Kerbal7SegmentCore library.
 *
 *              Override any #ifndef-guarded constant by defining it
 *              before including Kerbal7SegmentCore.h in the sketch.
 *
 * @license     GNU General Public License v3.0
 *
 * @note        Hardware: KC-01-1881/1882 7-Segment Display Module v2.0
 *              Target:   ATtiny816-MNR (megaTinyCore)
 */

#pragma once
#include <stdint.h>
#include <KerbalModuleCommon.h>

// ============================================================
//  Clock speed assertion
//
//  tinyNeoPixel (megaTinyCore bundled) requires minimum 2 MHz.
// ============================================================

#if F_CPU < 2000000UL
  #error "K7SC: CPU clock too slow for NeoPixel timing. Minimum 2 MHz required."
#endif

// ============================================================
//  Firmware version
// ============================================================

#define K7SC_FIRMWARE_MAJOR     1
#define K7SC_FIRMWARE_MINOR     0

// ============================================================
//  Button counts and bitmask indices
//
//  K7SC_BUTTON_COUNT includes BTN_EN (encoder pushbutton).
//  K7SC_NEO_COUNT covers only the 3 illuminated action buttons.
//
//  Bitmask layout (buttonPressed / buttonReleased / buttonChanged):
//    bit 0 = BTN01  (PA1)
//    bit 1 = BTN02  (PC2)
//    bit 2 = BTN03  (PC1)
//    bit 3 = BTN_EN (PB3, encoder pushbutton)
// ============================================================

#define K7SC_BUTTON_COUNT       4
#define K7SC_NEO_COUNT          3

#define K7SC_BIT_BTN01          0
#define K7SC_BIT_BTN02          1
#define K7SC_BIT_BTN03          2
#define K7SC_BIT_BTN_EN         3

// ============================================================
//  Module state byte bit layout (bytes 5+ of data packet)
//
//  The state byte encoding is module-specific. The sketch
//  assembles this byte and passes it in the payload to
//  k7scQueuePacket(). These masks document the GPWS module
//  convention and may differ for other module types.
// ============================================================

#define K7SC_STATE_BTN01_MASK   0x03  // bits 0-1: BTN01 cycle state (0/1/2)
#define K7SC_STATE_BTN02_BIT    2     // bit 2:    BTN02 active
#define K7SC_STATE_BTN03_BIT    3     // bit 3:    BTN03 active

// ============================================================
//  Pin assignments — ATtiny816, KC-01-1881/1882 v2.0
// ============================================================

/** @brief MAX7219 SPI clock — PA7. */
#define K7SC_PIN_CLK            PIN_PA7

/** @brief MAX7219 SPI data in — PA6. Serial bits shifted MSB first
 *         on rising CLK edge while LOAD is held low. */
#define K7SC_PIN_DATA           PIN_PA6

/** @brief MAX7219 SPI latch — PA5. Active-high: drive low during
 *         16-bit transfer, drive high to latch data into MAX7219.
 *         Schematic net name: LOAD_DATA. */
#define K7SC_PIN_LOAD           PIN_PA5

/** @brief BTN01 — PA1, active high. */
#define K7SC_PIN_BTN01          PIN_PA1

/** @brief BTN02 — PC2, active high. */
#define K7SC_PIN_BTN02          PIN_PC2

/** @brief BTN03 — PC1, active high. */
#define K7SC_PIN_BTN03          PIN_PC1

/** @brief BTN_EN — encoder pushbutton, PB3, active high (10k pull-down R6). */
#define K7SC_PIN_BTN_EN         PIN_PB3

/** @brief NeoPixel data out — PC3. */
#define K7SC_PIN_NEOPIX         PIN_PC3

/** @brief Encoder A channel — PB4 (interrupt source). */
#define K7SC_PIN_ENC_A          PIN_PB4

/** @brief Encoder B channel — PB5 (sampled in ISR). */
#define K7SC_PIN_ENC_B          PIN_PB5

/** @brief INT output to master — PC0, active low.
 *  @note  This module uses PC0 for INT. PA1 is used for BTN01
 *         on this PCB. Master wiring must account for this.
 *         Verified against KC-01-1880 v2.0 schematic. */
#define K7SC_PIN_INT            PIN_PC0

/** @brief I2C SCL — PB0. */
#define K7SC_PIN_SCL            PIN_PB0

/** @brief I2C SDA — PB1. */
#define K7SC_PIN_SDA            PIN_PB1

// ============================================================
//  NeoPixel configuration
//
//  SK6812MINI-EA: GRB 3-byte mode. NEO_GRB is the hardware-
//  validated configuration on KC-01-1880 v2.0 at 20 MHz.
// ============================================================

/** @brief SK6812MINI-EA colour order — GRB (3 bytes/pixel). */
#define K7SC_NEO_COLOR_ORDER    NEO_GRB

// ============================================================
//  Display value range
// ============================================================

#define K7SC_VALUE_MIN          0
#define K7SC_VALUE_MAX          9999

// ============================================================
//  Encoder acceleration — consecutive click count thresholds
//
//  Step size is determined by how many consecutive clicks have
//  arrived in the same direction. Purely event-driven, no
//  timing involved — immune to mechanical jitter.
//
//  Progression (clicks in same direction):
//    1  to MEDIUM_COUNT-1  →  STEP_SLOW   (  1)
//    MEDIUM_COUNT to FAST_COUNT-1  →  STEP_MEDIUM ( 10)
//    FAST_COUNT   to TURBO_COUNT-1 →  STEP_FAST   (100)
//    TURBO_COUNT  and above        →  STEP_TURBO  (1000)
//
//  Direction reversal resets the count — first click of a new
//  direction always steps by STEP_SLOW (1).
//
//  K7SC_ENC_ACCEL_TIMEOUT_MS: if no encoder event arrives within
//  this window, the consecutive click count resets to 0 — the
//  next click always starts at STEP_SLOW regardless of direction.
//  Set to 0 to disable the timeout.
//
//  All thresholds are overridable via #define before include.
// ============================================================

#ifndef K7SC_ENC_MEDIUM_COUNT
  #define K7SC_ENC_MEDIUM_COUNT   15
#endif

#ifndef K7SC_ENC_FAST_COUNT
  #define K7SC_ENC_FAST_COUNT     30
#endif

#ifndef K7SC_ENC_TURBO_COUNT
  #define K7SC_ENC_TURBO_COUNT    50
#endif

#if K7SC_ENC_FAST_COUNT <= K7SC_ENC_MEDIUM_COUNT
  #error "K7SC_ENC_FAST_COUNT must be greater than K7SC_ENC_MEDIUM_COUNT"
#endif
#if K7SC_ENC_TURBO_COUNT <= K7SC_ENC_FAST_COUNT
  #error "K7SC_ENC_TURBO_COUNT must be greater than K7SC_ENC_FAST_COUNT"
#endif

#ifndef K7SC_STEP_SLOW
  #define K7SC_STEP_SLOW          1
#endif

#ifndef K7SC_STEP_MEDIUM
  #define K7SC_STEP_MEDIUM        10
#endif

#ifndef K7SC_STEP_FAST
  #define K7SC_STEP_FAST          100
#endif

#ifndef K7SC_STEP_TURBO
  #define K7SC_STEP_TURBO         1000
#endif

#ifndef K7SC_ENC_ACCEL_TIMEOUT_MS
  #define K7SC_ENC_ACCEL_TIMEOUT_MS  500
#endif

// ============================================================
//  Button debounce
//
//  Time-based: state change registers only after the new state
//  has been stable for K7SC_BTN_DEBOUNCE_MS. The SK6812MINI-EA
//  integrated switch can be bouncier than discrete tactiles.
//  Hardware RC debounce (10nF) on encoder channels handles
//  ENC_A and ENC_B — no software debounce needed for encoder.
// ============================================================

#ifndef K7SC_BTN_DEBOUNCE_MS
  #define K7SC_BTN_DEBOUNCE_MS    30
#endif

// ============================================================
//  Poll interval
// ============================================================

/** @brief Button and encoder poll interval in milliseconds. */
#ifndef K7SC_POLL_INTERVAL_MS
  #define K7SC_POLL_INTERVAL_MS   5
#endif

// ============================================================
//  Packet format
//
//  Universal header (bytes 0-2, built by library):
//    Byte 0:  Status byte
//               bits 1:0  lifecycle (00=ACTIVE, 01=SLEEPING,
//                                    10=DISABLED, 11=BOOT_READY)
//               bit  2    fault flag
//               bit  3    data_changed (always set when packet queued)
//    Byte 1:  Module Type ID
//    Byte 2:  Transaction counter (uint8, wraps 255→0)
//
//  Module payload (bytes 3+, assembled by sketch):
//    Content and length are module-specific per protocol spec.
//    The library treats the payload as opaque bytes.
// ============================================================

#define K7SC_HEADER_SIZE        3
#define K7SC_IDENTITY_SIZE      4

// ============================================================
//  MAX7219 register addresses
// ============================================================

#define K7SC_MAX_REG_NOOP        0x00
#define K7SC_MAX_REG_DIGIT0      0x01  // rightmost digit
#define K7SC_MAX_REG_DIGIT1      0x02
#define K7SC_MAX_REG_DIGIT2      0x03
#define K7SC_MAX_REG_DIGIT3      0x04  // leftmost digit
#define K7SC_MAX_REG_DECODE      0x09
#define K7SC_MAX_REG_INTENSITY   0x0A
#define K7SC_MAX_REG_SCANLIMIT   0x0B
#define K7SC_MAX_REG_SHUTDOWN    0x0C
#define K7SC_MAX_REG_DISPLAYTEST 0x0F

/** @brief MAX7219 BCD blank digit code (suppresses leading zeros). */
#define K7SC_MAX_BLANK           0x0F

/** @brief Default MAX7219 display intensity (0-15). */
#ifndef K7SC_MAX_INTENSITY
  #define K7SC_MAX_INTENSITY     8
#endif
