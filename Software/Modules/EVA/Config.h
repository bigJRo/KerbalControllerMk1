/**
 * @file        Config.h
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — EVA Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Pin definitions, I2C constants, timing parameters,
 *              color palette, and LED state definitions for the
 *              EVA Module standalone sketch.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include <stdint.h>

// ============================================================
//  ATtiny816 clock speed assertion
// ============================================================

#if F_CPU < 7372800UL
  #error "EVA: CPU clock too slow for NeoPixel timing. Minimum 7.37 MHz required."
#endif

// ============================================================
//  I2C configuration
//
//  This module uses the Kerbal Controller Mk1 I2C protocol
//  but is NOT a KerbalButtonCore (KBC) module.
// ============================================================

/** @brief I2C address for the EVA module. */
#define EVA_I2C_ADDRESS         0x26

/** @brief Module type ID reported in identity response. */
#define EVA_MODULE_TYPE_ID      0x07

/** @brief Firmware version — major. */
#define EVA_FIRMWARE_MAJOR      1

/** @brief Firmware version — minor. */
#define EVA_FIRMWARE_MINOR      0

/** @brief Capability flags — encoders present (future use). */
#define EVA_CAP_ENCODERS        (1 << 2)

// ============================================================
//  I2C command bytes
//
//  Mirrors the Kerbal Controller Mk1 I2C protocol command set.
//  Defined independently here — no KBC library dependency.
// ============================================================

#define CMD_GET_IDENTITY        0x01
#define CMD_SET_LED_STATE       0x02
#define CMD_SET_BRIGHTNESS      0x03
#define CMD_BULB_TEST           0x04
#define CMD_SLEEP               0x05
#define CMD_WAKE                0x06
#define CMD_RESET               0x07
#define CMD_ACK_FAULT           0x08

// ============================================================
//  Packet sizes
// ============================================================

/** @brief Standard button state packet size (bytes). */
#define EVA_BUTTON_PACKET_SIZE  4

/**
 * @brief Extended packet size including encoder deltas (bytes).
 *        Byte 0-1: current button state bitmask
 *        Byte 2-3: change mask
 *        Byte 4:   ENC1 delta (signed int8, clicks since last read)
 *        Byte 5:   ENC2 delta (signed int8, clicks since last read)
 */
#define EVA_FULL_PACKET_SIZE    6

/** @brief Identity response packet size (bytes). */
#define EVA_IDENTITY_SIZE       4

/** @brief LED state payload size (bytes, nibble-packed). */
#define EVA_LED_PAYLOAD_SIZE    8

// ============================================================
//  LED state nibble values
//
//  Matches the Kerbal Controller Mk1 I2C protocol definition.
// ============================================================

#define LED_OFF                 0x0
#define LED_ENABLED             0x1
#define LED_ACTIVE              0x2
#define LED_WARNING             0x3
#define LED_ALERT               0x4
#define LED_ARMED               0x5
#define LED_PARTIAL_DEPLOY      0x6

// ============================================================
//  Button and LED counts
// ============================================================

/** @brief Number of physical buttons on this module. */
#define EVA_BUTTON_COUNT        6

/** @brief Number of NeoPixels in the chain. */
#define EVA_NEO_COUNT           6

/** @brief Number of encoder headers (future use). */
#define EVA_ENCODER_COUNT       2

// ============================================================
//  Polling and debounce
// ============================================================

/** @brief Button polling interval in milliseconds. */
#define EVA_POLL_INTERVAL_MS    5

/** @brief Consecutive matching reads to register a state change. */
#define EVA_DEBOUNCE_COUNT      4

/** @brief Button polling interval during sleep mode. */
#define EVA_SLEEP_POLL_MS       50

// ============================================================
//  NeoPixel pin and color order
//
//  PC0 is Port C — set Tools -> tinyNeoPixel Port -> Port C
//  WS2811 ICs use RGB data order (not GRB).
// ============================================================

/** @brief NeoPixel data output pin (Port C required in IDE). */
#define EVA_NEO_PIN             PIN_PC0

/** @brief NeoPixel color order — WS2811 uses RGB. */
#define EVA_NEO_COLOR_ORDER     NEO_RGB

/**
 * @brief Default ENABLED state brightness (0-255).
 *        Applied via RGB scaling — setBrightness() never called.
 */
#define EVA_ENABLED_BRIGHTNESS  32

// ============================================================
//  Button GPIO pins
//
//  All inputs are active high with hardware pull-down resistors.
//  Button indices 0-5 map directly to KCMk1 panel positions.
//
//  KBC index -> PCB label -> ATtiny816 pin
//    0  BUTTON01  PB3 (pin 11)
//    1  BUTTON02  PB2 (pin 12)
//    2  BUTTON03  PB5 (pin  9)
//    3  BUTTON04  PB4 (pin 10)
//    4  BUTTON05  PA5 (pin  6)
//    5  BUTTON06  PA6 (pin  7)
// ============================================================

