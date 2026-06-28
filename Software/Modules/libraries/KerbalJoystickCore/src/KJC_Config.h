/**
 * @file        KJC_Config.h
 * @version     2.0.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Pin definitions, ADC constants, timing parameters,
 *              and threshold values for the KerbalJoystickCore library.
 *
 *              All threshold constants are expressed in raw ADC counts
 *              (0-1023) to keep calibration, deadzone, and change
 *              threshold comparisons in the same unit space.
 *
 *              Override any #ifndef-guarded constant by defining it
 *              before including KerbalJoystickCore.h in the sketch.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 *
 * @note        Hardware: KC-01-1831/1832 Joystick Module v1.0
 *              Target:   ATtiny816-MNR (megaTinyCore)
 */

#pragma once
#include <stdint.h>
#include <KerbalModuleCommon.h>

// ============================================================
//  Clock speed assertion
// ============================================================

#if F_CPU < 7372800UL
  #error "KJC: CPU clock too slow for NeoPixel timing. Minimum 7.37 MHz required."
#endif

// ============================================================
//  Firmware version
// ============================================================

#define KJC_FIRMWARE_MAJOR      2
#define KJC_FIRMWARE_MINOR      0

// ============================================================
//  Button and LED counts
// ============================================================

/** @brief Total number of button inputs (BTN_JOY + BTN01 + BTN02). */
#define KJC_BUTTON_COUNT        3

/** @brief Number of NeoPixel RGB buttons in the chain. */
#define KJC_NEO_COUNT           2

/** @brief Number of analog axes. */
#define KJC_AXIS_COUNT          3

// ============================================================
//  Button bit assignments
//
//  Used in the button state and change mask bytes of the
//  joystick data packet.
// ============================================================

#define KJC_BIT_BTN_JOY         0   // bit 0 — joystick pushbutton
#define KJC_BIT_BTN01           1   // bit 1 — NeoPixel button 1
#define KJC_BIT_BTN02           2   // bit 2 — NeoPixel button 2

// ============================================================
//  Axis indices
// ============================================================

#define KJC_AXIS1               0
#define KJC_AXIS2               1
#define KJC_AXIS3               2

// ============================================================
//  Pin assignments — ATtiny816 KC-01-1831/1832 v1.0
// ============================================================

/** @brief AXIS1 analog input — PA5, ADC channel AIN5. */
#define KJC_PIN_AXIS1           PIN_PA5

/** @brief AXIS2 analog input — PA6, ADC channel AIN6. */
#define KJC_PIN_AXIS2           PIN_PA6

/** @brief AXIS3 analog input — PA7, ADC channel AIN7. */
#define KJC_PIN_AXIS3           PIN_PA7

/** @brief Joystick pushbutton — PB5, active high, hardware pull-down. */
#define KJC_PIN_BTN_JOY         PIN_PB5

/** @brief BUTTON01 NeoPixel button — PC0. */
#define KJC_PIN_BTN01           PIN_PC0

/** @brief BUTTON02 NeoPixel button — PB3. */
#define KJC_PIN_BTN02           PIN_PB3

/** @brief NeoPixel data output — PC1 (Port C). */
#define KJC_PIN_NEOPIX          PIN_PC1

/** @brief Interrupt output — PA1, active low. */
#define KJC_PIN_INT             PIN_PA1

/** @brief I2C SCL — PB0. */
#define KJC_PIN_SCL             PIN_PB0

/** @brief I2C SDA — PB1. */
#define KJC_PIN_SDA             PIN_PB1

// ============================================================
//  NeoPixel configuration
// ============================================================

/** @brief WS2811 uses RGB color order. */
#define KJC_NEO_COLOR_ORDER     NEO_RGB

// ============================================================
//  ADC thresholds — all in raw ADC counts (0-1023)
//
//  Keeping deadzone and change threshold in the same unit space
//  simplifies the processing pipeline and makes tuning intuitive.
//  All threshold comparisons happen before map() is called.
// ============================================================

/**
 * @brief Deadzone radius around calibrated center in ADC counts.
 *        Axis values within this range of center are treated as
 *        zero and suppressed. Prevents center drift noise from
 *        generating I2C traffic when the joystick is at rest.
 *        Default: 32 counts (~3.1% of full range).
 */
#ifndef KJC_DEADZONE
  #define KJC_DEADZONE          32
#endif

/**
 * @brief Minimum change in ADC counts required to trigger an INT
 *        assertion for axis data. Compared against raw ADC value
 *        vs last sent raw ADC value. Suppresses noise on a held
 *        joystick position outside the deadzone.
 *        Default: 8 counts (~0.8% of full range).
 */
#ifndef KJC_CHANGE_THRESHOLD
  #define KJC_CHANGE_THRESHOLD  8
#endif

/**
 * @brief Minimum time in milliseconds between INT assertions
 *        caused by axis data changes. Does not apply to button
 *        changes which always assert INT immediately.
 *        Default: 10ms (maximum 100 axis reads/second).
 */
#ifndef KJC_QUIET_PERIOD_MS
  #define KJC_QUIET_PERIOD_MS   10
#endif

/**
 * @brief Number of ADC samples averaged during startup calibration.
 *        Calibration reads the joystick center position at power-on.
 *        Do not touch the joystick during the first ~80ms after boot.
 *        Default: 16 samples.
 */
#ifndef KJC_CALIBRATION_SAMPLES
  #define KJC_CALIBRATION_SAMPLES 16
#endif

