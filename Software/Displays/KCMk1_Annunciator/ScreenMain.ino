/***************************************************************************************
   ScreenMain.ino -- Main screen for Kerbal Controller Mk1 Annunciator
   Layout constants, static chrome, C&W panel helpers, and per-frame update pass.
   Alarm condition tracking (ALARM_* bits, alarmActiveMask, updateAlarmMask) lives
   in Audio.ino alongside the rest of the audio wiring.
****************************************************************************************/
#include "KCMk1_Annunciator.h"



/***************************************************************************************
   LAYOUT CONSTANTS -- MAIN SCREEN
****************************************************************************************/
const uint16_t MASTER_W    = 240;
const uint16_t MASTER_H    = 168;
const uint16_t CAUTWARN_W  = 126;
const uint16_t CAUTWARN_H  = 96;
const uint16_t IND_W       = 56;
const uint16_t IND_H       = CAUTWARN_H / 2;
const uint16_t DATA_W      = 160;
const uint16_t DATA_H      = IND_H;
const uint16_t SOI_LABEL_W = MASTER_W;
const uint16_t SOI_LABEL_H = IND_H;

// Panel origins
const uint16_t CAUTWARN_X0 = MASTER_W;
const uint16_t CAUTWARN_Y0 = 0;
const uint16_t SITPANEL_X0 = MASTER_W + 4 * CAUTWARN_W;
const uint16_t SITPANEL_Y0 = IND_H;
const uint16_t CTRLMODE_X0 = SITPANEL_X0;
const uint16_t CTRLMODE_Y0 = 0;


/***************************************************************************************
   PRINT STATE — KDC v2 flicker-free rendering
   One PrintState per printDisp / printValue call site.
   Initialised to sentinel values by default constructor.
****************************************************************************************/
PrintState psSOILabel;     // SOI label in master area
PrintState psMaxTemp;      // Tmax data row
PrintState psCrew;         // Crew data row
PrintState psTW;           // TW data row
PrintState psCommNet;      // COM data row
PrintState psStage;        // STG data row
PrintState psSkinTemp;     // Tskin data row
PrintState psCtrlGrp;      // CtrlGrp data row
PrintState psVesselName;   // vessel name (printName uses internally)


/***************************************************************************************
   BUTTON LABEL DEFINITIONS
   Note: - stored at 0x94 in Roboto_Black fonts (non-standard encoding)
****************************************************************************************/

// Caution/Warning panel -- 16 buttons, column-major order (col*4 + row)
const ButtonLabel cautWarn[16] = {
  { "HIGH SPACE",  TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY },
  { "LOW SPACE",   TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY },
  { "FLYING HIGH", TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY },
  { "FLYING LOW",  TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY },
  { "ALT",         TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW,     TFT_GREY, TFT_GREY },
  { "DESCENT",     TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW,     TFT_GREY, TFT_GREY },
  { "GROUND PROX", TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY },
  { "MECO",        TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY },
  { "HIGH G",      TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY },
  { "BUS VOLTAGE", TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY },
  { "HIGH TEMP",   TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY },
  { "LOW \x94V",   TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY },
  { "WARP",        TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW,     TFT_GREY, TFT_GREY },
  { "ATMO",        TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_YELLOW,     TFT_GREY, TFT_GREY },
  { "O2 PRESENT",  TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_BLUE,       TFT_GREY, TFT_GREY },
  { "CONTACT",     TFT_DARK_GREY, TFT_WHITE,     TFT_OFF_BLACK, TFT_BLUE,       TFT_GREY, TFT_GREY },
};

// Vessel situation column -- 7 buttons, top to bottom
const ButtonLabel vesselSituation[7] = {
  { "DOCKED",      TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY },
  { "PRE- LAUNCH", TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY },
  { "FLIGHT",      TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY },
  { "SUB- ORBIT",  TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY },
  { "ORBIT",       TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY },
  { "ESCAPE",      TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_JUNGLE, TFT_GREY, TFT_GREY },
  { "SPLASH",      TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_NAVY,   TFT_GREY, TFT_GREY },
};

// Master alarm button
const ButtonLabel masterAlarmLabel = {
  "MASTER ALARM", TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_RED, TFT_GREY, TFT_GREY
};

// Control mode button -- text and fontColorOn set dynamically at draw time
const ButtonLabel ctrlModeTemplate = {
  "", TFT_DARK_GREY, TFT_WHITE, TFT_OFF_BLACK, TFT_BLACK, TFT_GREY, TFT_GREY
};


/***************************************************************************************
   PANEL DRAW HELPERS
****************************************************************************************/

