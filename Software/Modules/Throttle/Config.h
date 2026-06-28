/**
 * @file        Config.h
 * @version     2.0
 * @date        2026-06-28
 * @project     Kerbal Controller Mk1 — Throttle Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Pin definitions, constants, and tunable thresholds
 *              for the Throttle Module standalone sketch.
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#pragma once
#include <stdint.h>
#include <limits.h>
#include <KerbalModuleCommon.h>

// ============================================================
//  Module identity
// ============================================================

#define THR_I2C_ADDRESS         0x2C
#define THR_MODULE_TYPE_ID      KMC_TYPE_THROTTLE
#define THR_FIRMWARE_MAJOR      2
#define THR_FIRMWARE_MINOR      0

/** @brief Capability flag — motorized position control. */
#define THR_CAP_MOTORIZED       KMC_CAP_MOTORIZED

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
#define CMD_SET_THROTTLE        KMC_CMD_SET_THROTTLE
#define CMD_SET_PRECISION       KMC_CMD_SET_PRECISION

// ============================================================
//  Packet
// ============================================================

/** @brief Universal data-packet header size (status, type ID, tx counter). */
#define THR_HEADER_SIZE         KMC_HEADER_SIZE

/**
 * @brief Data packet size (module → controller), 7 bytes (spec §9.4):
 *   Byte 0:   Status byte   (lifecycle bits 1:0, fault bit 2, data-changed bit 3)
 *   Byte 1:   Module Type ID
 *   Byte 2:   Transaction counter
 *   Byte 3:   Status flags  (bit0=enabled, bit1=precision,
 *                            bit2=pilot touching, bit3=motor moving)
 *   Byte 4:   Button events (bit0=THRTL_100, bit1=THRTL_UP,
 *                            bit2=THRTL_DOWN, bit3=THRTL_00)
 *   Byte 5-6: Throttle value (uint16, big-endian, 0 to INT16_MAX)
 */
#define THR_PACKET_SIZE         7
#define THR_IDENTITY_SIZE       KMC_IDENTITY_SIZE

// ============================================================
//  Status flag bits
// ============================================================

#define THR_FLAG_ENABLED        (1 << 0)
#define THR_FLAG_PRECISION      (1 << 1)
#define THR_FLAG_TOUCH          (1 << 2)
#define THR_FLAG_MOTOR_MOVING   (1 << 3)

// ============================================================
//  Button bit assignments
// ============================================================

#define THR_BIT_100             0
#define THR_BIT_UP              1
#define THR_BIT_DOWN            2
#define THR_BIT_00              3

// ============================================================
//  Pin assignments — ATtiny816 KC-01-1861/1862 v1.1
// ============================================================

/** @brief Motor forward direction — binary output to L293D INPUT1. */
#define THR_PIN_MTR_FWD         PIN_PB4

/** @brief Motor reverse direction — binary output to L293D INPUT2. */
#define THR_PIN_MTR_REV         PIN_PB5

/** @brief Motor speed — PWM output to L293D ENABLE. */
#define THR_PIN_SPEED           PIN_PB3

/** @brief Throttle 100% button — active high. */
#define THR_PIN_THRTL_100       PIN_PA6

/** @brief Throttle 100% LED — discrete output via 2N3904. */
#define THR_PIN_LED_100         PIN_PA7

/** @brief Throttle UP button — active high. */
#define THR_PIN_THRTL_UP        PIN_PB2

/** @brief Throttle UP LED — discrete output via 2N3904. */
#define THR_PIN_LED_UP          PIN_PC0

/** @brief Throttle DOWN button — active high. */
#define THR_PIN_THRTL_DOWN      PIN_PC1

/** @brief Throttle DOWN LED — discrete output via 2N3904. */
#define THR_PIN_LED_DOWN        PIN_PC2

/** @brief Throttle 0% button — active high. */
#define THR_PIN_THRTL_00        PIN_PC3

/** @brief Throttle 0% LED — discrete output via 2N3904. */
#define THR_PIN_LED_00          PIN_PA1

/** @brief Capacitive touch sensor output — active high when touched. */
#define THR_PIN_TOUCH_IND       PIN_PA3

/** @brief Potentiometer wiper — analog input. */
#define THR_PIN_WIPER           PIN_PA2

/** @brief Interrupt output — active low. */
#define THR_PIN_INT             PIN_PA5

/** @brief I2C SCL. */
#define THR_PIN_SCL             PIN_PB0

/** @brief I2C SDA. */
#define THR_PIN_SDA             PIN_PB1

// ============================================================
//  ADC / position constants
// ============================================================

/** @brief ADC reading at full closed (0% throttle). */
#define THR_ADC_MIN             0

/** @brief ADC reading at full open (100% throttle). */
#define THR_ADC_MAX             1023

/** @brief Physical center for precision mode anchor. */
#define THR_ADC_CENTER          512

/**
 * @brief Tight arrival deadzone for 0% and 100% targets (ADC counts).
 *        Motor stops when within this range of THRTL_00 or THRTL_100.
 *        Small to ensure true 0 and true 100 are reached.
 */
#define THR_DEADZONE_HARD       8

/**
 * @brief Relaxed arrival deadzone for intermediate targets (ADC counts).
 *        Motor stops when within this range of any non-limit target.
 */
#define THR_DEADZONE_SOFT       20

/**
 * @brief Slowdown zone — motor reduces speed when within this many
 *        ADC counts of the target.
 */
#define THR_SLOWDOWN_ZONE       50

/**
 * @brief Step size for THRTL_UP / THRTL_DOWN buttons (ADC counts).
 *        5% of ADC range = 51 counts.
 */
#define THR_STEP_ADC            51

/**
 * @brief Precision mode output range as fraction of INT16_MAX.
 *        10% = ±1638 counts around anchor.
 */
#define THR_PRECISION_RANGE     (INT16_MAX / 10)

// ============================================================
//  Motor PWM speeds (0-255)
// ============================================================

/** @brief Full drive speed — used when outside slowdown zone. */
#define THR_MOTOR_SPEED_FULL    200

/** @brief Slow drive speed — used within slowdown zone. */
#define THR_MOTOR_SPEED_SLOW    80

// ============================================================
//  Timing
// ============================================================

/** @brief Button and wiper poll interval in milliseconds. */
#define THR_POLL_INTERVAL_MS    5

/** @brief Debounce count for button inputs. */
#define THR_DEBOUNCE_COUNT      4

/**
 * @brief Minimum wiper change (ADC counts) to report a new
 *        throttle value. Prevents noise from generating constant
 *        INT assertions when slider is held still.
 */
#define THR_WIPER_CHANGE_MIN    4

// ============================================================
//  Bulb test duration
// ============================================================

#define THR_BULB_TEST_MS        2000
