/***************************************************************************************
   Screen_RNDZ.ino -- Target screen — chromeScreen_RNDZ, drawScreen_RNDZ
   Shows target bearing, approach alignment, and relative state for rendezvous.
   Complements the DOCK screen: TARGET is used at range, DOCK for final approach.
   rowCache index: [5] (screen_RNDZ = 5)

   Layout:
     STATE (rows 0-1): Alt.SL, V.Orb
     TGT   (rows 2-3): Dist, V.Tgt
     BRG   (rows 4-5): Brg (raw bearing to target), Elev (raw elevation to target) — dark green
     APCH  (rows 6-7): Brg.Err (bearing error vs velocity), Elv.Err (elevation error)

   Dist colouring:
     White-on-green < 200m  — final approach range, consider switching to DOCK
     Yellow          < 5km  — closing
     Dark-green      ≥ 5km  — nominal

   APCH errors (target bearing minus velocity bearing, wrapped ±180°):
     Green  < DOCK_BRG_WARN_DEG  — well aligned
     Yellow < DOCK_BRG_ALARM_DEG — slightly off
     White-on-red ≥ DOCK_BRG_ALARM_DEG — significantly misaligned
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


bool _rndzChromDrawn = false;

static void chromeScreen_RNDZ(RA8875 &tft) {
  if (state.targetAvailable) {
    _rndzChromDrawn = true;
    static const tFont   *F      = &Roboto_Black_40;
    static const uint8_t  NR     = 8;
    static const uint16_t SECT_W = 26;
    static const uint16_t AX     = ROW_PAD + SECT_W;
    static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
    uint16_t rowH = rowHFor(NR);

    // Section labels: STATE(0-1), TGT(2-3), BRG(4-5), APCH(6-7)
    drawVerticalText(tft, 0, TITLE_TOP,          SECT_W, rowH*2, &Roboto_Black_16, "STATE", TFT_LIGHT_GREY, TFT_BLACK);
    drawVerticalText(tft, 0, TITLE_TOP + rowH*2, SECT_W, rowH*2, &Roboto_Black_16, "TGT",   TFT_LIGHT_GREY, TFT_BLACK);
    drawVerticalText(tft, 0, TITLE_TOP + rowH*4, SECT_W, rowH*2, &Roboto_Black_16, "BRG",   TFT_LIGHT_GREY, TFT_BLACK);
    drawVerticalText(tft, 0, TITLE_TOP + rowH*6, SECT_W, rowH*2, &Roboto_Black_16, "APCH",  TFT_LIGHT_GREY, TFT_BLACK);

    printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, "Alt.SL:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, "V.Orb:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "Dist:",    COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH, "V.Tgt:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(4,NR), AW, rowH, "Brg:",     COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(5,NR), AW, rowH, "Elv:",    COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(6,NR), AW, rowH, "Brg.Err:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(7,NR), AW, rowH, "Elv.Err:", COL_LABEL, COL_BACK, COL_NO_BDR);

    // Dividers — drawn LAST, +1 offset
    uint16_t d1 = TITLE_TOP + rowH*2 + 1;  // after STATE
    uint16_t d2 = TITLE_TOP + rowH*4 + 1;  // after TGT
    uint16_t d3 = TITLE_TOP + rowH*6 + 1;  // after BRG
    tft.drawLine(0, d1,   CONTENT_W, d1,   TFT_GREY);
    tft.drawLine(0, d1+1, CONTENT_W, d1+1, TFT_GREY);
    tft.drawLine(0, d2,   CONTENT_W, d2,   TFT_GREY);
    tft.drawLine(0, d2+1, CONTENT_W, d2+1, TFT_GREY);
    tft.drawLine(0, d3,   CONTENT_W, d3,   TFT_GREY);
    tft.drawLine(0, d3+1, CONTENT_W, d3+1, TFT_GREY);

  } else {
    _rndzChromDrawn = false;
    tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
    textCenter(tft, &Roboto_Black_72, 0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP,
               "NO TARGET SET", TFT_WHITE, TFT_RED);
  }
}


static void drawScreen_RNDZ(RA8875 &tft) {

  if (!state.targetAvailable) {
    if (_rndzChromDrawn) {
      _rndzChromDrawn = false;
      switchToScreen(screen_RNDZ);
    }
    return;
  }

  if (!_rndzChromDrawn) {
    switchToScreen(screen_RNDZ);
    return;
  }

  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  uint16_t fg, bg;
  char buf[16];

  auto rndzVal = [&](uint8_t row, const char *label, const String &val,
                     uint16_t fgc, uint16_t bgc) {
    RowCache &rc = rowCache[4][row];
    if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
    printValue(tft, F, AX, rowYFor(row, NR), AW, rowHFor(NR),
               label, val, fgc, bgc, COL_BACK, printState[4][row]);
    rc.value = val; rc.fg = fgc; rc.bg = bgc;
  };

  // ── STATE block (rows 0-1) ──

  fg = (state.altitude < 0) ? TFT_RED : TFT_DARK_GREEN;
  rndzVal(0, "Alt.SL:", formatAlt(state.altitude), fg, TFT_BLACK);

  fg = (state.orbitalVel < 0) ? TFT_RED : TFT_DARK_GREEN;
  rndzVal(1, "V.Orb:", fmtMs(state.orbitalVel), fg, TFT_BLACK);

  // ── TGT block (rows 2-3) ──

  // Row 2 — Distance: white-on-green inside 200m (ready for DOCK), yellow <5km, green
  // The white-on-green cue advises the pilot to consider switching to the DOCK screen.
  if      (state.tgtDistance < DOCK_DIST_WARN_M)  { fg = TFT_WHITE;     bg = TFT_DARK_GREEN; }
  else if (state.tgtDistance < RNDZ_DIST_WARN_M)  { fg = TFT_YELLOW;    bg = TFT_BLACK;      }
  else                                             { fg = TFT_DARK_GREEN; bg = TFT_BLACK;      }
  rndzVal(2, "Dist:", formatAlt(state.tgtDistance), fg, bg);

  // Row 3 — Relative velocity magnitude (always positive in KSP1 — informational)
  rndzVal(3, "V.Tgt:", fmtMs(state.tgtVelocity), TFT_DARK_GREEN, TFT_BLACK);

  // ── BRG block (rows 4-5): raw bearing and elevation to target ──

  // Row 4 — Bearing to target (informational — reference only)
  snprintf(buf, sizeof(buf), "%.1f\xB0", state.tgtHeading);
  rndzVal(4, "Brg:", buf, TFT_DARK_GREEN, TFT_BLACK);

  // Row 5 — Elevation to target (informational — reference only)
  snprintf(buf, sizeof(buf), "%.1f\xB0", state.tgtPitch);
  rndzVal(5, "Elv:", buf, TFT_DARK_GREEN, TFT_BLACK);

  // ── APCH block (rows 6-7): approach angle errors ──
  // Brg.Err = target bearing minus velocity bearing (are we flying toward the target?)
  // Elv.Err = target elevation minus velocity elevation
  // Wrapped to ±180°. Green <WARN, yellow <ALARM, white-on-red ≥ALARM.
  // Same thresholds as DOCK screen so the conventions are consistent.
  {
    float brgErr = state.tgtHeading - state.tgtVelHeading;
    if (brgErr >  180.0f) brgErr -= 360.0f;
    if (brgErr < -180.0f) brgErr += 360.0f;

    float elvErr = state.tgtPitch - state.tgtVelPitch;

    auto apchColor = [](float e, uint16_t &fg, uint16_t &bg) {
      float ae = fabsf(e);
      if      (ae >= DOCK_BRG_ALARM_DEG) { fg = TFT_WHITE;     bg = TFT_RED;   }
      else if (ae >= DOCK_BRG_WARN_DEG)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
      else                               { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    };

    apchColor(brgErr, fg, bg);
    snprintf(buf, sizeof(buf), "%+.1f\xB0", brgErr);
    rndzVal(6, "Brg.Err:", buf, fg, bg);

    apchColor(elvErr, fg, bg);
    snprintf(buf, sizeof(buf), "%+.1f\xB0", elvErr);
    rndzVal(7, "Elv.Err:", buf, fg, bg);
  }
}
