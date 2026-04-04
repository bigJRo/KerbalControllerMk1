/***********************************************************************************
  KCMk1_SystemConfig.h — Master hardware and cross-panel configuration
  for Kerbal Controller Mk1 display panels.
  Include from each sketch's main .h file.
  Contains only #defines — no variables, no functions.
  Panel-specific constants remain in each sketch's AAA_Config.ino.
***********************************************************************************/
#pragma once

// --- Display hardware ---
#define KCM_SCREEN_W              800
#define KCM_SCREEN_H              480

// --- I2C ---
#define KCM_I2C_INT_PIN           2
#define KCM_I2C_ADDR_ANNUNCIATOR  0x10
#define KCM_I2C_ADDR_RESDISP      0x11
#define KCM_I2C_ADDR_INFODISP     0x12
#define KCM_I2C_SYNC_ANNUNCIATOR  0xAC
#define KCM_I2C_SYNC_RESDISP      0xAD
#define KCM_I2C_SYNC_INFODISP     0xAE   // was 0xAD — collision fix (item #3)

// --- Serial ---
#define KCM_SERIAL_BAUD           115200

// --- Touch filter ---
#define KCM_TOUCH_DEBOUNCE_MS       500
#define KCM_TOUCH_DEAD_ZONE_PX       12
#define KCM_TOUCH_JITTER_MAX_PX      20
#define KCM_TOUCH_TITLE_DEBOUNCE_MS 200  // InfoDisp title-bar shorter window

// --- Default operating modes ---
#define KCM_DEFAULT_DEBUG_MODE       true
#define KCM_DEFAULT_DEMO_MODE        true
#define KCM_DEFAULT_DISPLAY_ROTATION 0

// --- Cross-panel aligned thresholds ---
// These values must stay identical between Annunciator C&W logic and InfoDisp
// display thresholds. Edit here only — local constants in each sketch alias these.
#define KCM_GROUND_PROX_S         10.0f   // CW_GROUND_PROX_S / LNDG_TGRND_ALARM_S
#define KCM_HIGH_G_ALARM_POS       9.0f   // CW_HIGH_G_ALARM   / G_ALARM_POS
#define KCM_HIGH_G_ALARM_NEG      -5.0f   // CW_HIGH_G_WARN    / G_ALARM_NEG
#define KCM_LOW_DV_MS            150.0f   // CW_LOW_DV_MS      / DV_STG_ALARM_MS
#define KCM_LOW_BURN_S            60.0f   // CW_LOW_BURN_S     / LNCH_BURNTIME_ALARM_S
