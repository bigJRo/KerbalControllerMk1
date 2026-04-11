/***************************************************************************************
   Screen_DOCK.ino  —  Docking screen: graphical approach reticle + critical numbers

   LAYOUT (800×480, content area 720×418 below title bar)
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

   RETICLE GEOMETRY (screen coords)
   ──────────────────────────────────
   Centre: (192, 271)  Radius: 170px  Scale: 170/30 = 5.67 px/deg
   Rings: ±5° r=28, ±10° r=57, ±20° r=113, ±30° r=170
   Left panel: x=0..384  Right panel: x=385..719

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


// ── Reticle geometry constants ────────────────────────────────────────────────────────
static const uint16_t RET_CX   = 192;   // reticle centre x (screen)
static const uint16_t RET_CY   = 251;   // reticle centre y — raised vs screen centre to give legend room
static const uint16_t RET_R    = 170;   // reticle radius (px)
static const float    RET_SCALE = (float)RET_R / 20.0f;  // px per degree (8.5) — ±20° full scale

// Ring radii for ±5°, ±10°, ±20°, ±30°
static const uint16_t RING_5  = 42;   // 5° × 8.5px
static const uint16_t RING_10 = 85;   // 10° × 8.5px
static const uint16_t RING_15 = 127;  // 15° × 8.5px (replaces old 20° ring)
static const uint16_t RING_20 = RET_R;  // 20° = full radius

// Dot display sizes
static const uint8_t DOT_R_PORT  = 8;    // target port dot radius
static const uint8_t DOT_R_VEL   = 6;    // velocity vector dot radius
static const uint8_t DOT_R_ERASE = 12;   // erase rect half-size (covers dot + any overdraw)

// Right panel geometry
static const uint16_t RP_X   = 385;     // right panel left edge
static const uint16_t RP_W   = 335;     // right panel width
static const uint8_t  RP_NR  = 7;       // number of rows
static const tFont   *RP_F   = &Roboto_Black_32;  // value font (32pt=38px height)

// Approach bar geometry (row 7 bottom)
static const uint16_t BAR_X  = 19;    // 90% bar — (LEFT_W - BAR_W)/2 margin
static const uint16_t BAR_W  = 346;   // 90% of 385px left panel
static const uint16_t BAR_H  = 22;   // taller bar, visible at docking distance
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
static void _dockDrawReticleChrome(RA8875 &tft) {
    // Black disc background
    tft.fillCircle(RET_CX, RET_CY, RET_R, TFT_BLACK);

    // Inner good-zone fill (subtle dark green inside ±5°)
    tft.fillCircle(RET_CX, RET_CY, RING_5, TFT_OFF_BLACK);

    // Concentric rings at 5° increments
    tft.drawCircle(RET_CX, RET_CY, RING_5,  TFT_DARK_GREEN);   // ±5° — good zone
    tft.drawCircle(RET_CX, RET_CY, RING_10, TFT_DARK_GREY);    // ±10°
    tft.drawCircle(RET_CX, RET_CY, RING_15, TFT_DARK_GREY);    // ±15°
    tft.drawCircle(RET_CX, RET_CY, RING_20, TFT_GREY);         // ±20° boundary

    // Cardinal lines: full-diameter lines at 0°/90°/180°/270°, dim grey
    // These extend across the full circle, split by a small gap at centre for the crosshair symbol
    uint16_t gap = 18, arm = RET_R - 1;
    // Vertical (0°/180°): top to bottom
    tft.drawLine(RET_CX, RET_CY - arm, RET_CX, RET_CY - gap, TFT_DARK_GREY);
    tft.drawLine(RET_CX, RET_CY + gap, RET_CX, RET_CY + arm, TFT_DARK_GREY);
    // Horizontal (90°/270°): left to right
    tft.drawLine(RET_CX - arm, RET_CY, RET_CX - gap, RET_CY, TFT_DARK_GREY);
    tft.drawLine(RET_CX + gap, RET_CY, RET_CX + arm, RET_CY, TFT_DARK_GREY);

    // Crosshair symbol at centre (your nose — always fixed)
    tft.drawLine(RET_CX - gap + 2, RET_CY, RET_CX - 4, RET_CY, TFT_GREY);
    tft.drawLine(RET_CX + 4,       RET_CY, RET_CX + gap - 2, RET_CY, TFT_GREY);
    tft.drawLine(RET_CX, RET_CY - gap + 2, RET_CX, RET_CY - 4, TFT_GREY);
    tft.drawLine(RET_CX, RET_CY + 4,       RET_CX, RET_CY + gap - 2, TFT_GREY);
    tft.fillCircle(RET_CX, RET_CY, 2, TFT_GREY);

    // Minor ticks at 30° increments on the outer ring (skipping cardinals at 0/90/180/270)
    for (uint16_t deg = 30; deg < 360; deg += 30) {
        if (deg % 90 == 0) continue;  // cardinals already drawn as full lines
        float rad = (deg - 90) * DEG_TO_RAD;  // -90 converts to 0=top convention
        int16_t ox = RET_CX + (int16_t)(RET_R * cosf(rad));
        int16_t oy = RET_CY + (int16_t)(RET_R * sinf(rad));
        int16_t ix = RET_CX + (int16_t)((RET_R - 14) * cosf(rad));
        int16_t iy = RET_CY + (int16_t)((RET_R - 14) * sinf(rad));
        tft.drawLine(ox, oy, ix, iy, TFT_DARK_GREY);
    }

    // Ring degree labels (positioned just inside each ring, in the NE quadrant)
    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(RET_CX + 3, RET_CY - RING_5  + 3);  tft.print("5");
    tft.setCursor(RET_CX + 3, RET_CY - RING_10 + 3);  tft.print("10");
    tft.setCursor(RET_CX + 3, RET_CY - RING_15 + 3);  tft.print("15");
    tft.setCursor(RET_CX + 3, RET_CY - RING_20 + 3);  tft.print("20");

    // Bezel ring
    tft.drawCircle(RET_CX, RET_CY, RET_R,     TFT_GREY);
    tft.drawCircle(RET_CX, RET_CY, RET_R + 1, TFT_DARK_GREY);

    // Legend: 3 rows stacked in top-left corner, above/beside the circle top
    // y=68,88,108 — all safely left of circle edge at those y positions
    static const uint16_t LEG_X  = 6;
    static const uint16_t LEG_Y0 = TITLE_TOP + 6;  // 68
    static const uint16_t LEG_DY = 20;              // row spacing

    tft.setFont(&Roboto_Black_12);

    // Row 0: VEL — hollow green circle
    tft.drawCircle(LEG_X + 6, LEG_Y0 + 6, 5, TFT_NEON_GREEN);
    tft.setTextColor(TFT_SAP_GREEN, TFT_BLACK);
    tft.setCursor(LEG_X + 16, LEG_Y0);
    tft.print("VEL");

    // Row 1: PORT — solid magenta diamond
    tft.fillTriangle(LEG_X+6, LEG_Y0+LEG_DY+1,  LEG_X+12, LEG_Y0+LEG_DY+7,
                     LEG_X+6, LEG_Y0+LEG_DY+13, TFT_VIOLET);
    tft.fillTriangle(LEG_X+6, LEG_Y0+LEG_DY+1,  LEG_X,    LEG_Y0+LEG_DY+7,
                     LEG_X+6, LEG_Y0+LEG_DY+13, TFT_VIOLET);
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

    // Bottom-left: APPROACH bar (90% width, centred, with label row above)
    uint16_t barY = RET_CY + RET_R + 20;   // 441 — bar top
    uint16_t lblY = barY - 16;              // 425 — label row above bar
    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(BAR_X, lblY);
    tft.print("APPROACH");
    tft.drawRect(BAR_X, barY, BAR_W, BAR_H, TFT_GREY);
}


// ── Draw right-panel chrome (static labels) ───────────────────────────────────────────
static void _dockDrawRightChrome(RA8875 &tft) {
    // Rows 0-5: printDispChrome draws the label; printValue draws the value to its right.
    printDispChrome(tft, &Roboto_Black_20, RP_X, rowYFor(0,RP_NR), RP_W, rowHFor(RP_NR), "Dist:",    COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, &Roboto_Black_20, RP_X, rowYFor(1,RP_NR), RP_W, rowHFor(RP_NR), "V.Close:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, &Roboto_Black_20, RP_X, rowYFor(2,RP_NR), RP_W, rowHFor(RP_NR), "Drift.H:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, &Roboto_Black_20, RP_X, rowYFor(3,RP_NR), RP_W, rowHFor(RP_NR), "Drift.V:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, &Roboto_Black_20, RP_X, rowYFor(4,RP_NR), RP_W, rowHFor(RP_NR), "Nos.Brg:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, &Roboto_Black_20, RP_X, rowYFor(5,RP_NR), RP_W, rowHFor(RP_NR), "Nos.Elv:", COL_LABEL, COL_BACK, COL_NO_BDR);

    // Divider before RCS/SAS row
    uint16_t divY = rowYFor(6, RP_NR) - 1;
    tft.drawLine(RP_X, divY,   RP_X + RP_W, divY,   TFT_GREY);
    tft.drawLine(RP_X, divY+1, RP_X + RP_W, divY+1, TFT_GREY);

    // Row 6: RCS (left half) | SAS (right half)
    uint16_t halfW = RP_W / 2;
    printDispChrome(tft, &Roboto_Black_20, RP_X,          rowYFor(6,RP_NR), halfW, rowHFor(RP_NR), "RCS:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, &Roboto_Black_20, RP_X + halfW,  rowYFor(6,RP_NR), halfW, rowHFor(RP_NR), "SAS:", COL_LABEL, COL_BACK, COL_NO_BDR);
    // Vertical divider between RCS and SAS (2px)
    tft.drawLine(RP_X + halfW,     rowYFor(6,RP_NR), RP_X + halfW,     SCREEN_H - 1, TFT_GREY);
    tft.drawLine(RP_X + halfW + 1, rowYFor(6,RP_NR), RP_X + halfW + 1, SCREEN_H - 1, TFT_GREY);
}


// ── Update dots — erase old, draw new ────────────────────────────────────────────────
// ── Ring/crosshair redraw after dot erase ─────────────────────────────────────────────
// After a fillRect erase, any static chrome (rings, crosshair) that intersected
// the erase box must be redrawn. Ring intersects erase box if:
//   closest point on ring to box centre is within the box,
//   AND farthest corner of box from centre is beyond the ring radius.
// Simplified: check if box overlaps the annulus [r-1, r+1].
static void _dockRepairChrome(RA8875 &tft, int16_t bx, int16_t by, uint8_t bh) {
    // bx,by = erase box top-left, bh = erase box half-size (so w=h=2*bh+1)
    // Box bounds: bx..bx+2*bh, by..by+2*bh
    int16_t boxX0=bx, boxX1=bx+2*bh, boxY0=by, boxY1=by+2*bh;

    // For each ring: check if the ring circle intersects the box
    // Closest point on box to ring centre (RET_CX, RET_CY):
    float cx = (float)constrain((int)RET_CX, (int)boxX0, (int)boxX1);
    float cy = (float)constrain((int)RET_CY, (int)boxY0, (int)boxY1);
    float distToCentre = sqrtf((cx-RET_CX)*(cx-RET_CX) + (cy-RET_CY)*(cy-RET_CY));
    // Farthest corner from ring centre:
    float dx = fmaxf(fabsf(boxX0-RET_CX), fabsf(boxX1-RET_CX));
    float dy = fmaxf(fabsf(boxY0-RET_CY), fabsf(boxY1-RET_CY));
    float distFar = sqrtf(dx*dx + dy*dy);

    static const uint16_t rings[]  = {RING_5, RING_10, RING_15, RING_20};
    static const uint16_t rcols[]  = {TFT_DARK_GREEN, TFT_DARK_GREY, TFT_DARK_GREY, TFT_GREY};
    for (uint8_t i = 0; i < 4; i++) {
        float r = (float)rings[i];
        if (distToCentre <= r + 1.5f && distFar >= r - 1.5f) {
            tft.drawCircle(RET_CX, RET_CY, rings[i], rcols[i]);
        }
    }

    // Crosshair / cardinal lines: redraw anything in the erase box
    // Cardinal lines span full radius; crosshair symbol is the inner gap region
    static const uint16_t cgap = 18, carm = RET_R - 1;
    // Horizontal cardinal segments
    if (boxY0 <= RET_CY && RET_CY <= boxY1) {
        if (boxX0 < (int16_t)(RET_CX - cgap))
            tft.drawLine(RET_CX - carm, RET_CY, RET_CX - cgap, RET_CY, TFT_DARK_GREY);
        if (boxX1 > (int16_t)(RET_CX + cgap))
            tft.drawLine(RET_CX + cgap, RET_CY, RET_CX + carm, RET_CY, TFT_DARK_GREY);
        // Crosshair symbol inner segments
        if (boxX0 <= RET_CX || boxX1 >= RET_CX) {
            tft.drawLine(RET_CX - cgap + 2, RET_CY, RET_CX - 4, RET_CY, TFT_GREY);
            tft.drawLine(RET_CX + 4, RET_CY, RET_CX + cgap - 2, RET_CY, TFT_GREY);
        }
    }
    // Vertical cardinal segments
    if (boxX0 <= RET_CX && RET_CX <= boxX1) {
        if (boxY0 < (int16_t)(RET_CY - cgap))
            tft.drawLine(RET_CX, RET_CY - carm, RET_CX, RET_CY - cgap, TFT_DARK_GREY);
        if (boxY1 > (int16_t)(RET_CY + cgap))
            tft.drawLine(RET_CX, RET_CY + cgap, RET_CX, RET_CY + carm, TFT_DARK_GREY);
        // Crosshair symbol inner segments
        if (boxY0 <= RET_CY || boxY1 >= RET_CY) {
            tft.drawLine(RET_CX, RET_CY - cgap + 2, RET_CX, RET_CY - 4, TFT_GREY);
            tft.drawLine(RET_CX, RET_CY + 4, RET_CX, RET_CY + cgap - 2, TFT_GREY);
        }
    }
    // Centre dot
    if (boxX0 <= RET_CX && RET_CX <= boxX1 && boxY0 <= RET_CY && RET_CY <= boxY1)
        tft.fillCircle(RET_CX, RET_CY, 2, TFT_GREY);

    // Inner good-zone fill (if erase hit the ±5° ring interior)
    if (distToCentre <= RING_5 - 1) {
        tft.fillCircle(RET_CX, RET_CY, RING_5 - 1, TFT_OFF_BLACK);
        tft.drawCircle(RET_CX, RET_CY, RING_5, TFT_DARK_GREEN);
    }
}


// ── Update dots: erase old, repair chrome, draw new ──────────────────────────────────
// Port dot:  solid filled diamond (magenta)
// Vel dot:   hollow circle outline only (green) — shows through if port is inside
static void _dockUpdateDots(RA8875 &tft, float noseBrg, float noseElv,
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
        // Solid diamond: 4 filled triangles
        uint8_t ds = DOT_R_PORT + 3;  // diamond half-size
        tft.fillTriangle(portSX, portSY-ds, portSX+ds, portSY, portSX, portSY+ds, TFT_VIOLET);
        tft.fillTriangle(portSX, portSY-ds, portSX-ds, portSY, portSX, portSY+ds, TFT_VIOLET);
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
        // Hollow circle: just outlines (so port diamond shows through if overlapping)
        tft.drawCircle(velSX, velSY, DOT_R_VEL,     TFT_NEON_GREEN);
        tft.drawCircle(velSX, velSY, DOT_R_VEL + 1, TFT_SAP_GREEN);
        _dockPrevVelX = velSX; _dockPrevVelY = velSY;
    }

    // Always redraw vel dot unconditionally so it is always on top of the port diamond.
    // Cost: 2 drawCircle = negligible. Guarantees vel is never buried under port.
    tft.drawCircle(velSX, velSY, DOT_R_VEL,     TFT_NEON_GREEN);
    tft.drawCircle(velSX, velSY, DOT_R_VEL + 1, TFT_SAP_GREEN);

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


static void _dockDrawDistBar(RA8875 &tft, float dist) {
    // barY must match what _dockDrawReticleChrome drew
    static const uint16_t barY = RET_CY + RET_R + 20;  // 441
    static const uint16_t lblY = barY - 16;             // 425 — label row above bar

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

    // Distance value: right-aligned at right edge of bar, on the label row above bar
    char buf[12];
    if      (dist >= 1000.0f) snprintf(buf, sizeof(buf), "%.1fkm", dist/1000.0f);
    else if (dist >= 100.0f)  snprintf(buf, sizeof(buf), "%.0fm",  dist);
    else                      snprintf(buf, sizeof(buf), "%.1fm",  dist);

    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(barCol, TFT_BLACK);
    // Clear right half of label row then right-align distance text
    tft.fillRect(BAR_X + BAR_W/2, lblY, BAR_W/2, 14, TFT_BLACK);
    int16_t tw = getFontStringWidth(&Roboto_Black_12, buf);
    tft.setCursor(BAR_X + BAR_W - tw, lblY);
    tft.print(buf);
}

// ── CHROME ────────────────────────────────────────────────────────────────────────────
static void chromeScreen_DOCK(RA8875 &tft) {
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
    _dockPrevPortX = 9999;  // also resets dots (already done above, belt+suspenders)
}


// ── DRAW (called every loop) ──────────────────────────────────────────────────────────
static void drawScreen_DOCK(RA8875 &tft) {
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

    // Lateral drift decomposition (horizontal and vertical components)
    float v_yaw = 0.0f, v_pitch_drift = 0.0f, v_lat_mag = 0.0f;
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
        float v_lat[3] = { vel_vec[0]-v_app*tgt_unit[0], vel_vec[1]-v_app*tgt_unit[1], vel_vec[2]-v_app*tgt_unit[2] };
        float world_up[3] = {0,0,1};
        float right[3] = { tgt_unit[1]*world_up[2]-tgt_unit[2]*world_up[1],
                            tgt_unit[2]*world_up[0]-tgt_unit[0]*world_up[2],
                            tgt_unit[0]*world_up[1]-tgt_unit[1]*world_up[0] };
        float r_mag = sqrtf(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);
        if (r_mag > 0.001f) { right[0]/=r_mag; right[1]/=r_mag; right[2]/=r_mag; }
        else { right[0]=1; right[1]=0; right[2]=0; }
        float up[3] = { right[1]*tgt_unit[2]-right[2]*tgt_unit[1],
                         right[2]*tgt_unit[0]-right[0]*tgt_unit[2],
                         right[0]*tgt_unit[1]-right[1]*tgt_unit[0] };
        v_yaw         = v_lat[0]*right[0]+v_lat[1]*right[1]+v_lat[2]*right[2];
        v_pitch_drift = v_lat[0]*up[0]   +v_lat[1]*up[1]   +v_lat[2]*up[2];
        v_lat_mag     = sqrtf(v_lat[0]*v_lat[0]+v_lat[1]*v_lat[1]+v_lat[2]*v_lat[2]);
        if (fabsf(v_yaw)         < 0.005f) v_yaw = 0.0f;
        if (fabsf(v_pitch_drift) < 0.005f) v_pitch_drift = 0.0f;
    }

    // ── Update reticle dots ───────────────────────────────────────────────────────────
    _dockUpdateDots(tft, noseBrg, noseElv, velBrg, velElv);

    // ── Right panel values ────────────────────────────────────────────────────────────
    char buf[20];
    uint16_t fg, bg;

    // Helper: cache-checked drawValue for dock screen (index 5)
    auto dockVal = [&](uint8_t row, const char *label, const String &val,
                        uint16_t fgc, uint16_t bgc) {
        RowCache &rc = rowCache[5][row];
        if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
        printValue(tft, RP_F, RP_X, rowYFor(row, RP_NR), RP_W, rowHFor(RP_NR),
                   label, val, fgc, bgc, COL_BACK, printState[5][row]);
        rc.value = val; rc.fg = fgc; rc.bg = bgc;
    };

    // Row 0 — Distance
    if      (state.tgtDistance < DOCK_DIST_ALARM_M) { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (state.tgtDistance < DOCK_DIST_WARN_M)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                                              { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    dockVal(0, "Dist:", formatAlt(state.tgtDistance), fg, bg);

    // Row 1 — Closure velocity (negative = approaching = good)
    {
        float vc = state.tgtVelocity;
        bool closing = (vc < 0.0f);
        if (!closing && fabsf(vc) > DOCK_VCLOSURE_ALARM_MS && state.tgtDistance < DOCK_VCLOSURE_ALARM_DIST_M) {
            fg = TFT_WHITE; bg = TFT_RED;
        } else if (closing && fabsf(vc) > DOCK_VCLOSURE_ALARM_MS && state.tgtDistance < DOCK_VCLOSURE_ALARM_DIST_M) {
            fg = TFT_WHITE; bg = TFT_RED;
        } else {
            fg = closing ? TFT_DARK_GREEN : TFT_YELLOW;
            bg = TFT_BLACK;
        }
        snprintf(buf, sizeof(buf), "%+.2f m/s", vc);
        dockVal(1, "V.Close:", buf, fg, bg);
    }

    // Drift colour helper
    auto driftCol = [](float v, uint16_t &fg, uint16_t &bg) {
        float av = fabsf(v);
        if      (av >= DOCK_DRIFT_ALARM_MS) { fg = TFT_WHITE;     bg = TFT_RED;   }
        else if (av >= DOCK_DRIFT_WARN_MS)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
        else                                 { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    };

    // Row 2 — Drift H
    driftCol(v_yaw, fg, bg);
    snprintf(buf, sizeof(buf), "%+.2f m/s", v_yaw);
    dockVal(2, "Drift.H:", buf, fg, bg);

    // Row 3 — Drift V
    driftCol(v_pitch_drift, fg, bg);
    snprintf(buf, sizeof(buf), "%+.2f m/s", v_pitch_drift);
    dockVal(3, "Drift.V:", buf, fg, bg);

    // Angle error colour helper
    auto angCol = [](float e, uint16_t &fg, uint16_t &bg) {
        float ae = fabsf(e);
        if      (ae >= DOCK_BRG_ALARM_DEG) { fg = TFT_WHITE;     bg = TFT_RED;   }
        else if (ae >= DOCK_BRG_WARN_DEG)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
        else                                { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    };

    // Row 4 — Nose bearing error
    angCol(noseBrg, fg, bg);
    snprintf(buf, sizeof(buf), "%+.1f\xB0", noseBrg);
    dockVal(4, "Nos.Brg:", buf, fg, bg);

    // Row 5 — Nose elevation error
    angCol(noseElv, fg, bg);
    snprintf(buf, sizeof(buf), "%+.1f\xB0", noseElv);
    dockVal(5, "Nos.Elv:", buf, fg, bg);

    // Row 6 — RCS | SAS
    {
        uint16_t halfW = RP_W / 2;
        uint16_t y6 = rowYFor(6, RP_NR), h6 = rowHFor(RP_NR);

        const char *rcsStr = state.rcs_on ? "ON"  : "OFF";
        uint16_t    rcsFg  = state.rcs_on ? TFT_DARK_GREEN : TFT_WHITE;
        uint16_t    rcsBg  = state.rcs_on ? TFT_BLACK      : TFT_RED;
        {
            RowCache &rc = rowCache[5][6];
            String s = rcsStr;
            if (rc.value != s || rc.fg != rcsFg || rc.bg != rcsBg) {
                printValue(tft, RP_F, RP_X, y6, halfW, h6,
                           "RCS:", s, rcsFg, rcsBg, COL_BACK, printState[5][6]);
                rc.value = s; rc.fg = rcsFg; rc.bg = rcsBg;
            }
        }

        const char *sasStr;
        uint16_t    sasFg, sasBg = TFT_BLACK;
        switch (state.sasMode) {
            case 255: sasStr = "OFF";   sasFg = TFT_WHITE;      sasBg = TFT_RED;   break; // SAS off — always alarm
            case 0:   sasStr = "STAB";  sasFg = TFT_SKY;                            break; // Stability — neutral
            case 1:   sasStr = "PROG";  sasFg = TFT_RED;                            break; // Prograde — wrong for dock
            case 2:   sasStr = "RETR";  sasFg = TFT_RED;                            break; // Retrograde — wrong
            case 3:   sasStr = "NORM";  sasFg = TFT_RED;                            break; // Normal — wrong
            case 4:   sasStr = "ANRM";  sasFg = TFT_RED;                            break; // Anti-normal — wrong
            case 5:   sasStr = "ROUT";  sasFg = TFT_RED;                            break; // Radial out — wrong
            case 6:   sasStr = "RINX";  sasFg = TFT_RED;                            break; // Radial in — wrong
            case 7:   sasStr = "TGT";   sasFg = TFT_DARK_GREEN;                     break; // Target — correct for docking
            case 8:   sasStr = "ATGT";  sasFg = TFT_RED;                            break; // Anti-target — wrong
            case 9:   sasStr = "MNV";   sasFg = TFT_RED;                            break; // Maneuver — wrong
            default:  sasStr = "---";   sasFg = TFT_DARK_GREY;                      break;
        }
        {
            RowCache &rc = rowCache[5][7];
            String s = sasStr;
            if (rc.value != s || rc.fg != sasFg || rc.bg != sasBg) {
                // Explicitly clear the entire SAS half-row before redrawing.
                // printValue only clears on background colour change — same-bg
                // transitions (e.g. ATGT->TGT) leave leftover pixels otherwise.
                tft.fillRect(RP_X + halfW, y6, halfW, h6, TFT_BLACK);
                printDispChrome(tft, &Roboto_Black_20, RP_X + halfW, y6, halfW, h6, "SAS:", COL_LABEL, COL_BACK, COL_NO_BDR);
                printValue(tft, RP_F, RP_X + halfW, y6, halfW, h6,
                           "SAS:", s, sasFg, sasBg, COL_BACK, printState[5][7]);
                rc.value = s; rc.fg = sasFg; rc.bg = sasBg;
            }
        }
    }

    // Approach bar — left panel bottom (threshold-gated internally)
    _dockDrawDistBar(tft, state.tgtDistance);

    // Redraw dividers last so printValue fills never overwrite them
    uint16_t _divY = rowYFor(6, RP_NR) - 1;
    tft.drawLine(RP_X, _divY,   RP_X + RP_W, _divY,   TFT_GREY);
    tft.drawLine(RP_X, _divY+1, RP_X + RP_W, _divY+1, TFT_GREY);
    tft.drawLine(RP_X + RP_W/2,     rowYFor(6,RP_NR), RP_X + RP_W/2,     SCREEN_H - 1, TFT_GREY);
    tft.drawLine(RP_X + RP_W/2 + 1, rowYFor(6,RP_NR), RP_X + RP_W/2 + 1, SCREEN_H - 1, TFT_GREY);
    tft.drawLine(RP_X - 2, TITLE_TOP, RP_X - 2, SCREEN_H, TFT_GREY);
    tft.drawLine(RP_X - 1, TITLE_TOP, RP_X - 1, SCREEN_H, TFT_GREY);
}
