/**
 * @file        K7SC_State.h
 * @version     2.0.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Shared state structs for Kerbal7SegmentCore.
 *
 *              Two structs form the complete interface between the
 *              library and the sketch:
 *
 *              K7SCCommandState — commands from the master controller.
 *                Lifecycle fields (lifecycle, isBulbTest) are persistent:
 *                set by master, stay until master changes them.
 *                Event fields (isReset, hasNewValue, hasLEDState) are
 *                one-shot: set by library on command receipt, auto-cleared
 *                by library after one loop iteration.
 *
 *              K7SCInputState — hardware input from local devices.
 *                All fields accumulate between sketch reads.
 *                Sketch clears fields after consuming them.
 *
 * @license     GNU General Public License v3.0
 */

#pragma once
#include <stdint.h>
#include <KerbalModuleCommon.h>

// ============================================================
//  Lifecycle enum — mirrors KMC_STATUS_* packet header values
// ============================================================

enum K7SCLifecycle : uint8_t {
    K7SC_BOOT_READY = KMC_STATUS_BOOT_READY,  // 0x03 — awaiting master init
    K7SC_DISABLED   = KMC_STATUS_DISABLED,     // 0x02 — no game context
    K7SC_SLEEPING   = KMC_STATUS_SLEEPING,     // 0x01 — paused, state frozen
    K7SC_ACTIVE     = KMC_STATUS_ACTIVE        // 0x00 — normal operation
};

// ============================================================
//  K7SCCommandState — commands from master controller via I2C
//
//  Persistent fields stay set until master changes them.
//  One-shot fields are auto-cleared by library after one loop.
// ============================================================

struct K7SCCommandState {

    // ── Persistent — set by master, stays until master changes ──

    /** Current module lifecycle state. */
    K7SCLifecycle lifecycle;

    /** Bulb test active. Set by CMD_BULB_TEST 0x01, cleared by 0x00.
     *  Sketch may also set/clear directly. */
    bool isBulbTest;

    // ── One-shot — set by library, auto-cleared after one loop ──

    /** CMD_RESET received. Sketch should reset all module state. */
    bool isReset;

    /** CMD_SET_VALUE received. newValue contains the value. */
    bool hasNewValue;

    /** Signed value from CMD_SET_VALUE. */
    int16_t newValue;

    /** CMD_SET_LED_STATE received. ledState contains raw payload byte. */
    bool hasLEDState;

    /** Raw byte from CMD_SET_LED_STATE — sketch interprets. */
    uint8_t ledState;
};

// ============================================================
//  K7SCInputState — local hardware input
//
//  All fields accumulate between sketch reads.
//  Sketch is responsible for clearing fields after consuming them.
// ============================================================

struct K7SCInputState {

    // ── Button edges — bitmask, bit0=BTN01, bit1=BTN02,
    //                           bit2=BTN03, bit3=BTN_EN ──

    /** Rising edges since last sketch clear. */
    uint8_t buttonPressed;

    /** Falling edges since last sketch clear. */
    uint8_t buttonReleased;

    /** Any edge (pressed | released). */
    uint8_t buttonChanged;

    // ── Encoder ──

    /** Net clicks since last sketch clear. +CW, -CCW. */
    int16_t encoderDelta;

    /** True if any encoder movement since last sketch clear. */
    bool encoderChanged;
};

// ============================================================
//  Global instances — defined in K7SC_I2C.cpp
// ============================================================

extern K7SCCommandState cmdState;
extern K7SCInputState   inputState;