#define EVA_BTN_PIN_0           PIN_PB3   // BUTTON01 — EVA Lights
#define EVA_BTN_PIN_1           PIN_PB2   // BUTTON02 — Jetpack Enable
#define EVA_BTN_PIN_2           PIN_PB5   // BUTTON03 — Board Craft
#define EVA_BTN_PIN_3           PIN_PB4   // BUTTON04 — EVA Construction
#define EVA_BTN_PIN_4           PIN_PA5   // BUTTON05 — Jump / Let Go
#define EVA_BTN_PIN_5           PIN_PA6   // BUTTON06 — Grab

/** @brief Button pin array indexed by button index. */
#define EVA_BTN_PINS            { EVA_BTN_PIN_0, EVA_BTN_PIN_1, \
                                  EVA_BTN_PIN_2, EVA_BTN_PIN_3, \
                                  EVA_BTN_PIN_4, EVA_BTN_PIN_5 }

// ============================================================
//  Encoder pins (future use)
//
//  Encoders have no external pull-ups — use INPUT_PULLUP if
//  common-ground encoders are connected.
//
//  ENC1: PA3=A, PA2=B, PA1=SW
//  ENC2: PC3=A, PC2=B, PC1=SW
// ============================================================

#define EVA_ENC1_PIN_A          PIN_PA3
#define EVA_ENC1_PIN_B          PIN_PA2
#define EVA_ENC1_PIN_SW         PIN_PA1

#define EVA_ENC2_PIN_A          PIN_PC3
#define EVA_ENC2_PIN_B          PIN_PC2
#define EVA_ENC2_PIN_SW         PIN_PC1

// ============================================================
//  INT output pin
// ============================================================

/** @brief Interrupt output pin — active low. */
#define EVA_INT_PIN             PIN_PA4

// ============================================================
//  RGBColor struct and color palette
//
//  EVA module uses a green family palette to distinguish it
//  from cockpit modules. Colors are defined independently
//  of KBC_Colors.h — no library dependency.
// ============================================================

struct RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// --- Utility ---
static const RGBColor EVA_OFF          = {   0,   0,   0 };
static const RGBColor EVA_WHITE        = { 255, 255, 255 };

// --- Green family — EVA palette ---

/** @brief Mint. Soft cool green. EVA Lights. */
static const RGBColor EVA_MINT         = { 150, 255, 200 };

/** @brief Lime. Bright yellow-green. Jetpack Enable. */
static const RGBColor EVA_LIME         = { 180, 255,   0 };

/** @brief Green. Standard go/positive. Board Craft. */
static const RGBColor EVA_GREEN        = {  34, 197,  94 };

/** @brief Chartreuse. Warm yellow-green. Jump / Let Go. */
static const RGBColor EVA_CHARTREUSE   = { 163, 230,  53 };

/** @brief Teal. Blue-green. EVA Construction. */
static const RGBColor EVA_TEAL         = {  20, 184, 166 };

/** @brief Seafoam. Mid warm green. Grab. */
static const RGBColor EVA_SEAFOAM      = {  80, 200, 160 };

// ============================================================
//  Per-button active colors
//
//  Indexed by button index (0-5).
//  Controller sends LED_ACTIVE to illuminate in this color.
// ============================================================

static const RGBColor EVA_ACTIVE_COLORS[EVA_BUTTON_COUNT] = {
    EVA_MINT,        // B0 — EVA Lights       (Col 1, Row 1)
    EVA_LIME,        // B1 — Jetpack Enable   (Col 1, Row 2)
    EVA_GREEN,       // B2 — Board Craft      (Col 2, Row 1)
    EVA_TEAL,        // B3 — EVA Construction (Col 2, Row 2)
    EVA_CHARTREUSE,  // B4 — Jump / Let Go    (Col 3, Row 1)
    EVA_SEAFOAM,     // B5 — Grab             (Col 3, Row 2)
};

// ============================================================
//  Color scaling helper
// ============================================================

inline RGBColor evaScaleColor(RGBColor c, uint8_t brightness) {
    return {
        (uint8_t)((uint16_t)c.r * brightness / 255),
        (uint8_t)((uint16_t)c.g * brightness / 255),
        (uint8_t)((uint16_t)c.b * brightness / 255)
    };
}

// ============================================================
//  Nibble pack/unpack helpers
// ============================================================

inline uint8_t ledPackGet(const uint8_t* payload, uint8_t btn) {
    return (btn % 2 == 0)
        ? (payload[btn / 2] >> 4) & 0x0F
        : (payload[btn / 2]) & 0x0F;
}

inline void ledPackSet(uint8_t* payload, uint8_t btn, uint8_t state) {
    if (btn % 2 == 0)
        payload[btn / 2] = (payload[btn / 2] & 0x0F) | ((state & 0x0F) << 4);
    else
        payload[btn / 2] = (payload[btn / 2] & 0xF0) | (state & 0x0F);
}
