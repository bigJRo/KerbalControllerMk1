/***************************************************************************************
   Screen_TGT.ino  —  Target / Rendezvous screen  (RPOD display)

   Replaces Screen_RNDZ.ino. Rename the file and update any references from
   screen_RNDZ / chromeScreen_RNDZ / drawScreen_RNDZ to the TGT equivalents
   (see header changes below).

   LAYOUT (800×480, content area 720×418 below title bar)
   ┌─────────────────────────────────┬──────────────────────────────────────┐
   │                                 │ ALT.SL:         142.8 km             │
   │   RPOD Scope                    │ V.ORB:         2247.3 m/s            │
   │   (black disc, R=130, ±60°)     │ DIST:           48.3 km             │
   │                                 │ V.TGT:          124.0 m/s            │
   │   ◆ target (TFT_VIOLET)         │ BRG:  +22.0°  │ ELV:  -14.0°        │
   │   ○ velocity vector (NEON_GREEN)│ B.ERR:  +14.0° │ E.ERR:  -9.0°      │
   │   + nose crosshair (fixed)      │ T.INT:          6m 30s               │
   └─────────────────────────────────┴──────────────────────────────────────┘

   SCOPE SEMANTICS
   ───────────────
   Fixed crosshair (+) = your nose direction (always at screen centre)
   Violet diamond (TGT) = where the target is relative to your nose
                          Centre → nose is pointed at target
   Green circle (VEL)   = where your velocity vector points relative to target
                          Centre → you are flying directly toward target
   Perfect intercept    = both dots converging toward centre simultaneously

   SCOPE GEOMETRY
   ──────────────
   Centre: (175, 271)   Radius: 130px   Scale: 130/60 = 2.167 px/deg
   Field of view: ±60° (wider than DOCK ±20° — long-range ops need more range)
   Rings: 15°=r54, 30°=r65, 45°=r98, 60°=r130
   Left panel: x=0..349   Right panel: x=352..719   Divider: x=350..351

   RIGHT PANEL — 7 rows, rowHFor(7) = 59px each
   Row 0  Alt.SL   full-width
   Row 1  V.Orb    full-width
   Row 2  Dist     full-width, colour-coded by range (RNDZ_DIST thresholds)
   Row 3  V.Tgt    full-width, colour-coded by speed (TGT_VCLOSURE thresholds)
   Row 4  Brg|Elv  split — raw bearing and elevation to target, informational
   Row 5  Err|Err   split — approach alignment errors (bearing/elevation), colour-coded
   Row 6  T.Int    full-width — estimated intercept time (dist / |vtgt|), closing only

   DOT UPDATE STRATEGY  (same as DOCK)
   ────────────────────
   prevDot stored in screen coords. On change > 1px:
     fillRect(prev - R_ERASE, ..., TFT_BLACK) → _tgtRepairChrome() → draw new dot
   Erase rect is slightly larger than dot to clean up any anti-alias edge.

   CHANGES REQUIRED IN OTHER FILES
   ────────────────────────────────
   1. KCMk1_InfoDisp.h
      - Rename  screen_RNDZ → screen_TGT  (keep value = 4)
      - Replace  extern bool _rndzChromDrawn  →  extern bool _tgtChromDrawn
      - Add to SCREEN_IDS: "TGT"  (slot 4, replacing "TGT" which was already there)
      - Update SCREEN_TITLES slot 4: "TARGET" (was "TARGET" — no change needed)

   2. AAA_Screens.ino / AAA_Screens chrome/draw dispatch
      - case screen_TGT: replace RNDZ calls with TGT calls

   3. AAA_Globals.ino
      - Update contextScreen() tgtDistance auto-switch reference (screen_RNDZ → screen_TGT)

   4. SimpitHandler.ino
      - Update any screen_RNDZ references to screen_TGT

   5. AAA_Config.ino
      - Add TGT_VCLOSURE_WARN_MS and TGT_VCLOSURE_ALARM_MS (values at bottom of this file)
      - Rename section header "FLIGHT THRESHOLDS — TARGET (RNDZ screen)" to "(TGT screen)"

   6. Demo.ino — no changes required; demo already drives tgt* state fields

   7. Delete Screen_RNDZ.ino
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


// ── Chrome state ──────────────────────────────────────────────────────────────────────
bool _tgtChromDrawn = false;


// ── Scope geometry constants — matched to DOCK screen reticle ─────────────────────────
static const int16_t  TGT_SCX    = 192;          // scope centre x  (= DOCK RET_CX)
static const int16_t  TGT_SCY    = 251;          // scope centre y  (= DOCK RET_CY)
static const int16_t  TGT_R      = 170;          // scope radius px (= DOCK RET_R)
static const float    TGT_SCALE  = (float)TGT_R / 60.0f;  // 2.833 px/deg — ±60° full scale

// Ring radii at ±15°, ±30°, ±45°, ±60° — recalculated for R=170, scale=2.833
static const uint16_t TGT_RING_15 =  43;   // 15° × 2.833 = 42.5 → 43
static const uint16_t TGT_RING_30 =  85;   // 30° × 2.833 = 85
static const uint16_t TGT_RING_45 = 128;   // 45° × 2.833 = 127.5 → 128
static const uint16_t TGT_RING_60 = TGT_R; // 60° = full radius

// Dot display sizes — matched to DOCK
static const uint8_t TGT_DOT_R_TGT   = 11;  // target diamond half-size (= DOCK DOT_R_PORT)
static const uint8_t TGT_DOT_R_VEL   =  9;  // velocity circle radius   (= DOCK DOT_R_VEL)
static const uint8_t TGT_DOT_R_ERASE = 16;  // erase rect half-size     (= DOCK DOT_R_ERASE)

// Right panel geometry — matched to DOCK screen
static const uint16_t TGT_RP_X  = 385;   // right panel left edge (= DOCK RP_X)
static const uint16_t TGT_RP_W  = 335;   // right panel width     (= DOCK RP_W)
static const uint8_t  TGT_RP_NR = 7;     // 7 rows × 59px ≈ CONTENT_H
static const tFont   *TGT_RP_LF = &Roboto_Black_20;  // label font (printDispChrome)
static const tFont   *TGT_RP_F  = &Roboto_Black_28;  // value font (printValue)


// ── Previous dot positions (for erase-before-redraw) ─────────────────────────────────
// 9999 = not yet drawn (skip erase on first frame after chrome)
static int16_t _tgtPrevTgtX = 9999, _tgtPrevTgtY = 9999;
static int16_t _tgtPrevVelX = 9999, _tgtPrevVelY = 9999;


// ── Heading error wrap to ±180° ───────────────────────────────────────────────────────
static inline float _tgtWrapErr(float e) {
    while (e >  180.0f) e -= 360.0f;
    while (e < -180.0f) e += 360.0f;
    return e;
}


// ── Clamp dot to within scope boundary ───────────────────────────────────────────────
static void _tgtClampDot(int16_t &sx, int16_t &sy) {
    float dx = sx - TGT_SCX, dy = sy - TGT_SCY;
    float dist = sqrtf(dx*dx + dy*dy);
    float maxR = (float)(TGT_R - TGT_DOT_R_TGT - 2);
    if (dist > maxR && dist > 0.5f) {
        float scale = maxR / dist;
        sx = TGT_SCX + (int16_t)(dx * scale);
        sy = TGT_SCY + (int16_t)(dy * scale);
    }
}


// ── Draw static scope chrome ──────────────────────────────────────────────────────────
static void _tgtDrawScopeChrome(RA8875 &tft) {
    // Black disc background
    tft.fillCircle(TGT_SCX, TGT_SCY, TGT_R, TFT_BLACK);

    // Inner good-zone fill — subtle dark green inside ±15° ring
    tft.fillCircle(TGT_SCX, TGT_SCY, TGT_RING_15, TFT_OFF_BLACK);

    // Concentric range rings — colours match DOCK/MNVR convention
    tft.drawCircle(TGT_SCX, TGT_SCY, TGT_RING_15, TFT_DARK_GREEN);  // ±15° — good zone
    tft.drawCircle(TGT_SCX, TGT_SCY, TGT_RING_30, TFT_DARK_GREY);   // ±30°
    tft.drawCircle(TGT_SCX, TGT_SCY, TGT_RING_45, TFT_DARK_GREY);   // ±45°
    tft.drawCircle(TGT_SCX, TGT_SCY, TGT_RING_60, TFT_GREY);        // ±60° boundary

    // Cardinal lines: full-diameter, split by gap at centre for crosshair
    uint16_t gap = 16, arm = TGT_R - 1;
    tft.drawLine(TGT_SCX, TGT_SCY - arm, TGT_SCX, TGT_SCY - gap, TFT_DARK_GREY);
    tft.drawLine(TGT_SCX, TGT_SCY + gap, TGT_SCX, TGT_SCY + arm, TFT_DARK_GREY);
    tft.drawLine(TGT_SCX - arm, TGT_SCY, TGT_SCX - gap, TGT_SCY, TFT_DARK_GREY);
    tft.drawLine(TGT_SCX + gap, TGT_SCY, TGT_SCX + arm, TGT_SCY, TFT_DARK_GREY);

    // Nose crosshair symbol at centre
    tft.drawLine(TGT_SCX - gap + 2, TGT_SCY, TGT_SCX - 4, TGT_SCY, TFT_GREY);
    tft.drawLine(TGT_SCX + 4,       TGT_SCY, TGT_SCX + gap - 2, TGT_SCY, TFT_GREY);
    tft.drawLine(TGT_SCX, TGT_SCY - gap + 2, TGT_SCX, TGT_SCY - 4, TFT_GREY);
    tft.drawLine(TGT_SCX, TGT_SCY + 4,       TGT_SCX, TGT_SCY + gap - 2, TFT_GREY);
    tft.fillCircle(TGT_SCX, TGT_SCY, 2, TFT_GREY);

    // Minor ticks at 30° increments on outer ring, skipping cardinals
    for (uint16_t deg = 30; deg < 360; deg += 30) {
        if (deg % 90 == 0) continue;
        float rad = (deg - 90) * DEG_TO_RAD;
        int16_t ox = TGT_SCX + (int16_t)(TGT_R * cosf(rad));
        int16_t oy = TGT_SCY + (int16_t)(TGT_R * sinf(rad));
        int16_t ix = TGT_SCX + (int16_t)((TGT_R - 10) * cosf(rad));
        int16_t iy = TGT_SCY + (int16_t)((TGT_R - 10) * sinf(rad));
        tft.drawLine(ox, oy, ix, iy, TFT_DARK_GREY);
    }

    // Bezel ring
    tft.drawCircle(TGT_SCX, TGT_SCY, TGT_R,     TFT_GREY);
    tft.drawCircle(TGT_SCX, TGT_SCY, TGT_R + 1, TFT_DARK_GREY);

    // Ring degree labels — NE quadrant, just inside each ring.
    // Single-arg setTextColor = transparent background (no black rectangle under text).
    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(TFT_LIGHT_GREY);
    tft.setCursor(TGT_SCX + 3, TGT_SCY - TGT_RING_15 + 3);  tft.print("15");
    tft.setCursor(TGT_SCX + 3, TGT_SCY - TGT_RING_30 + 3);  tft.print("30");
    tft.setCursor(TGT_SCX + 3, TGT_SCY - TGT_RING_45 + 3);  tft.print("45");
    tft.setCursor(TGT_SCX + 3, TGT_SCY - TGT_RING_60 + 3);  tft.print("60");

    // Legend — top-left of scope area, above circle edge
    static const uint16_t LEG_X  = 6;
    static const uint16_t LEG_Y0 = TITLE_TOP + 6;
    static const uint16_t LEG_DY = 20;

    tft.setFont(&Roboto_Black_12);

    // VEL — hollow green circle
    tft.drawCircle(LEG_X + 6, LEG_Y0 + 6, 5, TFT_NEON_GREEN);
    tft.setTextColor(TFT_SAP_GREEN, TFT_BLACK);
    tft.setCursor(LEG_X + 16, LEG_Y0);
    tft.print("VEL");

    // TGT — solid violet diamond (same symbol as DOCK PORT dot)
    tft.fillTriangle(LEG_X,      LEG_Y0+LEG_DY+7, LEG_X+12, LEG_Y0+LEG_DY+7,
                     LEG_X+6,    LEG_Y0+LEG_DY+1,  TFT_VIOLET);
    tft.fillTriangle(LEG_X,      LEG_Y0+LEG_DY+7, LEG_X+12, LEG_Y0+LEG_DY+7,
                     LEG_X+6,    LEG_Y0+LEG_DY+13, TFT_VIOLET);
    tft.setTextColor(TFT_VIOLET, TFT_BLACK);
    tft.setCursor(LEG_X + 16, LEG_Y0 + LEG_DY);
    tft.print("TGT");

    // NOSE — crosshair symbol matching the scope centre
    tft.drawLine(LEG_X+1,  LEG_Y0+LEG_DY*2+6,  LEG_X+4,  LEG_Y0+LEG_DY*2+6,  TFT_GREY);
    tft.drawLine(LEG_X+8,  LEG_Y0+LEG_DY*2+6,  LEG_X+11, LEG_Y0+LEG_DY*2+6,  TFT_GREY);
    tft.drawLine(LEG_X+6,  LEG_Y0+LEG_DY*2+1,  LEG_X+6,  LEG_Y0+LEG_DY*2+4,  TFT_GREY);
    tft.drawLine(LEG_X+6,  LEG_Y0+LEG_DY*2+8,  LEG_X+6,  LEG_Y0+LEG_DY*2+11, TFT_GREY);
    tft.fillCircle(LEG_X+6, LEG_Y0+LEG_DY*2+6, 1, TFT_GREY);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(LEG_X + 16, LEG_Y0 + LEG_DY * 2);
    tft.print("NOSE");
}


// ── Repair scope chrome after dot erase ───────────────────────────────────────────────
// After a fillRect erase, any rings or crosshair lines that intersected the
// erase box must be redrawn. Logic mirrors Screen_DOCK._dockRepairChrome().
static void _tgtRepairChrome(RA8875 &tft, int16_t bx, int16_t by, uint8_t bh) {
    int16_t boxX0 = bx, boxX1 = bx + 2*bh, boxY0 = by, boxY1 = by + 2*bh;

    float cx = (float)constrain((int)TGT_SCX, (int)boxX0, (int)boxX1);
    float cy = (float)constrain((int)TGT_SCY, (int)boxY0, (int)boxY1);
    float distToCentre = sqrtf((cx-TGT_SCX)*(cx-TGT_SCX) + (cy-TGT_SCY)*(cy-TGT_SCY));
    float dx = fmaxf(fabsf(boxX0-TGT_SCX), fabsf(boxX1-TGT_SCX));
    float dy = fmaxf(fabsf(boxY0-TGT_SCY), fabsf(boxY1-TGT_SCY));
    float distFar = sqrtf(dx*dx + dy*dy);

    static const uint16_t rings[] = {TGT_RING_15, TGT_RING_30, TGT_RING_45, TGT_RING_60};
    static const uint16_t rcols[] = {TFT_DARK_GREEN, TFT_DARK_GREY, TFT_DARK_GREY, TFT_GREY};
    for (uint8_t i = 0; i < 4; i++) {
        float r = (float)rings[i];
        if (distToCentre <= r + 1.5f && distFar >= r - 1.5f)
            tft.drawCircle(TGT_SCX, TGT_SCY, rings[i], rcols[i]);
    }

    static const uint16_t cgap = 16, carm = TGT_R - 1;
    if (boxY0 <= TGT_SCY && TGT_SCY <= boxY1) {
        if (boxX0 < (int16_t)(TGT_SCX - cgap))
            tft.drawLine(TGT_SCX - carm, TGT_SCY, TGT_SCX - cgap, TGT_SCY, TFT_DARK_GREY);
        if (boxX1 > (int16_t)(TGT_SCX + cgap))
            tft.drawLine(TGT_SCX + cgap, TGT_SCY, TGT_SCX + carm, TGT_SCY, TFT_DARK_GREY);
        if (boxX0 <= TGT_SCX || boxX1 >= TGT_SCX) {
            tft.drawLine(TGT_SCX - cgap + 2, TGT_SCY, TGT_SCX - 4, TGT_SCY, TFT_GREY);
            tft.drawLine(TGT_SCX + 4, TGT_SCY, TGT_SCX + cgap - 2, TGT_SCY, TFT_GREY);
        }
    }
    if (boxX0 <= TGT_SCX && TGT_SCX <= boxX1) {
        if (boxY0 < (int16_t)(TGT_SCY - cgap))
            tft.drawLine(TGT_SCX, TGT_SCY - carm, TGT_SCX, TGT_SCY - cgap, TFT_DARK_GREY);
        if (boxY1 > (int16_t)(TGT_SCY + cgap))
            tft.drawLine(TGT_SCX, TGT_SCY + cgap, TGT_SCX, TGT_SCY + carm, TFT_DARK_GREY);
        if (boxY0 <= TGT_SCY || boxY1 >= TGT_SCY) {
            tft.drawLine(TGT_SCX, TGT_SCY - cgap + 2, TGT_SCX, TGT_SCY - 4, TFT_GREY);
            tft.drawLine(TGT_SCX, TGT_SCY + 4, TGT_SCX, TGT_SCY + cgap - 2, TFT_GREY);
        }
    }
    if (boxX0 <= TGT_SCX && TGT_SCX <= boxX1 && boxY0 <= TGT_SCY && TGT_SCY <= boxY1)
        tft.fillCircle(TGT_SCX, TGT_SCY, 2, TFT_GREY);

    // Restore inner good-zone fill if erased
    if (distToCentre <= TGT_RING_15 - 1) {
        tft.fillCircle(TGT_SCX, TGT_SCY, TGT_RING_15 - 1, TFT_OFF_BLACK);
        tft.drawCircle(TGT_SCX, TGT_SCY, TGT_RING_15, TFT_DARK_GREEN);
    }

    // Redraw ring labels if the erase box overlaps their NE quadrant positions.
    // Each label sits at (SCX+3, SCY - RING_r + 3) and is ~14px wide × 12px tall.
    // Use a generous bounding check: any ring whose label box intersects the erase rect.
    static const uint16_t lblR[]  = {TGT_RING_15, TGT_RING_30, TGT_RING_45, TGT_RING_60};
    static const char    *lblTxt[]= {"15", "30", "45", "60"};
    bool needLabel = false;
    for (uint8_t i = 0; i < 4; i++) {
        int16_t lx = TGT_SCX + 3, ly = TGT_SCY - lblR[i] + 3;
        // Label box: lx..lx+18, ly..ly+12
        if (boxX1 >= lx && boxX0 <= lx + 18 && boxY1 >= ly && boxY0 <= ly + 12) {
            needLabel = true; break;
        }
    }
    if (needLabel) {
        tft.setFont(&Roboto_Black_12);
        tft.setTextColor(TFT_LIGHT_GREY);
        for (uint8_t i = 0; i < 4; i++) {
            tft.setCursor(TGT_SCX + 3, TGT_SCY - lblR[i] + 3);
            tft.print(lblTxt[i]);
        }
    }
}


// ── Update scope dots — erase old, repair chrome, draw new ───────────────────────────
// TGT dot:  solid violet diamond — where the target is relative to nose
// VEL dot:  hollow neon-green circle — where velocity vector points relative to target
//
// Angular convention (matches DOCK):
//   tgtSX = SCX + (-bearingErr × SCALE)   — positive bearing → dot left of centre
//   tgtSY = SCY + (  elevErr   × SCALE)   — positive elevation → dot above centre
static void _tgtUpdateDots(RA8875 &tft, float tgtBrg, float tgtElv,
                                        float velBrg, float velElv) {
    // TGT dot: where is the target relative to your nose?
    int16_t tSX = TGT_SCX + (int16_t)(-tgtBrg * TGT_SCALE);
    int16_t tSY = TGT_SCY + (int16_t)( tgtElv * TGT_SCALE);
    _tgtClampDot(tSX, tSY);

    // VEL dot: where is your velocity vector relative to the target bearing?
    int16_t vSX = TGT_SCX + (int16_t)(-velBrg * TGT_SCALE);
    int16_t vSY = TGT_SCY + (int16_t)( velElv * TGT_SCALE);
    _tgtClampDot(vSX, vSY);

    const uint8_t EH = TGT_DOT_R_ERASE;

    // ── TGT dot (solid violet diamond) ───────────────────────────────────────────────
    bool tgtMoved = (_tgtPrevTgtX == 9999 ||
                     abs(tSX - _tgtPrevTgtX) > 1 ||
                     abs(tSY - _tgtPrevTgtY) > 1);
    if (tgtMoved) {
        if (_tgtPrevTgtX != 9999) {
            tft.fillRect(_tgtPrevTgtX - EH, _tgtPrevTgtY - EH, EH*2+1, EH*2+1, TFT_BLACK);
            _tgtRepairChrome(tft, _tgtPrevTgtX - EH, _tgtPrevTgtY - EH, EH);
        }
        uint8_t ds = TGT_DOT_R_TGT + 3;
        tft.fillTriangle(tSX-ds, tSY, tSX+ds, tSY, tSX, tSY-ds, TFT_VIOLET);
        tft.fillTriangle(tSX-ds, tSY, tSX+ds, tSY, tSX, tSY+ds, TFT_VIOLET);
        _tgtPrevTgtX = tSX; _tgtPrevTgtY = tSY;
    }

    // ── VEL dot (hollow neon-green circle) ────────────────────────────────────────────
    bool velMoved = (_tgtPrevVelX == 9999 ||
                     abs(vSX - _tgtPrevVelX) > 1 ||
                     abs(vSY - _tgtPrevVelY) > 1);
    if (velMoved) {
        if (_tgtPrevVelX != 9999) {
            tft.fillRect(_tgtPrevVelX - EH, _tgtPrevVelY - EH, EH*2+1, EH*2+1, TFT_BLACK);
            _tgtRepairChrome(tft, _tgtPrevVelX - EH, _tgtPrevVelY - EH, EH);
        }
        tft.drawCircle(vSX, vSY, TGT_DOT_R_VEL,     TFT_NEON_GREEN);
        tft.drawCircle(vSX, vSY, TGT_DOT_R_VEL + 1, TFT_SAP_GREEN);
        _tgtPrevVelX = vSX; _tgtPrevVelY = vSY;
    }

    // Always redraw VEL dot on top — ensures it is never buried under TGT diamond
    tft.drawCircle(vSX, vSY, TGT_DOT_R_VEL,     TFT_NEON_GREEN);
    tft.drawCircle(vSX, vSY, TGT_DOT_R_VEL + 1, TFT_SAP_GREEN);

    // Redraw crosshair inner segments — VEL circle can clip them near centre
    {
        static const uint16_t g = 16;
        tft.drawLine(TGT_SCX - g + 2, TGT_SCY, TGT_SCX - 4, TGT_SCY, TFT_GREY);
        tft.drawLine(TGT_SCX + 4,     TGT_SCY, TGT_SCX + g - 2, TGT_SCY, TFT_GREY);
        tft.drawLine(TGT_SCX, TGT_SCY - g + 2, TGT_SCX, TGT_SCY - 4, TFT_GREY);
        tft.drawLine(TGT_SCX, TGT_SCY + 4,     TGT_SCX, TGT_SCY + g - 2, TFT_GREY);
    }
    tft.fillCircle(TGT_SCX, TGT_SCY, 2, TFT_GREY);
}


// ── CHROME ────────────────────────────────────────────────────────────────────────────
static void chromeScreen_TGT(RA8875 &tft) {
    if (!state.targetAvailable) {
        _tgtChromDrawn = false;
        tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
        textCenter(tft, &Roboto_Black_72, 0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP,
                   "NO TARGET SET", TFT_WHITE, TFT_RED);
        return;
    }

    _tgtChromDrawn = true;

    // Reset dot positions — first draw must not try to erase stale coordinates
    _tgtPrevTgtX = 9999; _tgtPrevTgtY = 9999;
    _tgtPrevVelX = 9999; _tgtPrevVelY = 9999;

    // Left panel: clear and draw scope chrome
    tft.fillRect(0, TITLE_TOP, TGT_RP_X, SCREEN_H - TITLE_TOP, TFT_BLACK);
    _tgtDrawScopeChrome(tft);

    // Panel divider
    tft.drawLine(TGT_RP_X - 2, TITLE_TOP, TGT_RP_X - 2, SCREEN_H, TFT_GREY);
    tft.drawLine(TGT_RP_X - 1, TITLE_TOP, TGT_RP_X - 1, SCREEN_H, TFT_GREY);

    // Right panel chrome — 7 rows
    const tFont *F   = TGT_RP_LF;   // label font for printDispChrome
    const uint8_t NR = TGT_RP_NR;
    uint16_t rowH    = rowHFor(NR);
    uint16_t HW      = TGT_RP_W / 2;

    // Rows 0–3: full-width labels
    printDispChrome(tft, F, TGT_RP_X, rowYFor(0,NR), TGT_RP_W, rowH, "Alt.SL:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, TGT_RP_X, rowYFor(1,NR), TGT_RP_W, rowH, "V.Orb:",  COL_LABEL, COL_BACK, COL_NO_BDR);

    // Divider between V.Orb (row 1) and Dist (row 2)
    {
        uint16_t dy = rowYFor(2,NR) - 1;
        tft.drawLine(TGT_RP_X, dy,   TGT_RP_X + TGT_RP_W, dy,   TFT_GREY);
        tft.drawLine(TGT_RP_X, dy+1, TGT_RP_X + TGT_RP_W, dy+1, TFT_GREY);
    }

    printDispChrome(tft, F, TGT_RP_X, rowYFor(2,NR), TGT_RP_W, rowH, "Dist:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, TGT_RP_X, rowYFor(3,NR), TGT_RP_W, rowH, "V.Tgt:",  COL_LABEL, COL_BACK, COL_NO_BDR);

    // Row 4: Brg | Elv split
    {
        uint16_t y = rowYFor(4, NR), h = rowH;
        printDispChrome(tft, F, TGT_RP_X,        y, HW - ROW_PAD, h, "Brg:", COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, F, TGT_RP_X + HW,   y, HW - ROW_PAD, h, "Elv:", COL_LABEL, COL_BACK, COL_NO_BDR);
        for (int8_t dx = -1; dx <= 1; dx++)
            tft.drawLine(TGT_RP_X + HW + dx, y, TGT_RP_X + HW + dx, y + h - 1, TFT_GREY);
    }

    // Row 5: Err | Err split (approach alignment errors)
    {
        uint16_t y = rowYFor(5, NR), h = rowH;
        printDispChrome(tft, F, TGT_RP_X,        y, HW - ROW_PAD, h, "Err:", COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, F, TGT_RP_X + HW,   y, HW - ROW_PAD, h, "Err:", COL_LABEL, COL_BACK, COL_NO_BDR);
        for (int8_t dx = -1; dx <= 1; dx++)
            tft.drawLine(TGT_RP_X + HW + dx, y, TGT_RP_X + HW + dx, y + h - 1, TFT_GREY);
    }

    // Row 6: T.Int full-width
    printDispChrome(tft, F, TGT_RP_X, rowYFor(6,NR), TGT_RP_W, rowH, "T.Int:", COL_LABEL, COL_BACK, COL_NO_BDR);

    // Horizontal dividers between rows 3/4, 5/6
    {
        uint16_t d1 = rowYFor(4,NR) - 1;
        tft.drawLine(TGT_RP_X, d1,   TGT_RP_X + TGT_RP_W, d1,   TFT_GREY);
        tft.drawLine(TGT_RP_X, d1+1, TGT_RP_X + TGT_RP_W, d1+1, TFT_GREY);
        uint16_t d2 = rowYFor(6,NR) - 1;
        tft.drawLine(TGT_RP_X, d2,   TGT_RP_X + TGT_RP_W, d2,   TFT_GREY);
        tft.drawLine(TGT_RP_X, d2+1, TGT_RP_X + TGT_RP_W, d2+1, TFT_GREY);
    }

    // Invalidate value caches — forces full redraw on first update pass
    for (uint8_t r = 0; r < ROW_COUNT; r++) rowCache[screen_TGT][r].value = "\x01";
    for (uint8_t r = 0; r < ROW_COUNT; r++) printState[screen_TGT][r] = PrintState{};
}


// ── DRAW (called every loop) ──────────────────────────────────────────────────────────
static void drawScreen_TGT(RA8875 &tft) {

    // State transitions
    if (!state.targetAvailable) {
        if (_tgtChromDrawn) { _tgtChromDrawn = false; switchToScreen(screen_TGT); }
        return;
    }
    if (!_tgtChromDrawn) { switchToScreen(screen_TGT); return; }

    // ── Derived values ────────────────────────────────────────────────────────────────

    // Nose-to-target errors: where is the target relative to your nose?
    float tgtBrg = _tgtWrapErr(state.heading   - state.tgtHeading);
    float tgtElv = state.pitch - state.tgtPitch;

    // Velocity-to-target errors: is your velocity vector pointed at the target?
    float velBrg = _tgtWrapErr(state.tgtHeading - state.tgtVelHeading);
    float velElv = state.tgtPitch - state.tgtVelPitch;

    // ── Update scope dots ─────────────────────────────────────────────────────────────
    _tgtUpdateDots(tft, tgtBrg, tgtElv, velBrg, velElv);

    // ── Right panel values ────────────────────────────────────────────────────────────
    const uint8_t NR = TGT_RP_NR;
    const uint16_t HW = TGT_RP_W / 2;
    uint16_t fg, bg;
    char buf[16];

    // Cache-checked draw helper — full-width right panel rows
    auto tgtVal = [&](uint8_t row, uint8_t slot, const char *label, const String &val,
                      uint16_t fgc, uint16_t bgc) {
        RowCache &rc = rowCache[screen_TGT][slot];
        if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
        printValue(tft, TGT_RP_F,
                   TGT_RP_X, rowYFor(row, NR), TGT_RP_W, rowHFor(NR),
                   label, val, fgc, bgc, COL_BACK,
                   printState[screen_TGT][slot]);
        rc.value = val; rc.fg = fgc; rc.bg = bgc;
    };

    // Cache-checked draw helper — half-width split rows, independent cache slots
    auto tgtValH = [&](uint8_t row, uint8_t slot, uint16_t x,
                       const char *label, const String &val,
                       uint16_t fgc, uint16_t bgc) {
        RowCache &rc = rowCache[screen_TGT][slot];
        if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
        printValue(tft, TGT_RP_F,
                   x, rowYFor(row, NR), HW - ROW_PAD, rowHFor(NR),
                   label, val, fgc, bgc, COL_BACK,
                   printState[screen_TGT][slot]);
        rc.value = val; rc.fg = fgc; rc.bg = bgc;
    };

    // Row 0 — Alt.SL  (cache slot 0)
    fg = (state.altitude < 0.0f) ? TFT_RED : TFT_DARK_GREEN;
    tgtVal(0, 0, "Alt.SL:", formatAlt(state.altitude), fg, TFT_BLACK);

    // Row 1 — V.Orb  (cache slot 1)
    tgtVal(1, 1, "V.Orb:", fmtMs(state.orbitalVel), TFT_DARK_GREEN, TFT_BLACK);

    // Row 2 — Dist  (cache slot 2)
    // White-on-green < DOCK_DIST_WARN_M: switch to DOCK screen.
    // Yellow < RNDZ_DIST_WARN_M: closing, pay attention.
    // Dark green otherwise: nominal long range.
    if      (state.tgtDistance < DOCK_DIST_WARN_M)  { fg = TFT_WHITE;      bg = TFT_DARK_GREEN; }
    else if (state.tgtDistance < RNDZ_DIST_WARN_M)  { fg = TFT_YELLOW;     bg = TFT_BLACK;      }
    else                                             { fg = TFT_DARK_GREEN; bg = TFT_BLACK;      }
    tgtVal(2, 2, "Dist:", formatAlt(state.tgtDistance), fg, bg);

    // Row 3 — V.Tgt  (cache slot 3)
    // Speed of closure. Negative = closing (good), positive = opening (bad).
    // Alarm: fast closure at short range. Warn: moderate. Nominal: dark green.
    {
        float vc  = state.tgtVelocity;
        float avc = fabsf(vc);
        bool closing = (vc < 0.0f);
        bool tooFast = (avc > TGT_VCLOSURE_ALARM_MS &&
                        state.tgtDistance < RNDZ_DIST_WARN_M);
        if (tooFast)       { fg = TFT_WHITE;  bg = TFT_RED;   }
        else if (!closing) { fg = TFT_YELLOW; bg = TFT_BLACK; }
        else               { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        tgtVal(3, 3, "V.Tgt:", fmtMs(vc), fg, bg);
    }

    // Row 4 — Brg | Elv  (cache slots 4, 5)
    // Raw bearing and elevation to target — informational, always dark green.
    // Display bearing as a signed value: wrap 0–360 to –180..+180 so
    // positive = target to the right, negative = target to the left.
    {
        float dispBrg = state.tgtHeading;
        if (dispBrg > 180.0f) dispBrg -= 360.0f;
        snprintf(buf, sizeof(buf), "%+.0f\xB0", dispBrg);
        tgtValH(4, 4, TGT_RP_X, "Brg:", String(buf), TFT_DARK_GREEN, TFT_BLACK);

        snprintf(buf, sizeof(buf), "%+.0f\xB0", state.tgtPitch);
        tgtValH(4, 5, TGT_RP_X + HW, "Elv:", String(buf), TFT_DARK_GREEN, TFT_BLACK);
    }

    // Row 5 — B.Err | E.Err  (cache slots 6, 7)
    // Approach alignment errors. Green < 5°, yellow < 15°, white-on-red >= 15°.
    // Uses TGT-specific thresholds (wider than DOCK — long-range ops are less precise).
    auto errColor = [](float ae, uint16_t &fg, uint16_t &bg) {
        if      (ae >= TGT_BRG_ALARM_DEG) { fg = TFT_WHITE;      bg = TFT_RED;   }
        else if (ae >= TGT_BRG_WARN_DEG)  { fg = TFT_YELLOW;     bg = TFT_BLACK; }
        else                               { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    };
    {
        float brgErr = _tgtWrapErr(state.tgtHeading - state.tgtVelHeading);
        float elvErr = state.tgtPitch - state.tgtVelPitch;

        errColor(fabsf(brgErr), fg, bg);
        snprintf(buf, sizeof(buf), "%+.0f\xB0", brgErr);
        tgtValH(5, 6, TGT_RP_X, "Err:", String(buf), fg, bg);

        errColor(fabsf(elvErr), fg, bg);
        snprintf(buf, sizeof(buf), "%+.0f\xB0", elvErr);
        tgtValH(5, 7, TGT_RP_X + HW, "Err:", String(buf), fg, bg);
    }

    // Row 6 — T.Int  (cache slot 8)
    // Estimated intercept time = distance / |closure rate|.
    // Only meaningful when closing; shows "---" otherwise.
    {
        bool closing = (state.tgtVelocity < -0.5f);
        String tIntStr;
        if (closing) {
            float t = state.tgtDistance / fabsf(state.tgtVelocity);
            tIntStr = formatTime((int64_t)t);
            fg = TFT_DARK_GREEN; bg = TFT_BLACK;
        } else {
            tIntStr = "---";
            fg = TFT_DARK_GREY; bg = TFT_BLACK;
        }
        tgtVal(6, 8, "T.Int:", tIntStr, fg, bg);
    }

    // ── Redraw dividers — printValue fillRect can overwrite them ─────────────────────
    {
        uint16_t d0 = rowYFor(2,NR) - 1;  // V.Orb → Dist
        tft.drawLine(TGT_RP_X, d0,   TGT_RP_X + TGT_RP_W, d0,   TFT_GREY);
        tft.drawLine(TGT_RP_X, d0+1, TGT_RP_X + TGT_RP_W, d0+1, TFT_GREY);
        uint16_t d1 = rowYFor(4,NR) - 1;  // V.Tgt → Brg|Elv
        tft.drawLine(TGT_RP_X, d1,   TGT_RP_X + TGT_RP_W, d1,   TFT_GREY);
        tft.drawLine(TGT_RP_X, d1+1, TGT_RP_X + TGT_RP_W, d1+1, TFT_GREY);
        uint16_t d2 = rowYFor(6,NR) - 1;
        tft.drawLine(TGT_RP_X, d2,   TGT_RP_X + TGT_RP_W, d2,   TFT_GREY);
        tft.drawLine(TGT_RP_X, d2+1, TGT_RP_X + TGT_RP_W, d2+1, TFT_GREY);
    }
    // Split-row vertical dividers
    for (uint8_t row = 4; row <= 5; row++) {
        uint16_t y = rowYFor(row, NR), h = rowHFor(NR);
        for (int8_t dx = -1; dx <= 1; dx++)
            tft.drawLine(TGT_RP_X + HW + dx, y, TGT_RP_X + HW + dx, y + h - 1, TFT_GREY);
    }
    // Panel left border
    tft.drawLine(TGT_RP_X - 2, TITLE_TOP, TGT_RP_X - 2, SCREEN_H, TFT_GREY);
    tft.drawLine(TGT_RP_X - 1, TITLE_TOP, TGT_RP_X - 1, SCREEN_H, TFT_GREY);
}


/***************************************************************************************
   AAA_Config.ino additions required
   Add these constants to the "FLIGHT THRESHOLDS — TARGET" section:

   // Closure velocity thresholds (m/s, absolute value)
   const float TGT_VCLOSURE_WARN_MS  = 200.0f;  // yellow — fast approach
   const float TGT_VCLOSURE_ALARM_MS = 500.0f;  // white-on-red — very fast

   // Approach alignment error thresholds (degrees absolute)
   // Wider than DOCK thresholds — long-range ops tolerate larger angles
   const float TGT_BRG_WARN_DEG  = 5.0f;   // yellow — off-axis approach
   const float TGT_BRG_ALARM_DEG = 15.0f;  // white-on-red — significantly misaligned

   KCMk1_InfoDisp.h additions required:
   - extern bool _tgtChromDrawn;   // replaces extern bool _rndzChromDrawn
   - Rename screen_RNDZ → screen_TGT in the ScreenType enum (keep value = 4)
   - Update SCREEN_TITLES[4] to "TARGET" (already correct if unchanged)
   - Update SCREEN_IDS[4]   to "TGT"    (already correct if unchanged)
****************************************************************************************/
