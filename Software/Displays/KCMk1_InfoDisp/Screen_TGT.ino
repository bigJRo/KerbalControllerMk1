/***************************************************************************************
   Screen_TGT.ino  —  Target / Rendezvous screen  (RPOD display)

   Replaces Screen_RNDZ.ino. Rename the file and update any references from
   screen_RNDZ / chromeScreen_RNDZ / drawScreen_RNDZ to the TGT equivalents
   (see header changes below).

   LAYOUT (1024×600, content area 940×538 below title bar; reticle R=210)
   ┌─────────────────────────────────┬──────────────────────────────────────┐
   │                                 │ ALT.SL:         142.8 km             │
   │   RPOD Scope                    │ V.ORB:         2247.3 m/s            │
   │   (black disc, R=130, ±60°)     │ DIST:           48.3 km             │
   │                                 │ V.TGT:          124.0 m/s            │
   │   ◆ target (TFT_VIOLET)         │ BRG:  +22.0°  │ ELV:  -14.0°        │
   │   ○ velocity vector (NEON_GREEN)│ B.ERR:  +14.0° │ E.ERR:  -9.0°      │
   │   + nose crosshair (fixed)      │ T+INT:          6m 30s               │
   └─────────────────────────────────┴──────────────────────────────────────┘

   SCOPE SEMANTICS
   ───────────────
   Fixed crosshair (+) = your nose direction (always at screen centre)
   Violet diamond (TGT) = where the target is relative to your nose
                          Centre → nose is pointed at target
   Green circle (VEL)   = where your velocity vector points relative to target
                          Centre → you are flying directly toward target
   Perfect intercept    = both dots converging toward centre simultaneously

   SCOPE GEOMETRY (rev-2, 1024×600)
   ──────────────
   Centre: (289, 300)   Radius: 210px   Scale: 3.5 px/deg (±60° full scale)
   Field of view: ±60° (wider than DOCK ±20° — long-range ops need more range)
   Rings: 15°=r52, 30°=r105, 45°=r157, 60°=r210. Centre/radius match the
   MNVR and DOCK reticles.
   Right panel: 360 px at x=580 (matches MNVR/DOCK), labels 28 / values 36

   RIGHT PANEL — 7 rows, rowHFor(7) = 59px each
   Row 0  Alt.SL   full-width
   Row 1  V.Orb    full-width
   Row 2  Dist     full-width, colour-coded by range (RNDZ_DIST thresholds)
   Row 3  V.Tgt    full-width, colour-coded by speed (TGT_VCLOSURE thresholds)
   Row 4  Brg|Elv  split — raw bearing and elevation to target, informational
   Row 5  Err|Err   split — approach alignment errors (bearing/elevation), colour-coded
   Row 6  T+Int    full-width — estimated intercept time (dist / |vtgt|), closing only

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


// ── Scope geometry — same centre/radius as the MNVR and DOCK reticles ─────────────────
// The readout panel sits on the far right (x=580), leaving x=[0,578] for the
// scope. Centre and radius match MNVR/DOCK exactly for a consistent family look.
static const int16_t  TGT_SCX    = RETICLE_CX;   // centre of the left region x=[0,578]
static const int16_t  TGT_SCY    = RETICLE_CY;   // = MNVR/DOCK reticle centre y
static const int16_t  TGT_R      = RETICLE_R;    // = MNVR/DOCK reticle radius
static const float    TGT_SCALE  = (float)TGT_R / 60.0f;  // 3.5 px/deg — ±60° full scale

// Ring radii at ±15°, ±30°, ±45°, ±60°
static const uint16_t TGT_RING_15 = TGT_R / 4;        // 52
static const uint16_t TGT_RING_30 = TGT_R / 2;        // 105
static const uint16_t TGT_RING_45 = (TGT_R * 3) / 4;  // 157
static const uint16_t TGT_RING_60 = TGT_R;            // 210 — ±60° boundary

// Dot display sizes — scaled up with the larger scope
static const uint8_t TGT_DOT_R_TGT   = 14;  // target diamond half-size
static const uint8_t TGT_DOT_R_VEL   = 12;  // velocity circle radius
static const uint8_t TGT_DOT_R_ERASE = 20;  // erase rect half-size

// Right panel geometry — matches the ascent/circ readout panel (360 px wide,
// right-aligned to the content edge, labels Black_28, values Black_36).
static const uint16_t TGT_RP_W  = RETICLE_RP_W;
static const uint16_t TGT_RP_X  = SCREEN_W - SIDEBAR_W - TGT_RP_W;   // 580
static const uint8_t  TGT_RP_NR = 7;
static const tFont   *TGT_RP_LF = &Roboto_Black_28;  // label font (printDispChrome)
static const tFont   *TGT_RP_F  = &Roboto_Black_36;  // value font (printValue)

// Closure-velocity bar — centred under the scope (mirrors DOCK's approach bar so
// all three reticle screens share the reticle + bottom-bar layout).
static const uint16_t TGT_BAR_W      = RETICLE_BAR_W;
static const uint16_t TGT_BAR_X      = TGT_SCX - TGT_BAR_W / 2;   // 64
static const uint16_t TGT_BAR_H      = 26;
static const float    TGT_BAR_MAX_MS = 250.0f;   // full bar = 250 m/s closure
static float          _tgtPrevBarVC  = -9999.0f; // bar redraw cache (reset on entry)


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
static void _tgtDrawScopeChrome(KCM_TFT &tft) {
    // Shared disc + rings + cardinals + crosshair + ticks + bezel (gap 18, tick 14
    // — same gap and tick length as MNVR/DOCK so the centre nose crosshair and
    // ticks are identical across all three reticle screens).
    reticleDrawBase(tft, TGT_SCX, TGT_SCY, TGT_R, 18, 14);

    // Ring degree labels — NE quadrant, just inside each ring.
    // Single-arg setTextColor = transparent background (no black rectangle under text).
    tft.setFont(Roboto_Black_16);
    tft.setTextColor(TFT_LIGHT_GREY);
    tft.setCursor(TGT_SCX + 3, TGT_SCY - TGT_RING_15 + 3);  tft.print("15");
    tft.setCursor(TGT_SCX + 3, TGT_SCY - TGT_RING_30 + 3);  tft.print("30");
    tft.setCursor(TGT_SCX + 3, TGT_SCY - TGT_RING_45 + 3);  tft.print("45");
    tft.setCursor(TGT_SCX + 3, TGT_SCY - TGT_RING_60 + 3);  tft.print("60");

    // Legend — top-left of scope area, above circle edge
    static const uint16_t LEG_X  = 6;
    static const uint16_t LEG_Y0 = TITLE_TOP + 6;
    static const uint16_t LEG_DY = 20;

    tft.setFont(Roboto_Black_16);

    // VEL — hollow green circle
    tft.drawCircle(LEG_X + 6, LEG_Y0 + 6, 5, TFT_NEON_GREEN);
    tft.setTextColor(TFT_SAP_GREEN, TFT_BLACK);
    tft.setCursor(LEG_X + 16, LEG_Y0);
    tft.print("VEL");

    // TGT — solid violet diamond (same symbol as DOCK PORT dot)
    drawDiamondMarker(tft, LEG_X + 6, LEG_Y0 + LEG_DY + 7, 6, TFT_VIOLET);
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

    // Closure bar chrome — label + empty bar, centred under the scope (Black_24,
    // same geometry math as the DOCK/MNVR bottom bars).
    uint16_t barY = TGT_SCY + TGT_R + 42;
    uint16_t lblY = barY - 34;
    tft.setFont(Roboto_Black_24);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(TGT_BAR_X, lblY);
    tft.print("CLOSURE");
    tft.drawRect(TGT_BAR_X, barY, TGT_BAR_W, TGT_BAR_H, TFT_GREY);
}


// ── Closure-velocity bar (threshold-gated) ────────────────────────────────────────────
// Fill proportional to |closure| (full = TGT_BAR_MAX_MS); colour matches the V.Tgt row:
// green closing, yellow opening, white-on-red when closing too fast at short range.
static void _tgtDrawClosureBar(KCM_TFT &tft, float vc, float dist) {
    static const uint16_t barY = TGT_SCY + TGT_R + 42;
    static const uint16_t lblY = barY - 34;

    if (fabsf(vc - _tgtPrevBarVC) < 0.5f) return;
    _tgtPrevBarVC = vc;

    float    av      = fabsf(vc);
    bool     closing = (vc < 0.0f);
    bool     tooFast = (av > TGT_VCLOSURE_ALARM_MS && dist < RNDZ_DIST_WARN_M);
    uint16_t barCol  = tooFast ? TFT_RED : (!closing ? TFT_YELLOW : TFT_DARK_GREEN);

    float    fraction = fminf(av / TGT_BAR_MAX_MS, 1.0f);
    uint16_t fillW    = (uint16_t)(fraction * (TGT_BAR_W - 2));
    tft.fillRect(TGT_BAR_X + 1, barY + 1, TGT_BAR_W - 2, TGT_BAR_H - 2, TFT_OFF_BLACK);
    if (fillW > 0)
        tft.fillRect(TGT_BAR_X + 1, barY + 1, fillW, TGT_BAR_H - 2, barCol);

    // Signed closure value, right-aligned on the label row.
    char buf[14];
    snprintf(buf, sizeof(buf), "%+.0fm/s", vc);
    tft.setFont(Roboto_Black_24);
    tft.setTextColor(barCol, TFT_BLACK);
    tft.fillRect(TGT_BAR_X + TGT_BAR_W / 2, lblY, TGT_BAR_W / 2, 30, TFT_BLACK);
    int16_t tw = getFontStringWidth(&Roboto_Black_24, buf);
    tft.setCursor(TGT_BAR_X + TGT_BAR_W - tw, lblY);
    tft.print(buf);
}


// ── Repair scope chrome after dot erase ───────────────────────────────────────────────
// After a fillRect erase, any rings or crosshair lines that intersected the
// erase box must be redrawn. Logic mirrors Screen_DOCK._dockRepairChrome().
static void _tgtRepairChrome(KCM_TFT &tft, int16_t bx, int16_t by, uint8_t bh) {
    int16_t boxX0 = bx, boxX1 = bx + 2*bh, boxY0 = by, boxY1 = by + 2*bh;

    // Shared reticle restore: rings / cardinals / crosshair / centre-dot / good-zone.
    // Returns the marker's distance from centre so we can detect a good-zone hit.
    float d = reticleRepair(tft, TGT_SCX, TGT_SCY, TGT_R, 18, bx, by, bh);

    // Redraw the ring label(s) whose bbox overlaps the erase box (each sits at
    // (SCX+3, SCY - RING_r + 3); bbox sized for Roboto_Black_16). Redrawing only
    // the overlapping one avoids painting a label over a different dot. The
    // innermost ("15") label sits at radius ~49, inside the good-zone fill
    // (radius R/4-1) — so when a marker enters the inner circle the good-zone
    // repaint erases it even though the erase box may not overlap its bbox. Force
    // its redraw whenever the marker is inside the good zone (goodZoneHit).
    {
        static const uint16_t lblR[]  = {TGT_RING_15, TGT_RING_30, TGT_RING_45, TGT_RING_60};
        static const char    *lblTxt[]= {"15", "30", "45", "60"};
        bool fontSet = false;
        for (uint8_t i = 0; i < 4; i++) {
            int16_t lx = TGT_SCX + 3, ly = TGT_SCY - lblR[i] + 3;
            bool boxHit      = (boxX1 >= lx && boxX0 <= lx + 26 && boxY1 >= ly && boxY0 <= ly + 20);
            bool goodZoneHit = (i == 0 && d <= (float)(TGT_R / 4));
            if (boxHit || goodZoneHit) {
                if (!fontSet) { tft.setFont(Roboto_Black_16); tft.setTextColor(TFT_LIGHT_GREY); fontSet = true; }
                tft.setCursor(lx, ly);
                tft.print(lblTxt[i]);
            }
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
static void _tgtUpdateDots(KCM_TFT &tft, float tgtBrg, float tgtElv,
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
        drawDiamondMarker(tft, tSX, tSY, ds, TFT_VIOLET);
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
        static const uint16_t g = 18;   // matches reticleDrawBase gap
        tft.drawLine(TGT_SCX - g + 2, TGT_SCY, TGT_SCX - 4, TGT_SCY, TFT_GREY);
        tft.drawLine(TGT_SCX + 4,     TGT_SCY, TGT_SCX + g - 2, TGT_SCY, TFT_GREY);
        tft.drawLine(TGT_SCX, TGT_SCY - g + 2, TGT_SCX, TGT_SCY - 4, TFT_GREY);
        tft.drawLine(TGT_SCX, TGT_SCY + 4,     TGT_SCX, TGT_SCY + g - 2, TFT_GREY);
    }
    tft.fillCircle(TGT_SCX, TGT_SCY, 2, TFT_GREY);
}


// ── CHROME ────────────────────────────────────────────────────────────────────────────
static void chromeScreen_TGT(KCM_TFT &tft) {
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
    _tgtPrevBarVC = -9999.0f;   // force the closure bar to redraw on entry

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
    printDispChrome(tft, F, TGT_RP_X, rowYFor(3,NR), TGT_RP_W, rowH, "V.Close:",  COL_LABEL, COL_BACK, COL_NO_BDR);

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

    // Row 6: T+Int full-width
    printDispChrome(tft, F, TGT_RP_X, rowYFor(6,NR), TGT_RP_W, rowH, "T+Int:", COL_LABEL, COL_BACK, COL_NO_BDR);

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
static void drawScreen_TGT(KCM_TFT &tft) {

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
        tgtVal(3, 3, "V.Close:", fmtMs(vc), fg, bg);
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

    // Row 6 — T+Int  (cache slot 8)
    // Estimated intercept time = distance / |closure rate|.
    // Only meaningful when closing; shows "---" otherwise.
    {
        bool closing = (state.tgtVelocity < -0.5f);
        String tIntStr;
        if (closing) {
            float t = state.tgtDistance / fabsf(state.tgtVelocity);
            tIntStr = formatTimeCompact((int64_t)t);
            fg = TFT_DARK_GREEN; bg = TFT_BLACK;
        } else {
            tIntStr = "---";
            fg = TFT_DARK_GREY; bg = TFT_BLACK;
        }
        tgtVal(6, 8, "T+Int:", tIntStr, fg, bg);
    }

    // ── Closure-velocity bar (left panel, under the scope) ───────────────────────────
    _tgtDrawClosureBar(tft, state.tgtVelocity, state.tgtDistance);

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
