/**
 * @file        KerbalModuleCommon.h
 * @version     1.4.0
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Shared color types, system palette, and LED utilities
 *              for all Kerbal Controller Mk1 target modules.
 *
 *              Single source of truth for the KMC_* color palette.
 *              All module libraries and sketches include this file.
 *
 *              All hardware on this system uses NEO_GRB 3-byte pixels.
 *              Colors are stored as plain RGBColor and passed directly
 *              to setPixelColor(i, r, g, b) — no format conversion needed.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include <stdint.h>

// ============================================================
//  Color struct
// ============================================================

struct RGBColor {
    uint8_t r;  ///< Red   (0-255)
    uint8_t g;  ///< Green (0-255)
    uint8_t b;  ///< Blue  (0-255)
};

// ============================================================
//  System palette — canonical RGB values
//
//  Hardware-validated on SK6812MINI-EA (KC-01-1880 v2.0, NEO_GRB).
//
//  Semantic colors carry fixed meaning across ALL modules:
//    KMC_GREEN    : active / go / nominal state
//    KMC_RED      : irreversible action (cut, release, jettison)
//    KMC_AMBER    : caution / awareness / WARNING flash state
//    KMC_BACKLIT  : button powered and available — ENABLED backlight
// ============================================================

// --- Utility ---
/** @brief Fully unlit. */
static const RGBColor KMC_OFF           = {   0,   0,   0 };

/** @brief Discrete indicator ON. Non-zero but no implied colour.
 *         Do not use for NeoPixel positions. */
static const RGBColor KMC_DISCRETE_ON   = {   1,   1,   1 };

/** @brief Full white. All channels maximum.
 *         On SK6812MINI-EA reads green-tinged due to LED efficiency.
 *         Use for bulb test and diagnostics only — not for UI colours. */
static const RGBColor KMC_WHITE         = { 255, 255, 255 };

// --- Semantic — fixed meaning across all modules ---
/** @brief Active / go / nominal. */
static const RGBColor KMC_GREEN         = {   0, 255,   0 };

/** @brief Irreversible action. Cut, release, jettison. */
static const RGBColor KMC_RED           = { 255,   0,   0 };

/** @brief Caution / awareness. WARNING flash state. */
static const RGBColor KMC_AMBER         = { 255,  80,   0 };

/** @brief ENABLED backlight — button is powered and its function
 *         is available. Dim warm white, visible in normal lighting,
 *         not jarring in a dark cockpit. */
static const RGBColor KMC_BACKLIT       = {  15,   8,   2 };

// --- Whites ---
/** @brief Cool white. */
static const RGBColor KMC_WHITE_COOL    = { 255, 180, 255 };

/** @brief Neutral white. Slight warmth. */
static const RGBColor KMC_WHITE_NEUTRAL = { 255, 160, 180 };

/** @brief Warm white. Amber-tinted incandescent feel. */
static const RGBColor KMC_WHITE_WARM    = { 255, 140, 100 };

/** @brief Soft white. Warm dim amber — night vision safe. */
static const RGBColor KMC_WHITE_SOFT    = { 120,  70,  20 };

// --- Warm family ---
/** @brief Orange. Engine Alt Mode, Radiator. */
static const RGBColor KMC_ORANGE        = { 255,  60,   0 };

/** @brief Yellow. Warp targets, Lights. */
static const RGBColor KMC_YELLOW        = { 234, 179,   8 };

/** @brief Gold. Solar Array, power generation. */
static const RGBColor KMC_GOLD          = { 255, 160,   0 };

// --- Green family ---
/** @brief Chartreuse. Physics warp. */
static const RGBColor KMC_CHARTREUSE    = { 163, 230,  53 };

/** @brief Lime. Terrain, Quicksave. */
static const RGBColor KMC_LIME          = { 132, 204,  22 };

/** @brief Mint. RCS, Quickload. */
static const RGBColor KMC_MINT          = { 100, 255, 160 };

// --- Blue / cyan family ---
/** @brief Blue. Science data. */
static const RGBColor KMC_BLUE          = {   0,   0, 255 };

