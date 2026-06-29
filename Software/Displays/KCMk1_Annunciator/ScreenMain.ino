/***************************************************************************************
   ScreenMain.ino -- Main screen for Kerbal Controller Mk1 Annunciator
   HARDWARE REV 2: RA8876 / Teensy 4.1, 1024x600 relayout.

   LAYOUT (1024x600) — all sizes from the approved mockup:
   ┌─────────────┬───────────────────────────────────────────┬────┬────┐
   │ MASTER ALARM│  C&W GRID 5x5 (120x80 each) @ (274,0)      │REG │SIT │  top
   │ 274x176     │                                            │75  │75  │  zone
   │ SOI 274x48  │                                            │col │col │  y0..400
   │ GLOBE 274x176                                            │    │    │
   ├─────────────┴──────────────┬─────────────────────────────┴────┴────┤
   │ NAME 424x60 │STG│Tmax│Crew  (200x60 each)                          │  bottom
   │ TW   424x60 │COM│Tskin│Cap                                          │  zone
   │ CtrlGrp│SPCFT (212x80)  │  MODE GRID 6x2 (100x40) @ (424,520)       │  y400..600
   └────────────────────────────┴────────────────────────────────────────┘

   Right side (top zone), two 75px columns:
     Inner (REG) @ x=874: DOCKED 75x100 (vertical), then FLYING LOW / FLYING HIGH /
       LOW SPACE / HIGH SPACE (75x75).
     Outer (SIT) @ x=949: CONTACT / PRE-LAUNCH / FLIGHT / SUB-ORBIT / ORBIT /
       ESCAPE / LANDED / SPLASH (75x50).

   The C&W bit indices, two-tier (Pe/Prop/LifeSupport) and chute-env colour logic,
   and the alarm-mask/chirp wiring are UNCHANGED from rev 1 — only tile geometry,
   the row-4 cell order, and the bottom zone changed.

   TWO-TIER / CHUTE-ENV / CTRL-MODE conventions: see CautionWarning.ino. The bottom
   6x2 grid is driven by state.modeFlags (MF_* bits) reported by the master.
****************************************************************************************/
#include "KCMk1_Annunciator.h"

// Extern companion bools from CautionWarning.ino
extern bool peLowYellow;
extern bool propLowYellow;
extern bool lsYellow;


/***************************************************************************************
   LAYOUT CONSTANTS -- MAIN SCREEN (1024x600)
****************************************************************************************/
// Top/bottom split
static const uint16_t TOP_H         = 400;

// Master Alarm
static const uint16_t MASTER_X      =   0;
static const uint16_t MASTER_Y      =   0;
static const uint16_t MASTER_W      = 274;
static const uint16_t MASTER_H      = 176;

// SOI display (label + globe)
static const uint16_t SOI_LABEL_X   =   0;
static const uint16_t SOI_LABEL_Y   = 176;
static const uint16_t SOI_LABEL_W   = 274;
static const uint16_t SOI_LABEL_H   =  48;
static const uint16_t SOI_GLOBE_X   =   0;
static const uint16_t SOI_GLOBE_Y   = 224;
static const uint16_t SOI_GLOBE_W   = 274;
static const uint16_t SOI_GLOBE_H   = 176;

// C&W grid: 5 columns x 5 rows
static const uint16_t CW_X0         = 274;
static const uint16_t CW_Y0         =   0;
static const uint16_t CW_BTN_W      = 120;
static const uint16_t CW_BTN_H      =  80;
static const uint16_t CW_COLS       =   5;
static const uint16_t CW_ROWS       =   5;

// Inner flag column (regimes) @ x=874, w=75
static const uint16_t DOCK_X        = 874;
static const uint16_t DOCK_Y        =   0;
static const uint16_t DOCK_W        =  75;
static const uint16_t DOCK_H        = 100;
static const uint16_t REG_X         = 874;
static const uint16_t REG_Y         = 100;
static const uint16_t REG_W         =  75;
static const uint16_t REG_H         =  75;
static const uint16_t REG_COUNT     =   4;   // FLYING LOW/HIGH, LOW/HIGH SPACE

// Outer flag column (situations) @ x=949, w=75
static const uint16_t SIT_X0        = 949;
static const uint16_t SIT_Y0        =   0;
static const uint16_t SIT_BTN_W     =  75;
static const uint16_t SIT_BTN_H     =  50;
static const uint16_t SIT_COUNT     =   8;

