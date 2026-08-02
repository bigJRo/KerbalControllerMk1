/***************************************************************************************
   Screen_DOCK.ino  —  Docking screen: graphical approach reticle + critical numbers

   LAYOUT (1024×600, content area 940×538 below title bar; reticle R=210)
   ┌────────────────────────────────┬──────────────────────────────────────┐
   │                                │ DIST:         247 m                  │
   │   Approach Reticle             │ V.CLOSE:    -1.4 m/s                 │
   │   (black disc, R=170)          │ DRIFT.H:   +0.24 m/s                 │
   │                                │ DRIFT.V:   -0.04 m/s                 │
   │   ● velocity vector (green)    │ NOS.BRG:    +12.3°                   │
   │   ◆ target port (magenta)      │ NOS.ELV:     +8.1°                   │
   │   + fixed crosshair (grey)     │ RCS: ON      SAS: TARGET             │
   │                                │ [approach distance bar]              │
   └────────────────────────────────┴──────────────────────────────────────┘

   RETICLE SEMANTICS
   ─────────────────
   Fixed crosshair  = your nose direction (always at centre)
   Green dot (VEL)  = where your velocity vector points relative to target bearing
                      At centre → you are flying straight at the port (on axis)
   Magenta dot (PORT) = where the port is relative to your nose
                      At centre → your nose is pointing at the port (aligned)
   Perfect approach = both dots at centre simultaneously

   RETICLE GEOMETRY (rev-2, 1024×600)
   ──────────────────────────────────
   Centre: (289, 300)  Radius: 210px  Scale: 10.5 px/deg (±20° full scale)
   Rings: ±5° r=52, ±10° r=105, ±15° r=157, ±20° r=210. Centred in the left
   region x=[0,578]; approach bar centred under it.
   Right panel: 360 px at x=580 (matches ascent/MNVR), labels 28 / values 36

   DOT UPDATE STRATEGY
   ────────────────────
   prevDot stored in screen coords. On change > 1px:
     fillRect(prev - R_ERASE, prev - R_ERASE, 2*R_ERASE+1, 2*R_ERASE+1, TFT_BLACK)
     fillCircle(new, R_DOT, colour)
   Erase radius slightly larger than draw radius to clean up anti-alias edge.
   Background of reticle is always solid black — safe to fillRect-erase.

   THREE STATES
   ─────────────
   DOCKED:      "DOCKED" splash (handled by state machine, unchanged)
   NO TARGET:   "NO TARGET SET" splash (unchanged)
   APPROACH:    Graphical reticle + right panel numbers
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


// ── State flags (extern linkage matches KCMk1_InfoDisp.h declarations) ───────────────
bool     _dockChromDrawn  = false;
bool     _vesselDocked    = false;
uint32_t _dockedTimestamp = 0;


// ── Reticle geometry — centred and stretched to fill the left region (matches MNVR) ──
// The readout panel now sits on the far right (x=580), leaving x=[0,578] for the
// reticle. It is centred there, enlarged, and the approach bar is centred below.
static const uint16_t RET_CX   = RETICLE_CX;   // centre of the left region x=[0,578]
static const uint16_t RET_CY   = RETICLE_CY;
static const uint16_t RET_R    = RETICLE_R;    // reticle radius (px)
static const float    RET_SCALE = (float)RET_R / 20.0f;  // 10.5 px/deg — ±20° full scale

// Ring radii for ±5°, ±10°, ±15°, ±20°
static const uint16_t RING_5  = RET_R / 4;         // 52
static const uint16_t RING_10 = RET_R / 2;         // 105
static const uint16_t RING_15 = (RET_R * 3) / 4;   // 157
static const uint16_t RING_20 = RET_R;             // 210 — ±20° boundary

// Dot display sizes — scaled up with the larger reticle
static const uint8_t DOT_R_PORT  = 14;   // target port dot radius
static const uint8_t DOT_R_VEL   = 12;   // velocity vector dot radius
static const uint8_t DOT_R_ERASE = 20;   // erase rect half-size (covers dot + overdraw)

// Right panel geometry — matches the ascent/circ readout panel (360 px wide,
// right-aligned to the content edge, labels Black_28, values Black_36).
static const uint16_t RP_W   = RETICLE_RP_W;
static const uint16_t RP_X   = SCREEN_W - SIDEBAR_W - RP_W;   // 580
static const uint8_t  RP_NR  = 8;       // number of rows
static const tFont   *RP_LBL = &Roboto_Black_28;  // label font (chrome)
static const tFont   *RP_F   = &Roboto_Black_36;  // value font

// Approach bar geometry — centred under the reticle
static const uint16_t BAR_W  = RETICLE_BAR_W;
static const uint16_t BAR_X  = RET_CX - BAR_W / 2;   // 64
static const uint16_t BAR_H  = 26;
static const float    BAR_MAX_DIST = 250.0f;   // full bar = 250m (docking approach range)


// ── Previous dot positions (for erase) ───────────────────────────────────────────────
// Stored as screen coordinates. 9999 = not yet drawn (skip erase on first frame).
static int16_t _dockPrevPortX = 9999, _dockPrevPortY = 9999;
static int16_t _dockPrevVelX  = 9999, _dockPrevVelY  = 9999;


// ── Wrap heading error to ±180° ───────────────────────────────────────────────────────
static inline float _dockWrapErr(float e) {
    while (e >  180.0f) e -= 360.0f;
    while (e < -180.0f) e += 360.0f;
    return e;
}


// ── Clamp a dot position to within the reticle boundary ──────────────────────────────
static void _dockClampDot(int16_t &sx, int16_t &sy) {
    float dx = sx - RET_CX, dy = sy - RET_CY;
    float dist = sqrtf(dx*dx + dy*dy);
    float maxR = (float)(RET_R - DOT_R_PORT - 2);
    if (dist > maxR && dist > 0.5f) {
        float scale = maxR / dist;
        sx = RET_CX + (int16_t)(dx * scale);
        sy = RET_CY + (int16_t)(dy * scale);
    }
}


// ── Draw the static reticle chrome ───────────────────────────────────────────────────
static void _dockDrawReticleChrome(KCM_TFT &tft) {
    // Shared disc + rings + cardinals + crosshair + ticks + bezel (MNVR/DOCK gap 18, tick 14)
    reticleDrawBase(tft, RET_CX, RET_CY, RET_R, 18, 14);

    // Ring degree labels (positioned just inside each ring, in the NE quadrant).
    // Single-arg setTextColor = transparent background.
    tft.setFont(Roboto_Black_16);
    tft.setTextColor(TFT_LIGHT_GREY);
    tft.setCursor(RET_CX + 3, RET_CY - RING_5  + 3);  tft.print("5");
    tft.setCursor(RET_CX + 3, RET_CY - RING_10 + 3);  tft.print("10");
    tft.setCursor(RET_CX + 3, RET_CY - RING_15 + 3);  tft.print("15");
    tft.setCursor(RET_CX + 3, RET_CY - RING_20 + 3);  tft.print("20");

    // Legend: 3 rows stacked in top-left corner, above/beside the circle top
    // y=68,88,108 — all safely left of circle edge at those y positions
    static const uint16_t LEG_X  = 6;
    static const uint16_t LEG_Y0 = TITLE_TOP + 6;  // 68
    static const uint16_t LEG_DY = 20;              // row spacing

    tft.setFont(Roboto_Black_16);

    // Row 0: VEL — green prograde marker
    drawProgradeMarker(tft, LEG_X + 6, LEG_Y0 + 6, 5, TFT_NEON_GREEN);
    tft.setTextColor(TFT_SAP_GREEN, TFT_BLACK);
    tft.setCursor(LEG_X + 16, LEG_Y0);
    tft.print("VEL");

    // Row 1: PORT — magenta target marker
    drawProgradeMarker(tft, LEG_X + 6, LEG_Y0 + LEG_DY + 7, 5, TFT_VIOLET);
    tft.setTextColor(TFT_VIOLET, TFT_BLACK);
    tft.setCursor(LEG_X + 16, LEG_Y0 + LEG_DY);
    tft.print("PORT");

    // Row 2: NOSE — crosshair symbol
    tft.drawLine(LEG_X+1,  LEG_Y0+LEG_DY*2+6, LEG_X+4,  LEG_Y0+LEG_DY*2+6, TFT_GREY);
    tft.drawLine(LEG_X+8,  LEG_Y0+LEG_DY*2+6, LEG_X+11, LEG_Y0+LEG_DY*2+6, TFT_GREY);
    tft.drawLine(LEG_X+6,  LEG_Y0+LEG_DY*2+1, LEG_X+6,  LEG_Y0+LEG_DY*2+4, TFT_GREY);
    tft.drawLine(LEG_X+6,  LEG_Y0+LEG_DY*2+8, LEG_X+6,  LEG_Y0+LEG_DY*2+11,TFT_GREY);
    tft.fillCircle(LEG_X+6, LEG_Y0+LEG_DY*2+6, 1, TFT_GREY);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(LEG_X + 16, LEG_Y0 + LEG_DY * 2);
    tft.print("NOSE");

    // Bottom-left: APPROACH bar (90% width, centred, with label row above).
    // Bar label/value font matches LNCH_Circ ΔV Burn bar and MNVR ΔV Burn bar
    // (Black_20). Bar shifted 8 px down (RET_CY+R+20 → +28) to make room for the
    // taller 24 px label between the reticle bottom and the bar.
    uint16_t barY = RET_CY + RET_R + 42;   // 552 — bar top
    uint16_t lblY = barY - 34;              // 518 — label row above bar
    tft.setFont(Roboto_Black_24);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(BAR_X, lblY);
    tft.print("APPROACH");
    tft.drawRect(BAR_X, barY, BAR_W, BAR_H, TFT_GREY);
}


// ── Draw right-panel chrome (static labels) ───────────────────────────────────────────
static void _dockDrawRightChrome(KCM_TFT &tft) {
    // Rows 0–1: range and time
    printDispChrome(tft, RP_LBL, RP_X, rowYFor(0,RP_NR), RP_W, rowHFor(RP_NR), "Dist:",    COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, RP_LBL, RP_X, rowYFor(1,RP_NR), RP_W, rowHFor(RP_NR), "T+Dock:",  COL_LABEL, COL_BACK, COL_NO_BDR);

    // Divider between T+Dock(1) and V.Close(2)
    { uint16_t dy = rowYFor(2,RP_NR) - 1;
      tft.drawLine(RP_X, dy,   RP_X+RP_W, dy,   TFT_GREY);
      tft.drawLine(RP_X, dy+1, RP_X+RP_W, dy+1, TFT_GREY); }

    // Rows 2–3: speed
    printDispChrome(tft, RP_LBL, RP_X, rowYFor(2,RP_NR), RP_W, rowHFor(RP_NR), "V.Close:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, RP_LBL, RP_X, rowYFor(3,RP_NR), RP_W, rowHFor(RP_NR), "V.Lat:",   COL_LABEL, COL_BACK, COL_NO_BDR);

    // Divider between V.Lat(3) and Vel.Brg(4)
    { uint16_t dy = rowYFor(4,RP_NR) - 1;
      tft.drawLine(RP_X, dy,   RP_X+RP_W, dy,   TFT_GREY);
      tft.drawLine(RP_X, dy+1, RP_X+RP_W, dy+1, TFT_GREY); }

    // Rows 4–5: approach path alignment (velocity vector vs port)
    printDispChrome(tft, RP_LBL, RP_X, rowYFor(4,RP_NR), RP_W, rowHFor(RP_NR), "Vel.Brg:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, RP_LBL, RP_X, rowYFor(5,RP_NR), RP_W, rowHFor(RP_NR), "Vel.Elv:", COL_LABEL, COL_BACK, COL_NO_BDR);

    // Divider between Vel.Elv(5) and Nos.Off(6)
    { uint16_t dy = rowYFor(6,RP_NR) - 1;
      tft.drawLine(RP_X, dy,   RP_X+RP_W, dy,   TFT_GREY);
      tft.drawLine(RP_X, dy+1, RP_X+RP_W, dy+1, TFT_GREY); }

    // Row 6: nose total angular offset from port (combined bearing + elevation)
    printDispChrome(tft, RP_LBL, RP_X, rowYFor(6,RP_NR), RP_W, rowHFor(RP_NR), "Nos.Off:", COL_LABEL, COL_BACK, COL_NO_BDR);

    // Divider before RCS/SAS button row (row 7)
    { uint16_t dy = rowYFor(7, RP_NR) - 1;
      tft.drawLine(RP_X, dy,   RP_X + RP_W, dy,   TFT_GREY);
      tft.drawLine(RP_X, dy+1, RP_X + RP_W, dy+1, TFT_GREY); }

    // Row 7: RCS | SAS split divider (buttons drawn in update function)
    uint16_t hw = RP_W / 2;
    tft.drawLine(RP_X + hw,     TITLE_TOP + 7 * rowHFor(RP_NR), RP_X + hw,     SCREEN_H - 1, TFT_GREY);
    tft.drawLine(RP_X + hw + 1, TITLE_TOP + 7 * rowHFor(RP_NR), RP_X + hw + 1, SCREEN_H - 1, TFT_GREY);
}


// ── Update dots — erase old, draw new ────────────────────────────────────────────────
// ── Ring/crosshair redraw after dot erase ─────────────────────────────────────────────
// After a fillRect erase, any static chrome (rings, crosshair) that intersected
// the erase box must be redrawn. Ring intersects erase box if:
//   closest point on ring to box centre is within the box,
//   AND farthest corner of box from centre is beyond the ring radius.
// Simplified: check if box overlaps the annulus [r-1, r+1].
static void _dockRepairChrome(KCM_TFT &tft, int16_t bx, int16_t by, uint8_t bh) {
    // bx,by = erase box top-left, bh = erase box half-size (so w=h=2*bh+1)
    int16_t boxX0=bx, boxX1=bx+2*bh, boxY0=by, boxY1=by+2*bh;

    // Shared reticle restore: rings / cardinals / crosshair / centre-dot / good-zone.
    float d = reticleRepair(tft, RET_CX, RET_CY, RET_R, 18, bx, by, bh);

    // Ring degree labels — the erase fillRect wipes them (drawn transparent), so
    // redraw the label(s) whose bbox overlaps the erase box, PLUS the innermost one
    // when the good-zone refill (radius R/4) painted over it (dot inside that ring).
    {
        static const uint16_t lblR[]   = {RING_5, RING_10, RING_15, RING_20};
        static const char    *lblTxt[] = {"5", "10", "15", "20"};
        bool fontSet = false;
        for (uint8_t i = 0; i < 4; i++) {
            int16_t lx = RET_CX + 3, ly = RET_CY - lblR[i] + 3;
            bool boxHit      = (boxX1 >= lx && boxX0 <= lx + 26 && boxY1 >= ly && boxY0 <= ly + 20);
            bool goodZoneHit = (i == 0 && d <= (float)(RET_R / 4));
            if (boxHit || goodZoneHit) {
                if (!fontSet) { tft.setFont(Roboto_Black_16); tft.setTextColor(TFT_LIGHT_GREY); fontSet = true; }
                tft.setCursor(lx, ly);
                tft.print(lblTxt[i]);
            }
        }
    }
}


// ── Update dots: erase old, repair chrome, draw new ──────────────────────────────────
// Port dot:  solid filled diamond (magenta)
// Vel dot:   hollow circle outline only (green) — shows through if port is inside
static void _dockUpdateDots(KCM_TFT &tft, float noseBrg, float noseElv,
                              float velBrg, float velElv) {
    // Port dot screen position
    int16_t portSX = RET_CX + (int16_t)(-noseBrg * RET_SCALE);
    int16_t portSY = RET_CY + (int16_t)( noseElv * RET_SCALE);
    _dockClampDot(portSX, portSY);

    // Vel dot screen position
    int16_t velSX = RET_CX + (int16_t)(-velBrg * RET_SCALE);
    int16_t velSY = RET_CY + (int16_t)( velElv * RET_SCALE);
    _dockClampDot(velSX, velSY);

    static const uint8_t EH = DOT_R_ERASE;

    // Erase and redraw port dot (solid diamond)
    bool portMoved = (_dockPrevPortX == 9999 ||
                      abs(portSX - _dockPrevPortX) > 1 ||
                      abs(portSY - _dockPrevPortY) > 1);
    if (portMoved) {
        if (_dockPrevPortX != 9999) {
            tft.fillRect(_dockPrevPortX-EH, _dockPrevPortY-EH, EH*2+1, EH*2+1, TFT_BLACK);
            _dockRepairChrome(tft, _dockPrevPortX-EH, _dockPrevPortY-EH, EH);
        }
        // KSP target marker: magenta ring + centre dot + spokes
        drawProgradeMarker(tft, portSX, portSY, DOT_R_PORT, TFT_VIOLET);
        _dockPrevPortX = portSX; _dockPrevPortY = portSY;
    }

    // Erase and redraw vel dot (hollow circle)
    bool velMoved = (_dockPrevVelX == 9999 ||
                     abs(velSX - _dockPrevVelX) > 1 ||
                     abs(velSY - _dockPrevVelY) > 1);
    if (velMoved) {
        if (_dockPrevVelX != 9999) {
            tft.fillRect(_dockPrevVelX-EH, _dockPrevVelY-EH, EH*2+1, EH*2+1, TFT_BLACK);
            _dockRepairChrome(tft, _dockPrevVelX-EH, _dockPrevVelY-EH, EH);
        }
        // KSP prograde marker: green ring + centre dot + spokes
        drawProgradeMarker(tft, velSX, velSY, DOT_R_VEL, TFT_NEON_GREEN);
        _dockPrevVelX = velSX; _dockPrevVelY = velSY;
    }

    // Always redraw the velocity marker on top so it is never buried under the target marker.
    drawProgradeMarker(tft, velSX, velSY, DOT_R_VEL, TFT_NEON_GREEN);

    // Redraw inner crosshair gap lines — vel circle (r=6,7) overlaps the inner gap
    // region when vel dot is near centre, erasing those segments.
    {
        static const uint16_t g = 18;  // cgap from chrome
        tft.drawLine(RET_CX - g + 2, RET_CY, RET_CX - 4, RET_CY, TFT_GREY);
        tft.drawLine(RET_CX + 4,     RET_CY, RET_CX + g - 2, RET_CY, TFT_GREY);
        tft.drawLine(RET_CX, RET_CY - g + 2, RET_CX, RET_CY - 4, TFT_GREY);
        tft.drawLine(RET_CX, RET_CY + 4,     RET_CX, RET_CY + g - 2, TFT_GREY);
    }

    // Crosshair centre always on top
    tft.fillCircle(RET_CX, RET_CY, 2, TFT_GREY);
}


static void _dockDrawDistBar(KCM_TFT &tft, float dist) {
    // barY must match what _dockDrawReticleChrome drew (Black_20 label).
    static const uint16_t barY = RET_CY + RET_R + 42;  // 552
    static const uint16_t lblY = barY - 34;             // 518 — label row above bar

    // Threshold gate: only redraw when distance changes by > 1m
    static float prevDist = -999.0f;
    if (fabsf(dist - prevDist) < 1.0f) return;
    prevDist = dist;

    float clamped = fminf(fmaxf(dist, 0.0f), BAR_MAX_DIST);
    uint16_t barCol = (dist < DOCK_DIST_ALARM_M) ? TFT_RED :
                      (dist < DOCK_DIST_WARN_M)  ? TFT_YELLOW :
                                                    TFT_DARK_GREEN;

    // Bar fill from RIGHT (close = right side lit)
    float fraction = 1.0f - (clamped / BAR_MAX_DIST);
    uint16_t fillW = (uint16_t)(fraction * (BAR_W - 2));
    uint16_t fillX = BAR_X + 1 + (BAR_W - 2 - fillW);
    tft.fillRect(BAR_X + 1, barY + 1, BAR_W - 2, BAR_H - 2, TFT_OFF_BLACK);
    if (fillW > 0)
        tft.fillRect(fillX, barY + 1, fillW, BAR_H - 2, barCol);

    // Distance value: right-aligned at right edge of bar, on the label row above bar.
    // Black_20 — matches the "APPROACH" label font for visual unity.
    char buf[12];
    if      (dist >= 1000.0f) snprintf(buf, sizeof(buf), "%.1fkm", dist/1000.0f);
    else if (dist >= 100.0f)  snprintf(buf, sizeof(buf), "%.0fm",  dist);
    else                      snprintf(buf, sizeof(buf), "%.1fm",  dist);

    tft.setFont(Roboto_Black_24);
    tft.setTextColor(barCol, TFT_BLACK);
    // Clear right half of label row (24 px tall for Black_20) then right-align distance text
    tft.fillRect(BAR_X + BAR_W/2, lblY, BAR_W/2, 30, TFT_BLACK);
    int16_t tw = getFontStringWidth(&Roboto_Black_24, buf);   // measure in the draw font, not RP_LBL (Black_28)
    tft.setCursor(BAR_X + BAR_W - tw, lblY);
    tft.print(buf);
}

// ── CHROME ────────────────────────────────────────────────────────────────────────────
static void chromeScreen_DOCK(KCM_TFT &tft) {
    if (_vesselDocked) {
        _dockChromDrawn = false;
        tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
        textCenter(tft, &Roboto_Black_72, 0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP,
                   "DOCKED", TFT_WHITE, TFT_DARK_GREEN);
        return;
    }

    if (!state.targetAvailable) {
        _dockChromDrawn = false;
        tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
        textCenter(tft, &Roboto_Black_72, 0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP,
                   "NO TARGET SET", TFT_WHITE, TFT_RED);
        return;
    }

    _dockChromDrawn = true;

    // Reset dot positions so first draw doesn't try to erase stale coords
    _dockPrevPortX = 9999; _dockPrevPortY = 9999;
    _dockPrevVelX  = 9999; _dockPrevVelY  = 9999;

    // Left panel: clear + reticle chrome
    tft.fillRect(0, TITLE_TOP, RP_X, SCREEN_H - TITLE_TOP, TFT_BLACK);
    _dockDrawReticleChrome(tft);

    // Divider between panels
    tft.drawLine(RP_X - 2, TITLE_TOP, RP_X - 2, SCREEN_H, TFT_GREY);
    tft.drawLine(RP_X - 1, TITLE_TOP, RP_X - 1, SCREEN_H, TFT_GREY);

    // Right panel chrome
    _dockDrawRightChrome(tft);

    // Invalidate value cache for this screen
    for (uint8_t r = 0; r < ROW_COUNT; r++) rowCache[5][r].value = "\x01";
    for (uint8_t r = 0; r < ROW_COUNT; r++) printState[5][r] = PrintState{};  // force full redraw
    // Reset approach bar so it redraws immediately on screen entry
    // (bar uses a static prevDist — reset by passing a sentinel value on next draw)
}


// ── DRAW (called every loop) ──────────────────────────────────────────────────────────
static void drawScreen_DOCK(KCM_TFT &tft) {
    uint32_t _t0 = micros();
    // Docked: nothing to update
    if (_vesselDocked) return;

    // State transitions
    if (!state.targetAvailable) {
        if (_dockChromDrawn) { _dockChromDrawn = false; switchToScreen(screen_DOCK); }
        return;
    }
    if (!_dockChromDrawn) { switchToScreen(screen_DOCK); return; }

    // ── Compute derived values ────────────────────────────────────────────────────────

    // Nose-to-port errors (where is the port relative to your nose?)
    float noseBrg = _dockWrapErr(state.heading   - state.tgtHeading);
    float noseElv = state.pitch - state.tgtPitch;

    // Velocity-to-target errors (where is velocity vector relative to target bearing?)
    float velBrg  = _dockWrapErr(state.tgtHeading - state.tgtVelHeading);
    float velElv  = state.tgtPitch - state.tgtVelPitch;

    // Lateral drift magnitude — off-axis speed component perpendicular to approach axis
    float v_lat_mag = 0.0f;
    {
        auto toUnit = [](float hdg_deg, float pit_deg, float out[3]) {
            float h = hdg_deg * DEG_TO_RAD, p = pit_deg * DEG_TO_RAD;
            out[0] = cosf(p)*sinf(h);
            out[1] = cosf(p)*cosf(h);
            out[2] = sinf(p);
        };
        float vel_unit[3], tgt_unit[3];
        toUnit(state.tgtVelHeading, state.tgtVelPitch, vel_unit);
        toUnit(state.tgtHeading,    state.tgtPitch,    tgt_unit);
        float speed = fabsf(state.tgtVelocity);
        float vel_vec[3] = { vel_unit[0]*speed, vel_unit[1]*speed, vel_unit[2]*speed };
        float v_app = vel_vec[0]*tgt_unit[0] + vel_vec[1]*tgt_unit[1] + vel_vec[2]*tgt_unit[2];
        float v_lat[3] = { vel_vec[0]-v_app*tgt_unit[0],
                           vel_vec[1]-v_app*tgt_unit[1],
                           vel_vec[2]-v_app*tgt_unit[2] };
        v_lat_mag = sqrtf(v_lat[0]*v_lat[0] + v_lat[1]*v_lat[1] + v_lat[2]*v_lat[2]);
        if (v_lat_mag < 0.005f) v_lat_mag = 0.0f;
    }

    // ── Update reticle dots ───────────────────────────────────────────────────────────
    _dockUpdateDots(tft, noseBrg, noseElv, velBrg, velElv);

    // ── Right panel values ────────────────────────────────────────────────────────────
    char buf[20];
    uint16_t fg, bg;

    auto dockVal = [&](uint8_t row, uint8_t slot, const char *label, const String &val,
                        uint16_t fgc, uint16_t bgc) {
        RowCache &rc = rowCache[5][slot];
        if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
        printValue(tft, RP_F, RP_X, rowYFor(row, RP_NR), RP_W, rowHFor(RP_NR),
                   label, val, fgc, bgc, COL_BACK, printState[5][slot]);
        rc.value = val; rc.fg = fgc; rc.bg = bgc;
    };

    auto angCol = [](float e, uint16_t &fg, uint16_t &bg) {
        float ae = fabsf(e);
        if      (ae >= DOCK_BRG_ALARM_DEG) { fg = TFT_WHITE;     bg = TFT_RED;   }
        else if (ae >= DOCK_BRG_WARN_DEG)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
        else                                { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    };

    // Row 0 — Distance  (cache slot 0)
    if      (state.tgtDistance < DOCK_DIST_ALARM_M) { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (state.tgtDistance < DOCK_DIST_WARN_M)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                                              { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    dockVal(0, 0, "Dist:", formatAlt(state.tgtDistance), fg, bg);

    // Row 1 — Time to docking  (cache slot 1)
    {
        float vc = state.tgtVelocity;
        bool closing = (vc < -0.01f);
        if (!closing) {
            dockVal(1, 1, "T+Dock:", "---", TFT_DARK_GREY, TFT_BLACK);
        } else {
            float tDock = state.tgtDistance / fabsf(vc);
            if      (tDock < 10.0f)  { fg = TFT_WHITE;     bg = TFT_RED;   }
            else if (tDock < 30.0f)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
            else                     { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
            dockVal(1, 1, "T+Dock:", formatTimeCompact(tDock), fg, bg);
        }
    }

    // Row 2 — Closure velocity  (cache slot 2)
    {
        float vc = state.tgtVelocity;
        bool closing = (vc < 0.0f);
        bool tooFast = fabsf(vc) > DOCK_VCLOSURE_ALARM_MS && state.tgtDistance < DOCK_VCLOSURE_ALARM_DIST_M;
        if (tooFast) { fg = TFT_WHITE; bg = TFT_RED; }
        else         { fg = closing ? TFT_DARK_GREEN : TFT_YELLOW; bg = TFT_BLACK; }
        snprintf(buf, sizeof(buf), "%+.2f m/s", vc);
        dockVal(2, 2, "V.Close:", buf, fg, bg);
    }

    // Row 3 — V.Lat total lateral drift magnitude  (cache slot 3)
    {
        float av = v_lat_mag;
        if      (av >= DOCK_DRIFT_ALARM_MS) { fg = TFT_WHITE;      bg = TFT_RED;   }
        else if (av >= DOCK_DRIFT_WARN_MS)  { fg = TFT_YELLOW;     bg = TFT_BLACK; }
        else                                 { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        dockVal(3, 3, "V.Lat:", fmtMs(v_lat_mag), fg, bg);
    }

    // Row 4 — Vel.Brg: bearing error of velocity vector vs port (cache slot 4)
    // Is your approach path aimed at the port?
    angCol(velBrg, fg, bg);
    snprintf(buf, sizeof(buf), "%+.1f\xB0", velBrg);
    dockVal(4, 4, "Vel.Brg:", buf, fg, bg);

    // Row 5 — Vel.Elv: elevation error of velocity vector vs port (cache slot 5)
    angCol(velElv, fg, bg);
    snprintf(buf, sizeof(buf), "%+.1f\xB0", velElv);
    dockVal(5, 5, "Vel.Elv:", buf, fg, bg);

    // Row 6 — Nos.Off: total angular offset of nose from port (cache slot 6)
    // Combined bearing + elevation error via Pythagorean approximation (accurate to <0.1°
    // within the ±20° operating range of the DOCK screen). Always positive — unsigned offset.
    {
        float noseOff = sqrtf(noseBrg * noseBrg + noseElv * noseElv);
        angCol(noseOff, fg, bg);
        snprintf(buf, sizeof(buf), "%.1f\xB0", noseOff);
        dockVal(6, 6, "Nos.Off:", buf, fg, bg);
    }

    // Row 7 — RCS | SAS buttons (full height to screen bottom)
    {
        uint16_t ry  = TITLE_TOP + 7 * rowHFor(RP_NR);
        uint16_t rh  = SCREEN_H - ry;
        uint16_t hw  = RP_W / 2;
        uint16_t sasX = RP_X + hw;
        uint16_t sasW = RP_X + RP_W - sasX;

        // RCS button (slot 8)
        {
            bool rcsOn = state.rcs_on;
            String rcsStr = rcsOn ? "ON" : "OFF";
            RowCache &rc = rowCache[5][8];
            if (rc.value != rcsStr) {
                ButtonLabel btn = rcsOn
                    ? ButtonLabel{ "RCS", TFT_WHITE,     TFT_WHITE,     TFT_DARK_GREEN, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }
                    : ButtonLabel{ "RCS", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK,  TFT_OFF_BLACK,  TFT_GREY, TFT_GREY };
                drawButton(tft, RP_X, ry, hw, rh, btn, RP_LBL, false);
                rc.value = rcsStr;
            }
        }

        // SAS button (slot 9)
        {
            const char *v; uint16_t sasFg, sasBg;
            switch (state.sasMode) {
                case 255: v = "SAS";  sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 0:   v = "STAB"; sasFg = TFT_BLACK;     sasBg = TFT_SKY;        break;
                case 1:   v = "PRO";  sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 2:   v = "RETR"; sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 3:   v = "NRM";  sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 4:   v = "ANRM"; sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 5:   v = "RAD+"; sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 6:   v = "RAD-"; sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 7:   v = "TGT";  sasFg = TFT_WHITE;     sasBg = TFT_DARK_GREEN; break;
                case 8:   v = "ATGT"; sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 9:   v = "MNVR"; sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                default:  v = "SAS";  sasFg = TFT_DARK_GREY; sasBg = TFT_OFF_BLACK;  break;
            }
            RowCache &rc = rowCache[5][9];
            String sv = v;
            if (rc.value != sv || rc.fg != sasFg || rc.bg != sasBg) {
                ButtonLabel btn = { v, sasFg, sasFg, sasBg, sasBg, TFT_GREY, TFT_GREY };
                drawButton(tft, sasX, ry, sasW, rh, btn, RP_LBL, false);
                rc.value = sv; rc.fg = sasFg; rc.bg = sasBg;
            }
        }
    }

    // Approach bar — left panel bottom (threshold-gated internally)
    _dockDrawDistBar(tft, state.tgtDistance);

    // Redraw all dividers last — printValue fills can erase them
    // Between T+Dock(1) and V.Close(2)
    { uint16_t dy = rowYFor(2,RP_NR) - 1;
      tft.drawLine(RP_X, dy,   RP_X+RP_W, dy,   TFT_GREY);
      tft.drawLine(RP_X, dy+1, RP_X+RP_W, dy+1, TFT_GREY); }
    // Between V.Lat(3) and Vel.Brg(4)
    { uint16_t dy = rowYFor(4,RP_NR) - 1;
      tft.drawLine(RP_X, dy,   RP_X+RP_W, dy,   TFT_GREY);
      tft.drawLine(RP_X, dy+1, RP_X+RP_W, dy+1, TFT_GREY); }
    // Between Vel.Elv(5) and Nos.Off(6)
    { uint16_t dy = rowYFor(6,RP_NR) - 1;
      tft.drawLine(RP_X, dy,   RP_X+RP_W, dy,   TFT_GREY);
      tft.drawLine(RP_X, dy+1, RP_X+RP_W, dy+1, TFT_GREY); }
    // Before buttons(7)
    { uint16_t dy = rowYFor(7,RP_NR) - 1;
      tft.drawLine(RP_X, dy,   RP_X+RP_W, dy,   TFT_GREY);
      tft.drawLine(RP_X, dy+1, RP_X+RP_W, dy+1, TFT_GREY); }
    // Panel left border
    tft.drawLine(RP_X - 2, TITLE_TOP, RP_X - 2, SCREEN_H, TFT_GREY);
    tft.drawLine(RP_X - 1, TITLE_TOP, RP_X - 1, SCREEN_H, TFT_GREY);

    if (debugMode) {
        uint32_t _dt = micros() - _t0;
        Serial.print("DOCK total=");
        Serial.print((float)_dt / 1000.0f, 2);
        Serial.print("ms  dist="); Serial.print(state.tgtDistance, 1);
        Serial.print("m  vc=");    Serial.println(state.tgtVelocity, 2);
    }
}