/** @brief Sky blue. Navigation, map. */
static const RGBColor KMC_SKY           = {  14, 165, 233 };

/** @brief Teal. Ship navigation, atmosphere. */
static const RGBColor KMC_TEAL          = {  20, 184, 166 };

/** @brief Cyan. Atmosphere, ARMED state. */
static const RGBColor KMC_CYAN          = {   6, 182, 212 };

// --- Purple family ---
/** @brief Purple. Science discovery. */
static const RGBColor KMC_PURPLE        = { 120,   0, 220 };

/** @brief Indigo. Science group. */
static const RGBColor KMC_INDIGO        = {  60,   0, 200 };

/** @brief Violet. Mechanical access, Cargo Door. */
static const RGBColor KMC_VIOLET        = { 160,   0, 255 };

// --- Pink / red family ---
/** @brief Coral. Mode shift, IVA. */
static const RGBColor KMC_CORAL         = { 255, 100,  54 };

/** @brief Rose. Vessel configuration, Control Points. */
static const RGBColor KMC_ROSE          = { 255,  80, 120 };

/** @brief Pink. Communications, Antenna. */
static const RGBColor KMC_PINK          = { 236,  72, 153 };

/** @brief Magenta. Debug / development. */
static const RGBColor KMC_MAGENTA       = { 255,   0, 255 };

// ============================================================
//  LED state nibble values
// ============================================================

#define KMC_LED_OFF             0x0
#define KMC_LED_ENABLED         0x1
#define KMC_LED_ACTIVE          0x2   // full per-button active color
#define KMC_LED_WARNING         0x3   // flashing amber (500ms)
#define KMC_LED_ALERT           0x4   // flashing red (150ms)
#define KMC_LED_ARMED           0x5   // static cyan
#define KMC_LED_PARTIAL_DEPLOY  0x6   // static amber
#define KMC_LED_CUT             0x7   // static red — state-machine terminal
                                      // (parachute cut / heat-shield release)

// ============================================================
//  Module Type IDs
// ============================================================

#define KMC_TYPE_UI_CONTROL         0x01
#define KMC_TYPE_FUNCTION_CONTROL   0x02
#define KMC_TYPE_ACTION_CONTROL     0x03
#define KMC_TYPE_STABILITY_CONTROL  0x04
#define KMC_TYPE_VEHICLE_CONTROL    0x05
#define KMC_TYPE_TIME_CONTROL       0x06
#define KMC_TYPE_EVA_MODULE         0x07
#define KMC_TYPE_JOYSTICK_ROTATION  0x09
#define KMC_TYPE_JOYSTICK_TRANS     0x0A
#define KMC_TYPE_GPWS_INPUT         0x0B
#define KMC_TYPE_PRE_WARP_TIME      0x0C
#define KMC_TYPE_THROTTLE           0x0D
#define KMC_TYPE_DUAL_ENCODER       0x0E
#define KMC_TYPE_SWITCH_PANEL       0x0F
#define KMC_TYPE_INDICATOR          0x10

// ============================================================
//  Capability flags
// ============================================================

#define KMC_CAP_EXTENDED_STATES  0x01
#define KMC_CAP_FAULT            0x02
#define KMC_CAP_ENCODERS         0x04
#define KMC_CAP_JOYSTICK         0x08
#define KMC_CAP_DISPLAY          0x10
#define KMC_CAP_MOTORIZED        0x20

// ============================================================
//  Packet header — status byte (byte 0 of every data packet)
// ============================================================

// Lifecycle state — bits 1:0
#define KMC_STATUS_ACTIVE       0x00  ///< Normal operation
#define KMC_STATUS_SLEEPING     0x01  ///< Suspended via CMD_SLEEP
#define KMC_STATUS_DISABLED     0x02  ///< No valid game context
#define KMC_STATUS_BOOT_READY   0x03  ///< Just powered on, awaiting master init

#define KMC_STATUS_LIFECYCLE_MASK  0x03  ///< Mask for lifecycle bits

// Status flags — individual bits
#define KMC_STATUS_FAULT        0x04  ///< Hardware fault (bit 2)
#define KMC_STATUS_DATA_CHANGED 0x08  ///< State changed since last read (bit 3)

