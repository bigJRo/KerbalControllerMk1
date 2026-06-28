/**
 * @file        TesterUI.h
 * @version     2.0.0
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Touchscreen UI layer (LovyanGFX + FT6236) for the Module
 *              Tester. Pure rendering + touch hit-testing; all protocol
 *              logic lives in TesterHW. Screens: splash, scan/select, and
 *              the per-module test dashboard.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */

#pragma once
#include <Arduino.h>
#include "ModuleCatalog.h"
#include "TesterHW.h"

// ============================================================
//  Dashboard control actions (returned by uiDashboardTouch)
// ============================================================
enum UIAction : uint8_t {
    UI_NONE = 0,
    UI_ENABLE,
    UI_DISABLE,
    UI_SLEEP,
    UI_WAKE,
    UI_RESET,
    UI_BULB,
    UI_LEDCYCLE,
    UI_TEST,        // enter the per-board construction test
    UI_BACK
};

// ============================================================
//  Construction-test screen controls
// ============================================================
enum CtButton : uint8_t {
    CT_NONE = 0,
    CT_PASS,
    CT_FAIL,
    CT_NEXT,
    CT_RETRY,
    CT_ABORT
};

// Button-visibility mask for uiCtRender / uiCtPoll
#define CTB_PASS    0x01
#define CTB_FAIL    0x02
#define CTB_NEXT    0x04
#define CTB_RETRY   0x08
#define CTB_ABORT   0x10

// ------------------------------------------------------------
//  Lifecycle
// ------------------------------------------------------------

/** @brief Initialise display (ILI9341), touch (FT6236), backlight, rotation. */
void uiBegin();

/** @brief Draw the boot splash screen. */
void uiSplash();

// ------------------------------------------------------------
//  Scan / select screen
// ------------------------------------------------------------

/** @brief Draw the static scan-screen chrome (title, Rescan button). */
void uiScanBegin();

/**
 * @brief  Draw/refresh the list of detected modules.
 * @param  infos    catalog entries (nullptr entry = unknown type)
 * @param  addrs    matching I2C addresses
 * @param  typeIds  matching identity type IDs (for unknown entries)
 * @param  count    number of entries
 */
void uiScanList(const ModuleInfo* const* infos, const uint8_t* addrs,
                const uint8_t* typeIds, uint8_t count);

/**
 * @brief  Poll touch on the scan screen.
 * @return selected list index [0..count-1], -1 for none, -2 for Rescan.
 */
int uiScanTouch(uint8_t count);

// ------------------------------------------------------------
//  Dashboard screen
// ------------------------------------------------------------

/** @brief Draw the static dashboard chrome for a selected module. */
void uiDashboardBegin(const ModuleInfo* info, uint8_t addr);

/** @brief Refresh the dynamic header (lifecycle / fault / tx counter). */
void uiDashboardHeader(const ModuleState& st);

/** @brief Refresh the live input area (buttons / axes / encoders / value). */
void uiDashboardInputs(const ModuleInfo* info, const ModuleState& st);

/** @brief Poll the dashboard control buttons. */
UIAction uiDashboardTouch();

// ------------------------------------------------------------
//  Shared widgets
// ------------------------------------------------------------

// ------------------------------------------------------------
//  Construction-test screen (generic, driven by ConstructionTest)
// ------------------------------------------------------------

/**
 * @brief  Render one construction-test step.
 * @param  header       top line (e.g. "CONSTRUCTION TEST — Vehicle Ctrl  2/5")
 * @param  instruction  what the operator should do this step
 * @param  status       array of live status lines (may be nullptr if 0)
 * @param  statusCount  number of status lines
 * @param  btnMask      OR of CTB_* selecting which control buttons to show
 */
void uiCtRender(const char* header, const char* instruction,
                const char* const* status, uint8_t statusCount, uint8_t btnMask);

/** @brief Poll the construction-test control buttons (press-edge). */
CtButton uiCtPoll(uint8_t btnMask);

// ------------------------------------------------------------
//  Shared widgets
// ------------------------------------------------------------

/** @brief Draw the power readout (voltage / current / watts) in the top bar. */
void uiPowerBar(const PowerReading& p);

/** @brief Briefly show a status message (e.g. command sent / fault). */
void uiToast(const char* msg);