void updateCautWarnPanel(RA8875 &tft, uint16_t prevState, uint16_t newState) {
  uint16_t changed = prevState ^ newState;
  if (changed == 0) return;
  for (uint8_t i = 0; i < 16; i++) {
    if (!bitRead(changed, i)) continue;
    uint8_t col = i / 4;
    uint8_t row = i % 4;
    int16_t x   = CAUTWARN_X0 + col * CAUTWARN_W;
    int16_t y   = CAUTWARN_Y0 + row * CAUTWARN_H;
    drawButton(tft, x, y, CAUTWARN_W, CAUTWARN_H,
               cautWarn[i], &Roboto_Black_24, bitRead(newState, i));
  }
}

void updateVesselSitPanel(RA8875 &tft, uint8_t prevState, uint8_t newState) {
  uint8_t changed = prevState ^ newState;
  if (changed == 0) return;
  for (uint8_t row = 0; row < 7; row++) {
    if (!bitRead(changed, row)) continue;
    int16_t x = SITPANEL_X0;
    int16_t y = SITPANEL_Y0 + row * IND_H;
    drawButton(tft, x, y, IND_W, IND_H,
               vesselSituation[row], &Roboto_Black_12, bitRead(newState, row));
  }
}

const char *ctrlModeText(CtrlMode mode) {
  switch (mode) {
    case ctrl_Rover:      return "ROVER";
    case ctrl_Plane:      return "PLANE";
    case ctrl_Spacecraft: return "SPCFT";
    default:              return "ERROR";
  }
}

uint16_t ctrlModeColor(CtrlMode mode, VesselType type) {
  bool shouldBeSpacecraft = (type == type_Probe || type == type_Relay ||
                             type == type_Lander || type == type_Ship || type == type_Station);
  if (mode != ctrl_Spacecraft && shouldBeSpacecraft) return TFT_RED;
  if (mode != ctrl_Plane      && type == type_Plane)  return TFT_RED;
  if (mode != ctrl_Rover      && type == type_Rover)  return TFT_RED;
  return TFT_DARK_GREEN;
}

void updateCtrlMode(RA8875 &tft, CtrlMode newMode, VesselType newType,
                    CtrlMode prevMode, VesselType prevType) {
  if (newMode == prevMode && newType == prevType) return;
  ButtonLabel btn   = ctrlModeTemplate;
  btn.text          = ctrlModeText(newMode);
  btn.fontColorOn   = ctrlModeColor(newMode, newType);
  drawButton(tft, CTRLMODE_X0, CTRLMODE_Y0, IND_W, IND_H, btn, &Roboto_Black_12, true);
}


/***************************************************************************************
   STATIC CHROME -- draw once on screen entry
   Draws all fixed elements and syncs the corresponding prev fields to match.
   Syncing prev here prevents the update pass from immediately re-drawing elements
   that were just drawn, which would cause a visible flicker on screen entry.
****************************************************************************************/
void drawStaticMain(RA8875 &tft) {
  tft.setXY(0, 0);
  tft.fillScreen(TFT_BLACK);

  drawButton(tft, 0, 0, MASTER_W, MASTER_H, masterAlarmLabel,
             &Roboto_Black_36, state.masterAlarmOn);
  prev.masterAlarmOn = state.masterAlarmOn;

  updateCautWarnPanel(tft, ~state.cautionWarningState, state.cautionWarningState);
  prev.cautionWarningState = state.cautionWarningState;

  updateVesselSitPanel(tft, ~state.vesselSituationState, state.vesselSituationState);
  prev.vesselSituationState = state.vesselSituationState;

  updateCtrlMode(tft, state.vehCtrlMode, state.vesselType, (CtrlMode)0xFF, (VesselType)0xFF);
  prev.vehCtrlMode = state.vehCtrlMode;
  prev.vesselType  = state.vesselType;

  printDispChrome(tft, &Roboto_Black_24, 0, MASTER_H, SOI_LABEL_W, SOI_LABEL_H,
                  "SOI:", TFT_WHITE, TFT_BLACK, TFT_GREY);
  tft.drawRect(0, MASTER_H + SOI_LABEL_H, MASTER_W, MASTER_H, TFT_GREY);

  printDispChrome(tft, &Roboto_Black_24, 3 * DATA_W, 4 * CAUTWARN_H, DATA_W, DATA_H,
                  "Tmax:", TFT_WHITE, TFT_BLACK, TFT_GREY);
  printDispChrome(tft, &Roboto_Black_24, 4 * DATA_W, 4 * CAUTWARN_H, DATA_W, DATA_H,
                  "Crew:", TFT_WHITE, TFT_BLACK, TFT_GREY);

  printDispChrome(tft, &Roboto_Black_24, 0,           4 * CAUTWARN_H + DATA_H, DATA_W, DATA_H,
                  "TW:",      TFT_WHITE, TFT_BLACK, TFT_GREY);
  printDispChrome(tft, &Roboto_Black_24, DATA_W,      4 * CAUTWARN_H + DATA_H, DATA_W, DATA_H,
                  "COM:",     TFT_WHITE, TFT_BLACK, TFT_GREY);
  printDispChrome(tft, &Roboto_Black_24, 2 * DATA_W,  4 * CAUTWARN_H + DATA_H, DATA_W, DATA_H,
                  "STG:",     TFT_WHITE, TFT_BLACK, TFT_GREY);
  printDispChrome(tft, &Roboto_Black_24, 3 * DATA_W,  4 * CAUTWARN_H + DATA_H, DATA_W, DATA_H,
                  "Tskin:",   TFT_WHITE, TFT_BLACK, TFT_GREY);
  printDispChrome(tft, &Roboto_Black_24, 4 * DATA_W,  4 * CAUTWARN_H + DATA_H, DATA_W, DATA_H,
                  "CtrlGrp:", TFT_WHITE, TFT_BLACK, TFT_GREY);
}