// ============================================================
//  I2C Commands
// ============================================================

#define KMC_CMD_GET_IDENTITY     0x01
#define KMC_CMD_SET_LED_STATE    0x02
#define KMC_CMD_SET_BRIGHTNESS   0x03
#define KMC_CMD_BULB_TEST        0x04
#define KMC_CMD_SLEEP            0x05
#define KMC_CMD_WAKE             0x06
#define KMC_CMD_RESET            0x07
#define KMC_CMD_ACK_FAULT        0x08
#define KMC_CMD_ENABLE           0x09
#define KMC_CMD_DISABLE          0x0A
#define KMC_CMD_SET_VALUE        0x0D

// ============================================================
//  Canonical packet sizes
//
//  Single source of truth for the fixed-size protocol fields shared
//  by every module library. Device-specific data payload sizes are
//  defined per library; these are the universal constants.
// ============================================================

/** @brief Universal data-packet header: status byte, type ID, tx counter. */
#define KMC_HEADER_SIZE          3

/** @brief Identity response: type ID, fw major, fw minor, capability flags. */
#define KMC_IDENTITY_SIZE        4

/** @brief CMD_SET_LED_STATE payload: 16 positions × 4-bit nibble = 8 bytes. */
#define KMC_LED_PAYLOAD_SIZE     8

// --- Total data-packet sizes (header + device payload) ---
// Single source of truth for the controller's read lengths and each module
// library's response buffer. Total = KMC_HEADER_SIZE + payload (spec §9).

/** @brief Standard 16-input button module: 4-byte payload (events HI/LO,
 *         change HI/LO). UI/Action/Stability/Time/EVA/Dual Encoder/Switch Panel. */
#define KMC_BUTTON_PAYLOAD_SIZE    4
#define KMC_BUTTON_PACKET_SIZE     (KMC_HEADER_SIZE + KMC_BUTTON_PAYLOAD_SIZE)   // 7

/** @brief 24-input switch-group button module: 6-byte payload (3 event +
 *         3 change bytes). Function Control (0x21), Vehicle Control (0x24). */
#define KMC_BUTTON24_PAYLOAD_SIZE  6
#define KMC_BUTTON24_PACKET_SIZE   (KMC_HEADER_SIZE + KMC_BUTTON24_PAYLOAD_SIZE) // 9

/** @brief Joystick module: 9-byte payload (events, change, state, 3× int16
 *         axes). Joystick Rotation (0x09), Joystick Translation (0x0A). */
#define KMC_JOYSTICK_PAYLOAD_SIZE  9
#define KMC_JOYSTICK_PACKET_SIZE   (KMC_HEADER_SIZE + KMC_JOYSTICK_PAYLOAD_SIZE) // 12

/** @brief Display module: 5-byte payload (events, change, state, value HI/LO).
 *         GPWS Input (0x0B), Pre-Warp Time (0x0C). */
#define KMC_DISPLAY_PAYLOAD_SIZE   5
#define KMC_DISPLAY_PACKET_SIZE    (KMC_HEADER_SIZE + KMC_DISPLAY_PAYLOAD_SIZE)  // 8

/** @brief Throttle module: 4-byte payload (flags, button events, value HI/LO). */
#define KMC_THROTTLE_PAYLOAD_SIZE  4
#define KMC_THROTTLE_PACKET_SIZE   (KMC_HEADER_SIZE + KMC_THROTTLE_PAYLOAD_SIZE) // 7

// ============================================================
//  LED nibble pack / unpack helpers
// ============================================================

inline uint8_t kmcLedPackGet(const uint8_t* payload, uint8_t button) {
    return (payload[button / 2] >> ((button % 2) ? 4 : 0)) & 0x0F;
}

inline void kmcLedPackSet(uint8_t* payload, uint8_t button, uint8_t state) {
    uint8_t shift = (button % 2) ? 4 : 0;
    payload[button / 2] = (payload[button / 2] & ~(0x0F << shift)) | ((state & 0x0F) << shift);
}
