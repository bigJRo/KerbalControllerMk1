/**
 * @file        Config.h
 * @version     1.1.0
 * @date        2026-04-09
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
#define EVA_MODULE_TYPE_ID      KMC_TYPE_EVA_MODULE

/** @brief Firmware version — major. */
#define EVA_FIRMWARE_MAJOR      1

/** @brief Firmware version — minor. */
#define EVA_FIRMWARE_MINOR      0

/**
 * @brief Capability flag — encoder delta data present in packet (bit 2).
 * @note  NOT currently reported in the identity response. Encoders are
 *        present on the PCB (H1, H2) but not yet wired or implemented.
 *        Restore this flag in I2C.cpp _sendIdentityPacket() once encoder
 *        ISRs are active and bytes 2-3 of the data packet carry live data.
 */
#define EVA_CAP_ENCODERS        KMC_CAP_ENCODERS

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
//  Packet sizes
// ============================================================

/**
 * @brief EVA module data packet size (bytes).
 *        Byte 0:  Button state  (bits 0-5 = buttons 0-5, bits 6-7 unused)
 *        Byte 1:  Change mask   (bits 0-5 = buttons 0-5, bits 6-7 unused)
 *        Byte 2:  ENC1 delta    (signed int8, clicks since last read)
 *        Byte 3:  ENC2 delta    (signed int8, clicks since last read)
 */
#define EVA_PACKET_SIZE         4

/** @brief Identity response packet size (bytes). */
#define EVA_IDENTITY_SIZE       KMC_IDENTITY_SIZE

/** @brief LED state payload size (bytes, nibble-packed). */
#define EVA_LED_PAYLOAD_SIZE    8

// ============================================================
//  LED state nibble values
//
//  Matches the Kerbal Controller Mk1 I2C protocol definition.
// ============================================================

// ============================================================
//  LED state nibble values — aliases for KMC_LED_*
// ============================================================

#define LED_OFF                 KMC_LED_OFF
#define LED_ENABLED             KMC_LED_ENABLED
#define LED_ACTIVE              KMC_LED_ACTIVE
#define LED_WARNING             KMC_LED_WARNING
#define LED_ALERT               KMC_LED_ALERT
#define LED_ARMED               KMC_LED_ARMED
#define LED_PARTIAL_DEPLOY      KMC_LED_PARTIAL_DEPLOY

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
//    2  BUTTON03  PA7 (pin  8)  ← was incorrectly PIN_PB5
//    3  BUTTON04  PB5 (pin  9)  ← was incorrectly PIN_PB4 (NC)
//    4  BUTTON05  PA5 (pin  6)
//    5  BUTTON06  PA6 (pin  7)
//
//  Note: PB4 (pin 10) is not connected on the KC-01-1852 PCB.
// ============================================================

#define EVA_BTN_PIN_0           PIN_PB3   // BUTTON01 — EVA Lights
#define EVA_BTN_PIN_1           PIN_PB2   // BUTTON02 — Jetpack Enable
#define EVA_BTN_PIN_2           PIN_PA7   // BUTTON03 — Board Craft
#define EVA_BTN_PIN_3           PIN_PB5   // BUTTON04 — EVA Construction
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
//
//  IMPORTANT: This module uses PIN_PA4 for INT, not PIN_PA1
//  as on all KC-01-1822-based standard modules.
//
//  Reason: On the KC-01-1852 PCB, PA1 (pin 20) is routed to
//  the ENC1_SW net on encoder header H1 (see EVA_ENC1_PIN_SW
//  below). PA4 (pin 5) is therefore used for the INT output
//  and is the pin connected to the INT net on the Panel
//  Control Connector (P1).
//
//  The master controller firmware must wire this module's
//  interrupt input to the PA4 net on the KC-01-1852, not
//  the PA1 net used by all other modules.
//
//  Schematic reference: KC-01-1852 v1.0, sheet 1, U7 pin 5.
// ============================================================

/** @brief Interrupt output — PA4, active low.
 *  @note  Uses PA4, not PA1. See comment block above. */
#define EVA_INT_PIN             PIN_PA4

// ============================================================
//  Color palette
//
//  RGBColor struct, system palette, scaleColor(), and nibble
//  pack helpers are provided by KerbalModuleCommon.
//
//  EVA module uses a green family palette to distinguish it
//  from cockpit modules. The two EVA-specific colors (LIME and
//  SEAFOAM) that are not in the system palette are defined here.
//  All others reference KMC_* canonical names directly.
// ============================================================

#include <KerbalModuleCommon.h>

// --- Utility ---
static const RGBColor EVA_OFF    = KMC_OFF;
static const RGBColor EVA_WHITE  = KMC_WHITE_COOL;

// --- EVA-specific colors not in the system palette ---

/** @brief Lime. Bright yellow-green. Jetpack Enable.
 *         Distinct from KMC_LIME — more saturated, no brown tone. */
static const RGBColor EVA_LIME     = { 180, 255,   0 };

/** @brief Seafoam. Mid warm green. Grab.
 *         Not in system palette — EVA-specific. */
static const RGBColor EVA_SEAFOAM  = {  80, 200, 160 };

// ============================================================
//  Per-button active colors
//
//  Indexed by button index (0-5).
//  EVA-specific colors use EVA_* names above.
//  System palette colors use KMC_* names directly.
// ============================================================

static const RGBColor EVA_ACTIVE_COLORS[EVA_BUTTON_COUNT] = {
    KMC_MINT,        // B0 — EVA Lights       (Col 1, Row 1)
    EVA_LIME,        // B1 — Jetpack Enable   (Col 1, Row 2) — EVA-specific
    KMC_GREEN,       // B2 — Board Craft      (Col 2, Row 1)
    KMC_TEAL,        // B3 — EVA Construction (Col 2, Row 2)
    KMC_CHARTREUSE,  // B4 — Jump / Let Go    (Col 3, Row 1)
    EVA_SEAFOAM,     // B5 — Grab             (Col 3, Row 2) — EVA-specific
};

// ============================================================
//  evaScaleColor() — backward-compatible alias
// ============================================================

/** @brief Scale an RGBColor by brightness. Alias for scaleColor(). */
inline RGBColor evaScaleColor(RGBColor c, uint8_t brightness) {
    return scaleColor(c, brightness);
}

// ============================================================
//  ledPackGet / ledPackSet — backward-compatible aliases
// ============================================================

/** @brief Get LED nibble from payload. Alias for kmcLedPackGet(). */
inline uint8_t ledPackGet(const uint8_t* payload, uint8_t btn) {
    return kmcLedPackGet(payload, btn);
}

/** @brief Set LED nibble in payload. Alias for kmcLedPackSet(). */
inline void ledPackSet(uint8_t* payload, uint8_t btn, uint8_t state) {
    kmcLedPackSet(payload, btn, state);
}