// ============================================================
//  Per-axis invert flags
//
//  Set to -1 to invert a physical axis, 1 for normal polarity.
//  Applied as a sign flip after the split map, before the value
//  is stored and transmitted. Inversion does not affect the
//  deadzone or change threshold comparisons — those always
//  operate on the raw ADC value relative to calibrated center.
//
//  Override in the sketch before #include <KerbalJoystickCore.h>:
//    #define KJC_AXIS2_INVERT  -1   // invert Y axis
//
//  Default: all axes normal (1).
//
//  Translation module uses AXIS2 inverted — joystick forward
//  (stick pushed away) maps to Translate Up in KSP. Without
//  inversion, forward = Translate Down, which is unintuitive.
// ============================================================

#ifndef KJC_AXIS1_INVERT
  #define KJC_AXIS1_INVERT      1
#endif

#ifndef KJC_AXIS2_INVERT
  #define KJC_AXIS2_INVERT      1
#endif

#ifndef KJC_AXIS3_INVERT
  #define KJC_AXIS3_INVERT      1
#endif

/**
 * @brief ADC and button poll interval in milliseconds.
 *        Default: 5ms (200 Hz polling rate).
 */
#ifndef KJC_POLL_INTERVAL_MS
  #define KJC_POLL_INTERVAL_MS  5
#endif

/**
 * @brief Poll interval while in sleep mode.
 *        Default: 50ms.
 */
#ifndef KJC_SLEEP_POLL_MS
  #define KJC_SLEEP_POLL_MS     50
#endif

// ============================================================
//  LED configuration
// ============================================================

/**
 * @brief Default ENABLED state brightness (0-255).
 *        Applied via RGB scaling — setBrightness() not used.
 */
#ifndef KJC_ENABLED_BRIGHTNESS
  #define KJC_ENABLED_BRIGHTNESS  32
#endif

// ============================================================
//  Packet size
// ============================================================

/** @brief Universal data-packet header size (status, type ID, tx counter). */
#define KJC_HEADER_SIZE         KMC_HEADER_SIZE

/**
 * @brief Joystick data packet size in bytes (protocol spec §9.2).
 *        Byte 0:    Status byte   (lifecycle/fault/data-changed)
 *        Byte 1:    Module Type ID
 *        Byte 2:    Transaction counter
 *        Byte 3:    Button events (rising edges; bit0=BTN_JOY, bit1=BTN01, bit2=BTN02)
 *        Byte 4:    Change mask   (same bit layout)
 *        Byte 5:    Button state  (persistent; same bit layout)
 *        Byte 6-7:  AXIS1/PA5     (int16, signed, big-endian)
 *        Byte 8-9:  AXIS2/PA6     (int16, signed, big-endian)
 *        Byte 10-11:AXIS3/PA7     (int16, signed, big-endian)
 */
#define KJC_PACKET_SIZE         12

/** @brief Identity response packet size. */
#define KJC_IDENTITY_SIZE       KMC_IDENTITY_SIZE

/** @brief LED state payload size (nibble-packed). */
#define KJC_LED_PAYLOAD_SIZE    8

// ============================================================
//  I2C command bytes — aliases for KMC_CMD_* from KerbalModuleCommon
// ============================================================

#define KJC_CMD_GET_IDENTITY   KMC_CMD_GET_IDENTITY
#define KJC_CMD_SET_LED_STATE  KMC_CMD_SET_LED_STATE
#define KJC_CMD_SET_BRIGHTNESS KMC_CMD_SET_BRIGHTNESS
#define KJC_CMD_BULB_TEST      KMC_CMD_BULB_TEST
#define KJC_CMD_SLEEP          KMC_CMD_SLEEP
#define KJC_CMD_WAKE           KMC_CMD_WAKE
#define KJC_CMD_RESET          KMC_CMD_RESET
#define KJC_CMD_ACK_FAULT      KMC_CMD_ACK_FAULT
#define KJC_CMD_ENABLE         KMC_CMD_ENABLE
#define KJC_CMD_DISABLE        KMC_CMD_DISABLE

// ============================================================
//  LED state nibble values — aliases for KMC_LED_* from KerbalModuleCommon
// ============================================================

#define KJC_LED_OFF             KMC_LED_OFF
#define KJC_LED_ENABLED         KMC_LED_ENABLED
#define KJC_LED_ACTIVE          KMC_LED_ACTIVE
#define KJC_LED_WARNING         KMC_LED_WARNING
#define KJC_LED_ALERT           KMC_LED_ALERT
#define KJC_LED_ARMED           KMC_LED_ARMED
#define KJC_LED_PARTIAL_DEPLOY  KMC_LED_PARTIAL_DEPLOY

// ============================================================
//  Capability flags
// ============================================================

/** @brief Joystick module capability flag — analog axes present. */
#define KJC_CAP_JOYSTICK        KMC_CAP_JOYSTICK

// ============================================================
//  Sentinel value for axis suppression
// ============================================================

/**
 * @brief Sentinel returned by axis processing when the value
 *        has not changed enough to report. INT16_MIN is used
 *        because it cannot be a valid mapped joystick output
 *        (valid range is -32768 to +32767 but split map from
 *        a non-center ADC reading will never produce exactly
 *        INT16_MIN except at hardware rail).
 *
 *        Callers must check against KJC_AXIS_SUPPRESS before
 *        treating the return value as valid axis data.
 */
#define KJC_AXIS_SUPPRESS       INT16_MIN