/***************************************************************************************
   UPDATE PASS -- redraws only widgets whose state has changed
****************************************************************************************/
void updateScreenMain(RA8875 &tft) {

  if (state.masterAlarmOn != prev.masterAlarmOn) {
    drawButton(tft, 0, 0, MASTER_W, MASTER_H,
               masterAlarmLabel, &Roboto_Black_36, state.masterAlarmOn);
    prev.masterAlarmOn = state.masterAlarmOn;
  }

  if (state.cautionWarningState != prev.cautionWarningState) {
    // XOR with prev gives the set of bits that changed.
    // newBits = bits that just turned ON  (prev=0, new=1) -> trigger alarm/tone
    // clrBits = bits that just turned OFF (prev=1, new=0) -> cancel alarm
    uint16_t newBits = state.cautionWarningState & ~prev.cautionWarningState;
    uint16_t clrBits = prev.cautionWarningState  & ~state.cautionWarningState;

    if (audioEnabled) {
      if (newBits & (1 << CW_GROUND_PROX)) updateAlarmMask(ALARM_GROUND_PROX, true);
      if (clrBits & (1 << CW_GROUND_PROX)) updateAlarmMask(ALARM_GROUND_PROX, false);
      if (newBits & (1 << CW_HIGH_G))      updateAlarmMask(ALARM_HIGH_G,      true);
      if (clrBits & (1 << CW_HIGH_G))      updateAlarmMask(ALARM_HIGH_G,      false);
      if (newBits & (1 << CW_BUS_VOLTAGE)) updateAlarmMask(ALARM_BUS_VOLTAGE, true);
      if (clrBits & (1 << CW_BUS_VOLTAGE)) updateAlarmMask(ALARM_BUS_VOLTAGE, false);
      if (newBits & (1 << CW_HIGH_TEMP))   updateAlarmMask(ALARM_HIGH_TEMP,   true);
      if (clrBits & (1 << CW_HIGH_TEMP))   updateAlarmMask(ALARM_HIGH_TEMP,   false);
      if (newBits & (1 << CW_LOW_DV))      updateAlarmMask(ALARM_LOW_DV,      true);
      if (clrBits & (1 << CW_LOW_DV))      updateAlarmMask(ALARM_LOW_DV,      false);
      if (newBits & (1 << CW_ALT))         audioCautionTone();
      if (newBits & ((1 << CW_DESCENT) | (1 << CW_ATMO))) audioCautionChirp();
    }

    updateCautWarnPanel(tft, prev.cautionWarningState, state.cautionWarningState);
    prev.cautionWarningState = state.cautionWarningState;
  }

  if (state.vehCtrlMode != prev.vehCtrlMode || state.vesselType != prev.vesselType) {
    updateCtrlMode(tft, state.vehCtrlMode, state.vesselType,
                   prev.vehCtrlMode, prev.vesselType);
    prev.vehCtrlMode = state.vehCtrlMode;
    prev.vesselType  = state.vesselType;
  }

  if (state.vesselSituationState != prev.vesselSituationState) {
    updateVesselSitPanel(tft, prev.vesselSituationState, state.vesselSituationState);
    prev.vesselSituationState = state.vesselSituationState;
  }

  if (state.gameSOI != prev.gameSOI) {
    printDisp(tft, &Roboto_Black_24, 0, MASTER_H, SOI_LABEL_W, SOI_LABEL_H,
              "SOI:", currentBody.dispName, TFT_WHITE, TFT_DARK_GREEN,
              TFT_BLACK, TFT_BLACK, TFT_GREY, psSOILabel);
    tft.fillRect(1, MASTER_H + SOI_LABEL_H + 1, MASTER_W - 2, MASTER_H - 2, TFT_BLACK);
    drawBMP(tft, currentBody.image, 2, MASTER_H + SOI_LABEL_H + 2);
    tft.drawRect(0, MASTER_H + SOI_LABEL_H, MASTER_W, MASTER_H, TFT_GREY);
    prev.gameSOI = state.gameSOI;
  }

  if (state.vesselName != prev.vesselName) {
    printName(tft, &Roboto_Black_24, 0, 4 * CAUTWARN_H, 3 * DATA_W, DATA_H,
              state.vesselName, TFT_DARK_GREEN, TFT_BLACK, TFT_GREY, 32);
    prev.vesselName = state.vesselName;
  }

  if (state.maxTemp != prev.maxTemp) {
    uint16_t f, b;
    thresholdColor(state.maxTemp, tempCaution, TFT_DARK_GREEN, TFT_BLACK,
                   tempAlarm, TFT_DARK_GREY, TFT_YELLOW, TFT_WHITE, TFT_RED, f, b);
    printValue(tft, &Roboto_Black_24, 3 * DATA_W, 4 * CAUTWARN_H, DATA_W, DATA_H,
               "Tmax:", formatPerc(state.maxTemp), f, b, TFT_BLACK, psMaxTemp);
    prev.maxTemp = state.maxTemp;
  }

  if (state.crewCount != prev.crewCount) {
    printValue(tft, &Roboto_Black_24, 4 * DATA_W, 4 * CAUTWARN_H, DATA_W, DATA_H,
               "Crew:", formatInt(state.crewCount), TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, psCrew);
    prev.crewCount = state.crewCount;
  }

  if (state.twIndex != prev.twIndex) {
    printValue(tft, &Roboto_Black_24, 0, 4 * CAUTWARN_H + DATA_H, DATA_W, DATA_H,
               "TW:", twString(state.twIndex, physTW), TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, psTW);
    prev.twIndex = state.twIndex;
  }

  if (state.commNet != prev.commNet) {
    uint16_t f, b;
    thresholdColor(state.commNet, commAlarm, TFT_RED, TFT_BLACK,
                   commCaution, TFT_YELLOW, TFT_BLACK, TFT_DARK_GREEN, TFT_BLACK, f, b);
    printValue(tft, &Roboto_Black_24, DATA_W, 4 * CAUTWARN_H + DATA_H, DATA_W, DATA_H,
               "COM:", formatPerc(state.commNet), f, b, TFT_BLACK, psCommNet);
    prev.commNet = state.commNet;
  }

  if (state.stage != prev.stage) {
    printValue(tft, &Roboto_Black_24, 2 * DATA_W, 4 * CAUTWARN_H + DATA_H, DATA_W, DATA_H,
               "STG:", formatInt(state.stage), TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, psStage);
    prev.stage = state.stage;
  }

  if (state.skinTemp != prev.skinTemp) {
    uint16_t f, b;
    thresholdColor(state.skinTemp, tempCaution, TFT_DARK_GREEN, TFT_BLACK,
                   tempAlarm, TFT_DARK_GREY, TFT_YELLOW, TFT_WHITE, TFT_RED, f, b);
    printValue(tft, &Roboto_Black_24, 3 * DATA_W, 4 * CAUTWARN_H + DATA_H, DATA_W, DATA_H,
               "Tskin:", formatPerc(state.skinTemp), f, b, TFT_BLACK, psSkinTemp);
    prev.skinTemp = state.skinTemp;
  }

  if (state.ctrlGrp != prev.ctrlGrp) {
    printValue(tft, &Roboto_Black_24, 4 * DATA_W, 4 * CAUTWARN_H + DATA_H, DATA_W, DATA_H,
               "CtrlGrp:", formatInt(state.ctrlGrp), TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, psCtrlGrp);
    prev.ctrlGrp = state.ctrlGrp;
  }

  // --- Threshold crossing alert chirps ---
  // Suppressed on first pass after entering main screen to avoid spurious triggers.
  if (audioEnabled && !firstPassOnMain && state.alt_sl >= ALERT_ALT_THRESHOLD && prev.alt_sl < ALERT_ALT_THRESHOLD)
    audioAlertChirp();
  prev.alt_sl = state.alt_sl;

  if (audioEnabled && !firstPassOnMain && state.vel_surf >= ALERT_VEL_THRESHOLD && prev.vel_surf < ALERT_VEL_THRESHOLD)
    audioAlertChirp();
  prev.vel_surf = state.vel_surf;

  if (audioEnabled && !firstPassOnMain && currentBody.minSafe > 0 &&
      state.apoapsis >= currentBody.minSafe && prev.apoapsis < currentBody.minSafe)
    audioAlertChirp();
  prev.apoapsis = state.apoapsis;

  // Orbit-entry chirp -- fires when the ORBIT bit in vesselSituationState transitions low->high.
  if (audioEnabled && !firstPassOnMain &&
      (state.vesselSituationState & (1 << VSIT_ORBIT)) &&
      !(prev.vesselSituationState & (1 << VSIT_ORBIT)))
    audioAlertChirp();

  firstPassOnMain = false;
}