// --- Bottom zone (y 400..600) ---
static const uint16_t BOT_Y         = 400;
static const uint16_t TEL_ROW_H     =  60;   // telemetry rows 1-2
// Left telemetry column (vessel name / timewarp), width 424
static const uint16_t NAME_X        =   0;
static const uint16_t NAME_W        = 424;
// Right telemetry triple (STG/Tmax/Crew, COM/Tskin/Cap), 3 cols x 200
static const uint16_t TEL_X0        = 424;
static const uint16_t TEL_W         = 200;
// Row 3: CtrlGrp + SPCFT (212 each, h80) and the mode grid
static const uint16_t R3_Y          = 520;
static const uint16_t R3_H          =  80;
static const uint16_t CG_X          =   0;
static const uint16_t CG_W          = 212;
static const uint16_t SPCFT_X       = 212;
static const uint16_t SPCFT_W       = 212;
// Mode/status grid: 6 columns x 2 rows, 100x40
static const uint16_t MODE_X0       = 424;
static const uint16_t MODE_Y0       = 520;
static const uint16_t MODE_BTN_W    = 100;
static const uint16_t MODE_BTN_H    =  40;
static const uint16_t MODE_COLS     =   6;
static const uint16_t MODE_ROWS     =   2;


/***************************************************************************************
   CELL-ORDER MAPS (rev 2 relayout)
****************************************************************************************/
// C&W bit -> grid cell (row-major 0..24). Rows 0-3 are 1:1; row 4 reorders to
// SRB, ORBIT STABLE, ELEC GEN, CHUTE ENV, EVA ACTIVE per the mockup.
static const uint8_t cwBitToCell[CW_COUNT] = {
   0, 1, 2, 3, 4,    5, 6, 7, 8, 9,    10,11,12,13,14,    15,16,17,18,19,
  /*CW_ORBIT_STABLE 20*/ 21,
  /*CW_ELEC_GEN     21*/ 22,
  /*CW_CHUTE_ENV    22*/ 23,
  /*CW_SRB_ACTIVE   23*/ 20,
  /*CW_EVA_ACTIVE   24*/ 24
};

// Outer situation column: display row (top..bottom) -> vesselSituation[] index.
// Mockup order CONTACT,PRELAUNCH,FLIGHT,SUBORBIT,ORBIT,ESCAPE,LANDED,SPLASH puts
// LANDED (arr 7) above SPLASH (arr 6).
static const uint8_t sitRowToArr[SIT_COUNT] = { 0, 1, 2, 3, 4, 5, 7, 6 };

// Inner regime column: display row (under DOCKED) -> flightCond[] index.
// Mockup order FLYING LOW, FLYING HIGH, LOW SPACE, HIGH SPACE.
static const uint8_t regRowToArr[REG_COUNT] = { 0, 2, 1, 3 };


/***************************************************************************************
   PRINT STATE -- one per printValue/printDisp slot (flicker-free)
****************************************************************************************/
PrintState psSOILabel;
PrintState psSTG, psTmax, psCrew;
PrintState psTW, psCOM, psTskin, psCap;
PrintState psCtrlGrp;


