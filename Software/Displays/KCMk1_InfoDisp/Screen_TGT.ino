/***************************************************************************************
   Screen_TGT.ino  —  Target / Rendezvous screen  (RPOD display)

   LAYOUT (1024×600, content area 940×538 below title bar; reticle R=210)
   ┌─────────────────────────────────┬──────────────────────────────────────┐
   │                                 │ ALT.SL:         142.8 km             │
   │   RPOD Scope                    │ V.ORB:         2247.3 m/s            │
   │   (black disc, R=210, ±60°)     │ DIST:           48.3 km             │
   │                                 │ V.CLOSE:         -1.4 m/s            │
   │   ◆ target (TFT_VIOLET)         │ BRG:  +22.0°  │ ELV:  -14.0°        │
   │   ○ velocity vector (NEON_GREEN)│ V.BRG:  +14.0° │ V.ELV:  -9.0°      │
   │   + nose crosshair (fixed)      │ T+INT:          6m 30s               │
   └─────────────────────────────────┴──────────────────────────────────────┘

   SCOPE SEMANTICS
   ───────────────
   Fixed crosshair (+) = your nose direction (always at screen centre)
   Violet diamond (TGT) = where the target is relative to your nose
                          Centre → nose is pointed at target
   Green circle (VEL)   = where your relative velocity points relative to your nose
                          Centre → you are flying straight along the boresight
   Dimmed marker        = pinned at the scope boundary (further off the nose than the
                          outer ring can show); direction honest, distance understated.
                          Brg/Elv carry the true value.
   Perfect intercept    = both dots converging on each other (VEL sitting on TGT means
                          the relative velocity is aimed at the target; V.Brg/V.Elv are
                          that error in degrees, measured about the target axis)

   SCOPE GEOMETRY (rev-2, 1024×600)
   ──────────────
   Centre: (289, 300)   Radius: 210px   Scale: 3.5 px/deg (±60° full scale)
   Field of view: ±60° (wider than DOCK ±20° — long-range ops need more range)
   Rings: 15°=r52, 30°=r105, 45°=r157, 60°=r210. Centre/radius match the
   MNVR and DOCK reticles.
   Right panel: 360 px at x=580 (matches MNVR/DOCK), labels 28 / values 36

   RIGHT PANEL — 7 rows, rowHFor(7) = 76px each
   Row 0  Alt.SL   full-width
   Row 1  V.Orb    full-width
   Row 2  Dist     full-width, colour-coded by range (RNDZ_DIST thresholds)
   Row 3  V.Close  full-width, colour-coded by speed (TGT_VCLOSURE thresholds)
   Row 4  Brg|Elv  split — bearing and elevation of the target from the NOSE
   Row 5  V.Brg|V.Elv split — approach-path error about the target axis, colour-coded
   Row 6  T+Int    full-width — estimated intercept time (dist / |vtgt|), closing only

   DOT UPDATE STRATEGY  (same as DOCK)
   ────────────────────
   prevDot stored in screen coords. On change > 1px:
     fillRect(prev - R_ERASE, ..., TFT_BLACK) → reticleRepairDotChrome() → draw new dot
   Erase rect is slightly larger than dot to clean up any anti-alias edge.
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
static const uint8_t TGT_DOT_R_TGT   = 22;  // target marker radius
static const uint8_t TGT_DOT_R_VEL   = 19;  // velocity marker radius
static const uint8_t TGT_DOT_R_ERASE = 33;  // erase rect half-size (covers prograde ring 19 + spoke 12)

// Right panel geometry — matches the ascent/circ readout panel (360 px wide,
// right-aligned to the content edge, labels Black_28, values Black_36).
static const uint16_t TGT_RP_W  = RETICLE_RP_W;
static const uint16_t TGT_RP_X  = CONTENT_W - TGT_RP_W;              // 580
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


// ── Shared dot-layer geometry + per-screen erase cache ───────────────────────────────
// The moving marker layer is shared with MNVR/DOCK (reticleUpdateDots, KerbalDisplayCommon).
// Only the angular SCALE and the four ring labels differ; both are captured here. Built
// from the existing named constants above so values never diverge from the chrome/bar.
static const char *const TGT_RING_LBL[4] = { "15", "30", "45", "60" };
// Body-referenced, like every other boresight display in the project. TGT and DOCK are
// the same task at different ranges — they share a sidebar button, and the Dist row goes
// white-on-green below 200 m to say "switch to DOCK" — so a frame change between them
// would re-anchor every marker at exactly the moment the pilot crosses over.
static const ReticleGeom TGT_GEOM = {
    TGT_SCX, TGT_SCY, TGT_R, TGT_SCALE,
    TGT_DOT_R_TGT, TGT_DOT_R_VEL, TGT_DOT_R_ERASE,
    TGT_DOT_R_VEL * 3 / 2 + 2,      // clampMargin — widest symbol is the prograde ring + spoke
    TGT_RING_LBL, &Roboto_Black_16
};
// Per-screen erase-before-redraw cache (9999 = marker not shown). Reset on entry.
static ReticleDotCache _tgtDots;


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

    // VEL — green prograde marker (target-RELATIVE velocity; the PFD ball uses the
    //       same glyph for orbital/surface velocity — context tells them apart,
    //       the legend names this one)
    drawProgradeMarker(tft, LEG_X + 6, LEG_Y0 + 6, 5, TFT_NEON_GREEN);
    tft.setTextColor(TFT_SAP_GREEN, TFT_BLACK);
    tft.setCursor(LEG_X + 16, LEG_Y0);
    tft.print("VEL");

    // TGT — magenta target marker (same symbol as DOCK PORT dot)
    drawTargetMarker(tft, LEG_X + 6, LEG_Y0 + LEG_DY + 7, 6, TFT_VIOLET);
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


// Dot-layer machinery (project / clamp / erase / chrome-repair / update) lives in
// KerbalDisplayCommon ≥ 3.2.0 and is shared with MNVR and DOCK.
// TGT drives them via TGT_GEOM (scale = r/60, ring labels 15/30/45/60) and _tgtDots.


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
    _tgtDots = ReticleDotCache{};
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

    // Row 5: V.Brg | V.Elv split (approach-path error about the target axis)
    {
        uint16_t y = rowYFor(5, NR), h = rowH;
        printDispChrome(tft, F, TGT_RP_X,        y, HW - ROW_PAD, h, "V.Brg:", COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, F, TGT_RP_X + HW,   y, HW - ROW_PAD, h, "V.Elv:", COL_LABEL, COL_BACK, COL_NO_BDR);
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
    // Shared derived-angle block (identical to DOCK): nose→target, velocity→target and
    // both antipodal directions, bearings wrapped to ±180°.
    ReticleAngles ang = reticleComputeAngles();

    // ── Update scope dots ─────────────────────────────────────────────────────────────
    // Shared marker layer (KerbalDisplayCommon).
    reticleUpdateDots(tft, TGT_GEOM, _tgtDots, ang);

    // ── Right panel values ────────────────────────────────────────────────────────────
    const uint8_t NR = TGT_RP_NR;
    const uint16_t HW = TGT_RP_W / 2;
    uint16_t fg, bg;
    char buf[16];

    // Cache-checked draw helper — full-width right panel rows
    auto tgtVal = [&](uint8_t row, uint8_t slot, const char *label, const String &val,
                      uint16_t fgc, uint16_t bgc) {
        drawPanelValue(tft, screen_TGT, slot, row, TGT_RP_X, TGT_RP_W, label, val, fgc, bgc, TGT_RP_F, NR, false);
    };

    // Cache-checked draw helper — half-width split rows, independent cache slots
    auto tgtValH = [&](uint8_t row, uint8_t slot, uint16_t x,
                       const char *label, const String &val,
                       uint16_t fgc, uint16_t bgc) {
        drawPanelValue(tft, screen_TGT, slot, row, x, HW - ROW_PAD, label, val, fgc, bgc, TGT_RP_F, NR, false);
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
    // Where the target sits relative to the NOSE — the same quantity MNVR's Brg/Elv
    // shows, and literally where the violet TGT marker is on the scope. This used to
    // print state.tgtHeading wrapped to +/-180 under a comment claiming "positive =
    // target to the right", which was the vessel's compass bearing: with the target dead
    // ahead on heading 120 it read +120, not 0.
    {
        snprintf(buf, sizeof(buf), "%+.0f\xB0", ang.priRight);
        tgtValH(4, 4, TGT_RP_X, "Brg:", String(buf), TFT_DARK_GREEN, TFT_BLACK);

        snprintf(buf, sizeof(buf), "%+.0f\xB0", ang.priUp);
        tgtValH(4, 5, TGT_RP_X + HW, "Elv:", String(buf), TFT_DARK_GREEN, TFT_BLACK);
    }

    // Row 5 — V.Brg | V.Elv  (cache slots 6, 7)
    // Approach-path error about the TARGET axis: how far right and above the approach
    // path the relative velocity actually points. Same quantity DOCK shows under the
    // same labels. Colour bands are the scope rings (green inside 15 deg, yellow to
    // 30 deg, red beyond).
    auto errColor = [](float ae, uint16_t &fg, uint16_t &bg) {
        if      (ae >= TGT_BRG_ALARM_DEG) { fg = TFT_WHITE;      bg = TFT_RED;   }
        else if (ae >= TGT_BRG_WARN_DEG)  { fg = TFT_YELLOW;     bg = TFT_BLACK; }
        else                               { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    };
    {
        // Past 90 deg the craft is travelling away from the target and the signed pair
        // stops being flyable, so both cells show "---" (the idiom T+Int already uses
        // when not closing). It also keeps the value inside the half-width cell.
        bool appValid = (sqrtf(ang.appRight*ang.appRight + ang.appUp*ang.appUp) <= 90.0f);
        if (!appValid) {
            tgtValH(5, 6, TGT_RP_X,      "V.Brg:", "---", TFT_DARK_GREY, TFT_BLACK);
            tgtValH(5, 7, TGT_RP_X + HW, "V.Elv:", "---", TFT_DARK_GREY, TFT_BLACK);
        } else {
            errColor(fabsf(ang.appRight), fg, bg);
            snprintf(buf, sizeof(buf), "%+.0f\xB0", ang.appRight);
            tgtValH(5, 6, TGT_RP_X, "V.Brg:", String(buf), fg, bg);

            errColor(fabsf(ang.appUp), fg, bg);
            snprintf(buf, sizeof(buf), "%+.0f\xB0", ang.appUp);
            tgtValH(5, 7, TGT_RP_X + HW, "V.Elv:", String(buf), fg, bg);
        }
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
