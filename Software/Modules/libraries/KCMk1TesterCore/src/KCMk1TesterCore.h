/**
 * @file        KCMk1TesterCore.h
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Umbrella header for the per-module tester sketches.
 *
 *              A sketch fills in a KtcModuleDef (name, address, type ID,
 *              packet size, expected firmware and capability flags, and the
 *              step table) and calls:
 *
 *                  void setup() { ktcBegin(MODULE); }
 *                  void loop()  { ktcLoop(); }
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#pragma once
#include <KerbalModuleCommon.h>
#include "KTC_Config.h"
#include "KTC_Bus.h"
#include "KTC_UI.h"
#include "KTC_Runner.h"
#include "KTC_Steps.h"
#include "KTC_Kbc.h"
#include "KTC_Kjc.h"
#include "KTC_Script.h"