/***************************************************************************************
   C&W BUTTON LABELS — bit-indexed (unchanged from rev 1).
****************************************************************************************/
static const ButtonLabel cautWarn[CW_COUNT] = {
  // Row 0 -- WARNING (red)
  { "LOW \x94V",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 0  CW_LOW_DV
  { "HIGH G",        TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 1  CW_HIGH_G
  { "HIGH TEMP",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 2  CW_HIGH_TEMP
  { "BUS VOLTAGE",   TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 3  CW_BUS_VOLTAGE
  { "ABORT",         TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 4  CW_ABORT
  // Row 1 -- mixed
  { "GROUND PROX",   TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 5  CW_GROUND_PROX
  { "Pe LOW",        TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 6  CW_PE_LOW
  { "PROP LOW",      TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 7  CW_PROP_LOW
  { "LIFE SUPPORT",  TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 8  CW_LIFE_SUPPORT
  { "O2 PRESENT",    TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_NAVY,       TFT_GREY, TFT_GREY }, // 9  CW_O2_PRESENT
  // Row 2 -- CAUTION (yellow)
  { "IMPACT IMM",    TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 10 CW_IMPACT_IMM
  { "ALT",           TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 11 CW_ALT
  { "DESCENT",       TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 12 CW_DESCENT
  { "GEAR UP",       TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 13 CW_GEAR_UP
  { "ATMO",          TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 14 CW_ATMO
  // Row 3 -- CAUTION (yellow)
  { "RCS LOW",       TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 15 CW_RCS_LOW
  { "PROP RATIO",    TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 16 CW_PROP_IMBAL
  { "COMM LOST",     TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 17 CW_COMM_LOST
  { "Ap LOW",        TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 18 CW_Ap_LOW
  { "HIGH Q",        TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW, TFT_GREY, TFT_GREY }, // 19 CW_HIGH_Q
  // Row 4 -- POSITIVE / STATE (note: cell order remapped via cwBitToCell)
  { "ORBIT STABLE",  TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // 20 CW_ORBIT_STABLE
  { "ELEC GEN",      TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // 21 CW_ELEC_GEN
  { "CHUTE ENV",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // 22 CW_CHUTE_ENV
  { "SRB ACTIVE",    TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_ORANGE,     TFT_GREY, TFT_GREY }, // 23 CW_SRB_ACTIVE
  { "EVA ACTIVE",    TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_ORANGE,     TFT_GREY, TFT_GREY }, // 24 CW_EVA_ACTIVE
};


/***************************************************************************************
   VESSEL SITUATION COLUMN LABELS (outer column, array index order)
   index 0 = CONTACT (special: lit on LANDED|SPLASH), 1..7 map to VSIT_* bits.
****************************************************************************************/
static const ButtonLabel vesselSituation[SIT_COUNT] = {
  { "CONTACT",    TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_SKY,    TFT_GREY, TFT_GREY }, // 0 LANDED|SPLASH
  { "PRE- LAUNCH",TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY }, // 1 VSIT_PRELAUNCH
  { "FLIGHT",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY }, // 2 VSIT_FLIGHT
  { "SUB- ORBIT", TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY }, // 3 VSIT_SUBORBIT
  { "ORBIT",      TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY }, // 4 VSIT_ORBIT
  { "ESCAPE",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY }, // 5 VSIT_ESCAPE
  { "SPLASH",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_NAVY,   TFT_GREY, TFT_GREY }, // 6 VSIT_SPLASH
  { "LANDED",     TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY }, // 7 VSIT_LANDED
};


/***************************************************************************************
   REGIME TILES (inner column, flightCond index order: matches flightCondIndex()).
****************************************************************************************/
static const ButtonLabel flightCond[4] = {
  { "FLYING LOW",  TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // 0
  { "LOW SPACE",   TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // 1
  { "FLYING HIGH", TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // 2
  { "HIGH SPACE",  TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // 3
};


/***************************************************************************************
   MODE/STATUS GRID LABELS (6x2, row-major, MF_* bit order)
   On-colour per tile; lit when the corresponding state.modeFlags bit is set.
   All bits are reported by the master — provisional labels/colours (see header note).
****************************************************************************************/
static const ButtonLabel modeGrid[MF_COUNT] = {
  // Row 0
  { "DEMO",       TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_BLUE,       TFT_GREY, TFT_GREY }, // MF_DEMO
  { "WARP",       TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW,     TFT_GREY, TFT_GREY }, // MF_WARP
  { "AUDIO",      TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // MF_AUDIO
  { "THRTL ENA",  TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // MF_THRTL_ENA
  { "TRIM",       TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_AQUA,       TFT_GREY, TFT_GREY }, // MF_TRIM
  { "AUTOPILOT",  TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // MF_AUTOPILOT
  // Row 1
  { "DEBUG",      TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_PURPLE,     TFT_GREY, TFT_GREY }, // MF_DEBUG
  { "SWITCH ERR", TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // MF_SWITCH_ERR
  { "SIMPIT LOST",TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY }, // MF_SIMPIT_LOST
  { "THRTL PREC", TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // MF_THRTL_PREC
  { "INPUT PREC", TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // MF_INPUT_PREC
  { "ENG ARM",    TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }, // MF_ENG_ARM
};


/***************************************************************************************
   MASTER ALARM BUTTON LABEL
****************************************************************************************/
static const ButtonLabel masterAlarmLabel = {
  "MASTER\nALARM", TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED, TFT_GREY, TFT_GREY
};


/***************************************************************************************
   CTRL MODE HELPER (unchanged)
****************************************************************************************/
static const char *ctrlModeText(CtrlMode mode) {
  switch (mode) {
    case ctrl_Rover:      return "RVR";
    case ctrl_Plane:      return "PLN";
    case ctrl_Spacecraft: return "SPCFT";
    default:              return "ERR";
  }
}

static uint16_t ctrlModeColor(CtrlMode mode, VesselType type) {
  bool shouldBeSpacecraft = (type == type_Probe || type == type_Relay ||
                             type == type_Lander || type == type_Ship ||
                             type == type_Station);
  if (mode != ctrl_Spacecraft && shouldBeSpacecraft) return TFT_RED;
  if (mode != ctrl_Plane      && type == type_Plane)  return TFT_RED;
  if (mode != ctrl_Rover      && type == type_Rover)  return TFT_RED;
  return TFT_DARK_GREEN;
}


/***************************************************************************************
   FLIGHT CONDITION (regime) STATE — unchanged logic, returns flightCond[] index.
****************************************************************************************/
static int8_t flightCondIndex() {
  bool isAloft = bitRead(state.vesselSituationState, VSIT_FLIGHT)   ||
                 bitRead(state.vesselSituationState, VSIT_SUBORBIT)  ||
                 bitRead(state.vesselSituationState, VSIT_ORBIT)     ||
                 bitRead(state.vesselSituationState, VSIT_ESCAPE);
  if (!isAloft) return -1;
  if (inAtmo) {
    bool aboveFlyHigh = (currentBody.flyHigh > 0 && state.alt_sl > currentBody.flyHigh);
    return aboveFlyHigh ? 2 : 0;
  } else {
    bool aboveHighSpace = (currentBody.highSpace > 0 && state.alt_sl > currentBody.highSpace);
    return aboveHighSpace ? 3 : 1;
  }
}


/***************************************************************************************
   C&W PANEL UPDATE — unchanged logic; cell position via cwBitToCell[].
****************************************************************************************/
void updateCautWarnPanel(KCM_TFT &tft, uint32_t prevCW, uint32_t newCW) {
  uint32_t changed = prevCW ^ newCW;
  if (changed != 0 || chuteEnvState != prevChuteEnvState) {
    bitSet(changed, CW_PE_LOW);
    bitSet(changed, CW_PROP_LOW);
    bitSet(changed, CW_LIFE_SUPPORT);
    bitSet(changed, CW_CHUTE_ENV);
  }
  if (changed == 0) return;

  for (uint8_t i = 0; i < CW_COUNT; i++) {
    if (!bitRead(changed, i)) continue;

    uint8_t cell = cwBitToCell[i];
    uint8_t col  = cell % CW_COLS;
    uint8_t row  = cell / CW_COLS;
    int16_t x    = CW_X0 + col * CW_BTN_W;
    int16_t y    = CW_Y0 + row * CW_BTN_H;
    bool    on   = bitRead(newCW, i);

    ButtonLabel btn = cautWarn[i];

    if (i == CW_PE_LOW && !on && peLowYellow) {
      btn.backgroundColorOff = TFT_YELLOW; btn.fontColorOff = TFT_DARK_GREY;
      drawButton(tft, x, y, CW_BTN_W, CW_BTN_H, btn, &Roboto_Black_20, false); continue;
    }
    if (i == CW_PROP_LOW && !on && propLowYellow) {
      btn.backgroundColorOff = TFT_YELLOW; btn.fontColorOff = TFT_DARK_GREY;
      drawButton(tft, x, y, CW_BTN_W, CW_BTN_H, btn, &Roboto_Black_20, false); continue;
    }
    if (i == CW_LIFE_SUPPORT && !on && lsYellow) {
      btn.backgroundColorOff = TFT_YELLOW; btn.fontColorOff = TFT_DARK_GREY;
      drawButton(tft, x, y, CW_BTN_W, CW_BTN_H, btn, &Roboto_Black_20, false); continue;
    }
    if (i == CW_CHUTE_ENV) {
      switch (chuteEnvState) {
        case chute_Red:    btn.backgroundColorOn = TFT_RED;        btn.fontColorOn = TFT_WHITE;     break;
        case chute_Yellow: btn.backgroundColorOn = TFT_YELLOW;     btn.fontColorOn = TFT_DARK_GREY; break;
        case chute_Green:  btn.backgroundColorOn = TFT_DARK_GREEN; btn.fontColorOn = TFT_WHITE;     break;
        default: break;
      }
      drawButton(tft, x, y, CW_BTN_W, CW_BTN_H, btn, &Roboto_Black_20, chuteEnvState != chute_Off);
      continue;
    }
    drawButton(tft, x, y, CW_BTN_W, CW_BTN_H, btn, &Roboto_Black_20, on);
  }
  prevChuteEnvState = chuteEnvState;
}


/***************************************************************************************
   VESSEL SITUATION PANEL (outer column) — CONTACT special (LANDED|SPLASH).
****************************************************************************************/
static bool _prevContact = false;

void updateVesselSitPanel(KCM_TFT &tft, uint8_t prevSit, uint8_t newSit) {
  bool newContact = bitRead(newSit, VSIT_LANDED) || bitRead(newSit, VSIT_SPLASH);

  uint8_t changed = prevSit ^ newSit;
  for (uint8_t r = 0; r < SIT_COUNT; r++) {
    uint8_t arr = sitRowToArr[r];
    int16_t y   = SIT_Y0 + r * SIT_BTN_H;
    if (arr == 0) {                      // CONTACT row (special)
      if (newContact != _prevContact) {
        drawButton(tft, SIT_X0, y, SIT_BTN_W, SIT_BTN_H,
                   vesselSituation[0], &Roboto_Black_12, newContact);
      }
      continue;
    }
    if (!bitRead(changed, arr)) continue;
    drawButton(tft, SIT_X0, y, SIT_BTN_W, SIT_BTN_H,
               vesselSituation[arr], &Roboto_Black_12, bitRead(newSit, arr));
  }
  _prevContact = newContact;
}


/***************************************************************************************
   INNER REGIME COLUMN — only one lit at a time (flightCondIndex()).
****************************************************************************************/
static int8_t prevFlightCondIdx = -2;

void updateRegimeColumn(KCM_TFT &tft) {
  int8_t idx = flightCondIndex();
  if (idx == prevFlightCondIdx) return;
  for (uint8_t r = 0; r < REG_COUNT; r++) {
    uint8_t arr = regRowToArr[r];
    int16_t y   = REG_Y + r * REG_H;
    drawButton(tft, REG_X, y, REG_W, REG_H,
               flightCond[arr], &Roboto_Black_12, (arr == (uint8_t)idx));
  }
  prevFlightCondIdx = idx;
}


/***************************************************************************************
   MODE/STATUS GRID — 6x2, driven by state.modeFlags (master-reported).
****************************************************************************************/
static uint16_t prevModeFlags = 0xFFFF;  // force full draw on first pass

void updateModeGrid(KCM_TFT &tft) {
  if (state.modeFlags == prevModeFlags) return;
  uint16_t changed = state.modeFlags ^ prevModeFlags;
  for (uint8_t i = 0; i < MF_COUNT; i++) {
    if (!bitRead(changed, i)) continue;
    uint8_t col = i % MODE_COLS;
    uint8_t row = i / MODE_COLS;
    int16_t x   = MODE_X0 + col * MODE_BTN_W;
    int16_t y   = MODE_Y0 + row * MODE_BTN_H;
    drawButton(tft, x, y, MODE_BTN_W, MODE_BTN_H,
               modeGrid[i], &Roboto_Black_12, bitRead(state.modeFlags, i));
  }
  prevModeFlags = state.modeFlags;
}


/***************************************************************************************
   DOCKED INDICATOR — vertical text, top of inner column.
****************************************************************************************/
static bool prevDockedState = false;

void updateDockedIndicator(KCM_TFT &tft) {
  bool isDocked = bitRead(state.vesselSituationState, VSIT_DOCKED);
  if (isDocked == prevDockedState) return;
  prevDockedState = isDocked;
  uint16_t bgColor   = isDocked ? TFT_JUNGLE : TFT_OFF_BLACK;
  uint16_t textColor = isDocked ? TFT_WHITE  : TFT_DARK_GREY;
  drawVerticalText(tft, DOCK_X, DOCK_Y, DOCK_W, DOCK_H,
                   &Roboto_Black_16, "DOCKED", textColor, bgColor);
  tft.drawRect(DOCK_X, DOCK_Y, DOCK_W, DOCK_H, TFT_GREY);
}


/***************************************************************************************
   SPCFT / control-mode tile (row 3, right of CtrlGrp) — text colour by mode match.
****************************************************************************************/
static CtrlMode prevSpcftMode = (CtrlMode)0xFF;
static VesselType prevSpcftType = (VesselType)0xFF;

void updateSpcftTile(KCM_TFT &tft) {
  if (state.vehCtrlMode == prevSpcftMode && state.vesselType == prevSpcftType) return;
  prevSpcftMode = state.vehCtrlMode;
  prevSpcftType = state.vesselType;
  // Black background, coloured text (green = mode matches vessel type, red = mismatch).
  // TODO(rev2): draw the vessel-type icon to the right of the text (mockup shows one).
  ButtonLabel b = { ctrlModeText(state.vehCtrlMode),
                    ctrlModeColor(state.vehCtrlMode, state.vesselType), TFT_WHITE,
                    TFT_BLACK, TFT_BLACK, TFT_GREY, TFT_GREY };
  drawButton(tft, SPCFT_X, R3_Y, SPCFT_W, R3_H, b, &Roboto_Black_24, false);
}


/***************************************************************************************
   RESET SENTINELS — force full redraws on next pass / between TestMode steps.
****************************************************************************************/
void resetSitAndPanelState() {
  _prevContact      = false;
  prevFlightCondIdx = -2;
  prevModeFlags     = 0xFFFF;
  prevSpcftMode     = (CtrlMode)0xFF;
  prevSpcftType     = (VesselType)0xFF;
}
void forceContactState(bool newContact) { _prevContact = !newContact; }
void forceDockState(bool isDocked)      { prevDockedState = !isDocked; }


/***************************************************************************************
   TELEMETRY VALUE HELPERS (bottom zone)
****************************************************************************************/
// Tmax / Tskin: percent where HIGH is bad (green < 50, yellow < 85, red above).
static void tempTierColor(uint8_t pct, uint16_t &fc, uint16_t &bc) {
  thresholdColor((uint16_t)pct, 50, TFT_DARK_GREEN, TFT_BLACK,
                                 85, TFT_YELLOW,     TFT_BLACK,
                                     TFT_WHITE,      TFT_RED, fc, bc);
}
// COM: percent where LOW is bad (red < 25, yellow < 75, green above).
static void commTierColor(uint8_t pct, uint16_t &fc, uint16_t &bc) {
  thresholdColor((uint16_t)pct, 25, TFT_RED,        TFT_BLACK,
                                 75, TFT_YELLOW,     TFT_BLACK,
                                     TFT_DARK_GREEN, TFT_BLACK, fc, bc);
}


/***************************************************************************************
   STATIC CHROME — draw once on screen entry.
****************************************************************************************/
void drawStaticMain(KCM_TFT &tft) {
  tft.fillScreen(TFT_BLACK);

  // Master Alarm
  drawButton(tft, MASTER_X, MASTER_Y, MASTER_W, MASTER_H,
             masterAlarmLabel, &Roboto_Black_48, state.masterAlarmOn);
  prev.masterAlarmOn = state.masterAlarmOn;

  // C&W panel (full redraw)
  updateCautWarnPanel(tft, ~state.cautionWarningState, state.cautionWarningState);
  prev.cautionWarningState = state.cautionWarningState;

  // Situation outer column (force all rows)
  resetSitAndPanelState();
  forceContactState(bitRead(state.vesselSituationState, VSIT_LANDED) ||
                    bitRead(state.vesselSituationState, VSIT_SPLASH));
  updateVesselSitPanel(tft, 0xFF, state.vesselSituationState);
  prev.vesselSituationState = state.vesselSituationState;

  // Inner regime column + DOCKED
  prevFlightCondIdx = -2;
  updateRegimeColumn(tft);
  prevDockedState = !bitRead(state.vesselSituationState, VSIT_DOCKED);
  updateDockedIndicator(tft);

  // Mode grid (full redraw)
  prevModeFlags = ~state.modeFlags;
  updateModeGrid(tft);

  // SPCFT tile
  prevSpcftMode = (CtrlMode)0xFF;
  updateSpcftTile(tft);

  // SOI label chrome + globe border
  printDispChrome(tft, &Roboto_Black_24, SOI_LABEL_X, SOI_LABEL_Y,
                  SOI_LABEL_W, SOI_LABEL_H, "SOI:", TFT_WHITE, TFT_BLACK, TFT_GREY);
  tft.drawRect(SOI_GLOBE_X, SOI_GLOBE_Y, SOI_GLOBE_W, SOI_GLOBE_H, TFT_GREY);

  // Vessel name (left telemetry, row 1) — green, left-aligned
  tft.fillRect(NAME_X, BOT_Y, NAME_W, TEL_ROW_H, TFT_BLACK);
  textLeft(tft, &Roboto_Black_24, NAME_X, BOT_Y, NAME_W, TEL_ROW_H,
           state.vesselName, TFT_DARK_GREEN, TFT_BLACK);

  // Telemetry labels (chrome) — values drawn in the update pass.
  printDispChrome(tft, &Roboto_Black_24, NAME_X, BOT_Y + TEL_ROW_H, NAME_W, TEL_ROW_H,
                  "TimeWarp:", TFT_WHITE, TFT_BLACK, TFT_GREY);
  printDispChrome(tft, &Roboto_Black_24, TEL_X0,             BOT_Y, TEL_W, TEL_ROW_H, "STG:",   TFT_WHITE, TFT_BLACK, TFT_GREY);
  printDispChrome(tft, &Roboto_Black_24, TEL_X0 + TEL_W,     BOT_Y, TEL_W, TEL_ROW_H, "Tmax:",  TFT_WHITE, TFT_BLACK, TFT_GREY);
  printDispChrome(tft, &Roboto_Black_24, TEL_X0 + 2 * TEL_W, BOT_Y, TEL_W, TEL_ROW_H, "Crew:",  TFT_WHITE, TFT_BLACK, TFT_GREY);
  printDispChrome(tft, &Roboto_Black_24, TEL_X0,             BOT_Y + TEL_ROW_H, TEL_W, TEL_ROW_H, "COM:",   TFT_WHITE, TFT_BLACK, TFT_GREY);
  printDispChrome(tft, &Roboto_Black_24, TEL_X0 + TEL_W,     BOT_Y + TEL_ROW_H, TEL_W, TEL_ROW_H, "Tskin:", TFT_WHITE, TFT_BLACK, TFT_GREY);
  printDispChrome(tft, &Roboto_Black_24, TEL_X0 + 2 * TEL_W, BOT_Y + TEL_ROW_H, TEL_W, TEL_ROW_H, "Cap:",   TFT_WHITE, TFT_BLACK, TFT_GREY);
  printDispChrome(tft, &Roboto_Black_24, CG_X, R3_Y, CG_W, R3_H, "CtrlGrp:", TFT_WHITE, TFT_BLACK, TFT_GREY);
}


/***************************************************************************************
   UPDATE PASS — redraws only widgets whose state has changed.
****************************************************************************************/
void updateScreenMain(KCM_TFT &tft) {

  // --- MASTER ALARM ---
  if (state.masterAlarmOn != prev.masterAlarmOn) {
    drawButton(tft, MASTER_X, MASTER_Y, MASTER_W, MASTER_H,
               masterAlarmLabel, &Roboto_Black_48, state.masterAlarmOn);
    prev.masterAlarmOn = state.masterAlarmOn;
  }

  // --- C&W PANEL --- (alarm-mask + chirp wiring unchanged)
  if (state.cautionWarningState != prev.cautionWarningState ||
      chuteEnvState != prevChuteEnvState) {
    uint32_t newBits = state.cautionWarningState & ~prev.cautionWarningState;
    uint32_t clrBits = prev.cautionWarningState  & ~state.cautionWarningState;

    if (audioEnabled) {
      if (newBits & (1ul << CW_LOW_DV))        updateAlarmMask(ALARM_LOW_DV,       true);
      if (clrBits & (1ul << CW_LOW_DV))        updateAlarmMask(ALARM_LOW_DV,       false);
      if (newBits & (1ul << CW_HIGH_G))        updateAlarmMask(ALARM_HIGH_G,       true);
      if (clrBits & (1ul << CW_HIGH_G))        updateAlarmMask(ALARM_HIGH_G,       false);
      if (newBits & (1ul << CW_HIGH_TEMP))     updateAlarmMask(ALARM_HIGH_TEMP,    true);
      if (clrBits & (1ul << CW_HIGH_TEMP))     updateAlarmMask(ALARM_HIGH_TEMP,    false);
      if (newBits & (1ul << CW_BUS_VOLTAGE))   updateAlarmMask(ALARM_BUS_VOLTAGE,  true);
      if (clrBits & (1ul << CW_BUS_VOLTAGE))   updateAlarmMask(ALARM_BUS_VOLTAGE,  false);
      if (newBits & (1ul << CW_ABORT))         updateAlarmMask(ALARM_ABORT,        true);
      if (clrBits & (1ul << CW_ABORT))         updateAlarmMask(ALARM_ABORT,        false);
      if (newBits & (1ul << CW_GROUND_PROX))   updateAlarmMask(ALARM_GROUND_PROX,  true);
      if (clrBits & (1ul << CW_GROUND_PROX))   updateAlarmMask(ALARM_GROUND_PROX,  false);
      if (newBits & (1ul << CW_PE_LOW))        updateAlarmMask(ALARM_PE_LOW,       true);
      if (clrBits & (1ul << CW_PE_LOW))        updateAlarmMask(ALARM_PE_LOW,       false);
      if (newBits & (1ul << CW_PROP_LOW))      updateAlarmMask(ALARM_PROP_LOW,     true);
      if (clrBits & (1ul << CW_PROP_LOW))      updateAlarmMask(ALARM_PROP_LOW,     false);
      if (newBits & (1ul << CW_LIFE_SUPPORT))  updateAlarmMask(ALARM_LIFE_SUPPORT, true);
      if (clrBits & (1ul << CW_LIFE_SUPPORT))  updateAlarmMask(ALARM_LIFE_SUPPORT, false);

      if (newBits & (1ul << CW_ALT))           audioCautionTone();
      if (newBits & (1ul << CW_IMPACT_IMM))    audioCautionTone();
      if (newBits & ((1ul << CW_DESCENT) | (1ul << CW_ATMO) | (1ul << CW_GEAR_UP)))
        audioCautionChirp();
    }

    updateCautWarnPanel(tft, prev.cautionWarningState, state.cautionWarningState);
    prev.cautionWarningState = state.cautionWarningState;
  }

  // --- SITUATION COLUMN ---
  if (state.vesselSituationState != prev.vesselSituationState) {
    updateVesselSitPanel(tft, prev.vesselSituationState, state.vesselSituationState);
    prev.vesselSituationState = state.vesselSituationState;
  }

  // --- INNER REGIME COLUMN + DOCKED + MODE GRID + SPCFT ---
  updateRegimeColumn(tft);
  updateDockedIndicator(tft);
  updateModeGrid(tft);
  updateSpcftTile(tft);

  // --- SOI LABEL + GLOBE ---
  if (state.gameSOI != prev.gameSOI) {
    printDisp(tft, &Roboto_Black_24, SOI_LABEL_X, SOI_LABEL_Y,
              SOI_LABEL_W, SOI_LABEL_H, "SOI:", currentBody.dispName,
              TFT_WHITE, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, TFT_GREY, psSOILabel);
    tft.fillRect(SOI_GLOBE_X + 1, SOI_GLOBE_Y + 1, SOI_GLOBE_W - 2, SOI_GLOBE_H - 2, TFT_BLACK);
    // Body BMP is 240x168; centre it in the 274x176 globe area until assets are
    // regenerated to fill it. (rev2 TODO: 1024x600 body art.)
    drawBMP(tft, currentBody.image, SOI_GLOBE_X + (SOI_GLOBE_W - 240) / 2,
            SOI_GLOBE_Y + (SOI_GLOBE_H - 168) / 2);
    tft.drawRect(SOI_GLOBE_X, SOI_GLOBE_Y, SOI_GLOBE_W, SOI_GLOBE_H, TFT_GREY);
    prev.gameSOI = state.gameSOI;
  }

  // --- VESSEL NAME ---
  if (state.vesselName != prev.vesselName) {
    tft.fillRect(NAME_X, BOT_Y, NAME_W, TEL_ROW_H, TFT_BLACK);
    textLeft(tft, &Roboto_Black_24, NAME_X, BOT_Y, NAME_W, TEL_ROW_H,
             state.vesselName, TFT_DARK_GREEN, TFT_BLACK);
    prev.vesselName = state.vesselName;
  }

  // --- TELEMETRY VALUES ---
  if (state.stage != prev.stage) {
    printValue(tft, &Roboto_Black_24, TEL_X0, BOT_Y, TEL_W, TEL_ROW_H,
               "STG:", formatInt(state.stage), TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, psSTG);
    prev.stage = state.stage;
  }
  if (state.maxTemp != prev.maxTemp) {
    uint16_t fc, bc; tempTierColor(state.maxTemp, fc, bc);
    printValue(tft, &Roboto_Black_24, TEL_X0 + TEL_W, BOT_Y, TEL_W, TEL_ROW_H,
               "Tmax:", formatPerc(state.maxTemp), fc, bc, TFT_BLACK, psTmax);
    prev.maxTemp = state.maxTemp;
  }
  if (state.crewCount != prev.crewCount) {
    printValue(tft, &Roboto_Black_24, TEL_X0 + 2 * TEL_W, BOT_Y, TEL_W, TEL_ROW_H,
               "Crew:", formatInt(state.crewCount), TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, psCrew);
    prev.crewCount = state.crewCount;
  }
  if (state.twIndex != prev.twIndex) {
    printValue(tft, &Roboto_Black_24, NAME_X, BOT_Y + TEL_ROW_H, NAME_W, TEL_ROW_H,
               "TimeWarp:", twString(state.twIndex, physTW), TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, psTW);
    prev.twIndex = state.twIndex;
  }
  if (state.commNet != prev.commNet) {
    uint16_t fc, bc; commTierColor(state.commNet, fc, bc);
    printValue(tft, &Roboto_Black_24, TEL_X0, BOT_Y + TEL_ROW_H, TEL_W, TEL_ROW_H,
               "COM:", formatPerc(state.commNet), fc, bc, TFT_BLACK, psCOM);
    prev.commNet = state.commNet;
  }
  if (state.skinTemp != prev.skinTemp) {
    uint16_t fc, bc; tempTierColor(state.skinTemp, fc, bc);
    printValue(tft, &Roboto_Black_24, TEL_X0 + TEL_W, BOT_Y + TEL_ROW_H, TEL_W, TEL_ROW_H,
               "Tskin:", formatPerc(state.skinTemp), fc, bc, TFT_BLACK, psTskin);
    prev.skinTemp = state.skinTemp;
  }
  if (state.capValue != prev.capValue) {
    printValue(tft, &Roboto_Black_24, TEL_X0 + 2 * TEL_W, BOT_Y + TEL_ROW_H, TEL_W, TEL_ROW_H,
               "Cap:", formatInt(state.capValue), TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, psCap);
    prev.capValue = state.capValue;
  }
  if (state.ctrlGrp != prev.ctrlGrp) {
    printValue(tft, &Roboto_Black_24, CG_X, R3_Y, CG_W, R3_H,
               "CtrlGrp:", formatInt(state.ctrlGrp), TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, psCtrlGrp);
    prev.ctrlGrp = state.ctrlGrp;
  }

  // --- THRESHOLD CROSSING ALERT CHIRPS (unchanged) ---
  if (audioEnabled && !firstPassOnMain) {
    if (state.alt_sl   >= ALERT_ALT_THRESHOLD && prev.alt_sl   < ALERT_ALT_THRESHOLD) audioAlertChirp();
    if (state.vel_surf >= ALERT_VEL_THRESHOLD && prev.vel_surf < ALERT_VEL_THRESHOLD) audioAlertChirp();
    if (currentBody.minSafe > 0 &&
        state.apoapsis >= currentBody.minSafe && prev.apoapsis < currentBody.minSafe) audioAlertChirp();
    if ((state.vesselSituationState & (1 << VSIT_ORBIT)) &&
        !(prev.vesselSituationState & (1 << VSIT_ORBIT))) audioAlertChirp();
  }

  prev.alt_sl   = state.alt_sl;
  prev.vel_surf = state.vel_surf;
  prev.apoapsis = state.apoapsis;
  firstPassOnMain = false;
}
