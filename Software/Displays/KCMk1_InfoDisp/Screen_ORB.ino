/***************************************************************************************
   Screen_ORB.ino  —  Orbit display (left panel) + Inclination display (right panel)

   ── Architecture (v2 — three-layer redraw) ─────────────────────────────────────────
   Three layers, three different update strategies. Trying to use one strategy for all
   of them is what broke the previous implementation.

     LAYER 1 — Static chrome
       Panel divider, title labels, readout strip labels. Drawn once in
       chromeScreen_ORB, never touched again.

     LAYER 2 — Orbit scene (ellipse/hyperbola, body, atmosphere, Pe/Ap/AN/DN markers,
                            SOI ring, INCL orbit line + body)
       Treated as a single indivisible unit. A "scene signature" is computed each
       frame from the parameters that affect drawing. When the signature changes
       AND a minimum interval has elapsed since the last scene repaint, the entire
       panel rect is fillRect-ed to black and the whole scene is redrawn from
       scratch. No per-element erase, no cached pixel positions for markers, no
       black-tracing ellipses.

       Quantization is the hysteresis — scene changes fire only when a quantized
       field flips, not on floating-point jitter. The minimum interval prevents
       strobing during sustained maneuvering.

       PLAN panel and INCL panel are independent scenes with independent signatures
       and independent minimum-interval gates.

     LAYER 3 — Vessel dot (both panels)
       Small filled circle — the one shape where sprite-erase works cleanly.
       fillCircle(TFT_BLACK) on the cached previous position, redraw a short
       3-segment arc of the orbit curve locally to patch the erased line, then
       draw the new dot. Updates every frame independently of Layer 2.

   ── Geometry (unchanged from v1) ───────────────────────────────────────────────────
   Left panel — Orbit shape (ORBIT):
     Ellipse CENTRE at (ORB_PCX, ORB_CY). a_px = MAX_R always.
     b_px = MAX_R * sqrt(1-e²).  Body (focus) offset from centre by a*ecc toward Pe.
     Pe always MAX_R from centre (in argOfPe direction).
     Ap always MAX_R from centre (in argOfPe+180 direction).
     Body position: focusX = ORB_PCX + MAX_R*ecc*cos(argOfPe)
                    focusY = ORB_CY  - MAX_R*ecc*sin(argOfPe)
     Body pixel scale: body_px = (bodyR_m / sma_m) * MAX_R

   Escape geometry:
     Body at (ORB_PCX, ORB_CY). Scale = 3x body radius = panel half-extent.
     Switches to SOI view when altSL > 2*bodyR.

   Right panel — Orbital inclination (INCL):
     Edge-on view. Equatorial plane = dashed horizontal at INC_CY.
     Orbit line through body centre at angle = inclination.
     AN (TFT_CYAN) at upper-right end (matches planar view where AN is east/right).
     DN (TFT_YELLOW) at lower-left end.
     Vessel (TFT_NEON_GREEN) at sin(argPe+nu)*INC_L along the orbit line.

   Orbit colour:  Green=normal  Yellow=Pe/Ap in atmo or underground  Orange=escape
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

// ── Layout ────────────────────────────────────────────────────────────────────────────
static const uint16_t ORB_TITLE_TOP  = 62;
static const uint16_t ORB_SCREEN_H   = 480;
static const uint16_t ORB_MAX_R      = 150;   // orbit half-extent (pixels)

static const int16_t  ORB_PCX = 180;
static const int16_t  ORB_ICX = 540;
static const int16_t  ORB_CY  = 233;

// Header strip — protected from scene repaints. Holds ORBIT/INCL labels and
// the SOI body name. Scene repaints fillRect below this; chrome and the
// Header strip — contains the ORBIT / INCL panel-title labels and the SOI
// body name. Laid at y=62..92.
static const int16_t  HDR_Y0   = 62;   // top of header strip (ORB_TITLE_TOP)
static const int16_t  HDR_Y1   = 92;   // bottom of header strip

// PLAN panel drawing bounds vs. erase bounds:
//   PLAN_Y0/PLAN_Y1      — drawing bounds (markers, dots, visibility checks).
//   PLAN_FILL_Y0         — erase top edge for scene repaint (= HDR_Y0).
//                          Wider than the drawing bounds so atmo/body pixels
//                          that naturally extend up into the header strip get
//                          erased cleanly. The chrome labels are re-drawn
//                          inside the repaint function after the fillRect.
//   PLAN_CLIP_Y0/Y1      — inset for marker LABEL placement so labels don't
//                          drift above the drawing area.
static const int16_t  PLAN_X0  = 6,   PLAN_X1 = 354;
static const int16_t  PLAN_Y0  = HDR_Y1;       // 92 — drawing bound
static const int16_t  PLAN_Y1  = 400;
static const int16_t  PLAN_FILL_Y0 = HDR_Y0;   // 62 — wide erase bound
static const int16_t  PLAN_CLIP_Y0 = HDR_Y1 + 2;   // 94
static const int16_t  PLAN_CLIP_Y1 = PLAN_Y1 - 2;  // 398

// Readout strip (bottom-left of PLAN panel, Roboto_Black_20)
static const uint16_t ORB_RDY1 = 404;  // Alt.SL row
static const uint16_t ORB_RDY2 = 430;  // Pe row
static const uint16_t ORB_RDY3 = 456;  // Ap row

// ── INCL panel layout (right half, x=[362,719]) ───────────────────────────────────────
static const int16_t  INC_CX   = 540;  // panel centre x
static const int16_t  INC_CY   = 233;  // panel centre y (shared with ORB_CY)
// Orbit line half-length. Reduced from 150 to 130 to clear the 4-row readout
// strip below (top row = Inc at y=378). At 90° inclination, line extends
// bodyCY ± INC_L = 233 ± 130 = 103..363, giving ~13px clearance above the
// readout strip.
static const uint16_t INC_L    = 130;
static const uint16_t INC_BR   =  20;  // body disc radius (px)

static const int16_t  INCL_X0 = 362, INCL_X1 = 719;
static const int16_t  INCL_Y0 = HDR_Y1;
// INCL panel extends down only to y=374 to leave space for the 4th readout
// row at y=378. PLAN panel remains 3 rows (y=404..456 strip).
static const int16_t  INCL_Y1 = 374;
// INCL graphics never reach above y=92 (body at y=233, INC_L=130 means top is
// y=103), so the fillRect matches the drawing bound exactly — no need to
// erase into the header strip, which would force a body-name redraw.
static const int16_t  INCL_FILL_Y0 = INCL_Y0;
static const int16_t  INCL_CLIP_Y0 = HDR_Y1 + 2;
static const int16_t  INCL_CLIP_Y1 = INCL_Y1 - 2;

// Readout strip (bottom-right)
// Order: Inc (4th row, top) / PRD / Arg.Pe / T+Pe or T+Ap
static const uint16_t INC_RDY0 = 378;  // Inc (inclination) row — NEW, top
static const uint16_t INC_RDY1 = 404;  // PRD (period) row
static const uint16_t INC_RDY2 = 430;  // Arg.Pe row
static const uint16_t INC_RDY3 = 456;  // T+Pe or T+Ap row
static const uint16_t INC_LABEL_X = 366;
static const uint16_t INC_VALUE_X = 452;

// ── Body parameters ───────────────────────────────────────────────────────────────────
// Body colors are NOT stored here — see kspBodyColor() / kspAtmoColor() in
// AAA_Screens.ino. ORB and LNCH (Circularization) both use those shared lookups,
// so a body-color tweak only needs to happen in one place.
struct OrbBody {
    const char *name;
    float    radius_m;
    float    atmo_m;
    float    soi_m;       // sphere of influence radius (0 = no limit, e.g. Kerbol)
};

static const OrbBody ORB_BODIES[] = {
    { "Kerbol",261600000.0f,     0.0f,          0.0f },  // 0
    { "Moho",    250000.0f,     0.0f,    9646663.0f },  // 1
    { "Eve",     700000.0f, 90000.0f,   85109365.0f },  // 2
    { "Gilly",    13000.0f,     0.0f,     126123.0f },  // 3
    { "Kerbin",  600000.0f, 70000.0f,   84159286.0f },  // 4
    { "Mun",     200000.0f,     0.0f,    2429559.0f },  // 5
    { "Minmus",   60000.0f,     0.0f,    2247428.0f },  // 6
    { "Duna",    320000.0f, 50000.0f,   47921949.0f },  // 7
    { "Ike",     130000.0f,     0.0f,    1049599.0f },  // 8
    { "Dres",    138000.0f,     0.0f,   32832840.0f },  // 9
    { "Jool",   6000000.0f,200000.0f, 2455985200.0f },  // 10
    { "Laythe",  500000.0f, 50000.0f,    3723645.0f },  // 11
    { "Vall",    300000.0f,     0.0f,    2406401.0f },  // 12
    { "Tylo",    600000.0f,     0.0f,   10856518.0f },  // 13
    { "Bop",      65000.0f,     0.0f,    1221061.0f },  // 14
    { "Pol",      44000.0f,     0.0f,    1042138.0f },  // 15
    { "Eeloo",   210000.0f,     0.0f,  119082942.0f },  // 16
};

// ── Compatibility stub ────────────────────────────────────────────────────────────────
// _orbAdvancedMode is still referenced by AAA_Globals.ino (resets on screen
// exit) and SimpitHandler.ino (resets on vessel switch) even though ORB no
// longer toggles modes via title-bar tap. Kept as a no-op placeholder so those
// files continue to link. Can be removed when those call sites are cleaned up.
bool          _orbAdvancedMode = false;

// ── Tuning constants ──────────────────────────────────────────────────────────────────
// Scene-signature quantization bin sizes (produce hysteresis).
static const float SIG_ARGPE_DEG  = 1.0f;     // 1° bins
static const float SIG_ECC        = 0.01f;    // 0.01 bins
static const float SIG_INCL_DEG   = 0.5f;     // 0.5° bins
static const float SIG_APS_FRAC   = 0.01f;    // 1% of SMA bins

// Minimum interval between scene repaints (ms). Acts as a floor — sustained
// parameter changes (burns) cap at 2 Hz scene updates, which reads as
// professional rather than strobing. Below ~100 ms the gate has no effect
// because the repaint itself takes longer than that.
static const uint32_t SCENE_MIN_INTERVAL_MS = 500;

// Vessel-dot pixel movement threshold before redraw.
static const int VESSEL_PX_THRESH = 2;

// Debug timing output — one line per scene repaint, one line per N vessel updates.
static const uint32_t DBG_VESSEL_EVERY = 20;  // log one vessel timing per 20 updates

// ── Scene signature ───────────────────────────────────────────────────────────────────
// All fields that can affect what gets drawn in a given panel.
// Quantized where continuous so signature equality = "nothing visually different".
struct OrbSceneSig {
    uint8_t  bodyIdx;
    bool     isEscape;
    bool     useSoiView;
    uint16_t orbitCol;
    int16_t  argPe_q;     // argOfPe / SIG_ARGPE_DEG
    int16_t  ecc_q;       // ecc     / SIG_ECC
    int16_t  incl_q;      // incl    / SIG_INCL_DEG
    int16_t  rPe_q;       // (rPe / sma) / SIG_APS_FRAC, clamped
    int16_t  rAp_q;       // (rAp / sma) / SIG_APS_FRAC, clamped  (-1 for escape)

    bool operator==(const OrbSceneSig &o) const {
        return bodyIdx == o.bodyIdx && isEscape == o.isEscape
            && useSoiView == o.useSoiView && orbitCol == o.orbitCol
            && argPe_q == o.argPe_q && ecc_q == o.ecc_q
            && incl_q == o.incl_q && rPe_q == o.rPe_q && rAp_q == o.rAp_q;
    }
    bool operator!=(const OrbSceneSig &o) const { return !(*this == o); }
};

// Full numeric scene state — everything the draw functions need.
// Recomputed each frame from AppState. Stored on a scene repaint so vessel
// updates can reference the same geometry that the scene was drawn with.
struct OrbScene {
    // Derived shape
    float    ecc;
    float    argOfPe_rad;
    float    sma_m;
    float    rPe_m;
    float    bodyR_m;
    float    atmoH_m;
    float    r_pe_px;       // Pe radius in pixels (escape only; ORB_MAX_R for ellipse)
    uint16_t body_px;
    uint16_t atmo_px;
    int16_t  focusX, focusY;
    bool     isEscape;
    bool     useSoiView;
    float    soi_m;
    uint16_t bodyCol;
    uint16_t atmoCol;
    uint16_t orbitCol;
    uint8_t  bodyIdx;
    // INCL geometry
    float    incl_deg;
    int16_t  bodyCY;        // INCL body centre y (matched to planar focusY)
    float    dx, dy;        // INCL orbit line half-extents
};

// ── Forward declarations ──────────────────────────────────────────────────────────────
// Arduino IDE's auto-prototype generator (ctags) runs BEFORE the types above are
// visible, so any function that takes OrbScene / OrbSceneSig by reference will
// have a broken auto-generated prototype. Providing explicit prototypes here
// suppresses the auto-generation. Do not remove.
static void        _orbComputeScene(OrbScene &sc);
static OrbSceneSig _orbSceneSig(const OrbScene &sc);
static void        _orbDrawSOIView(RA8875 &tft, const OrbScene &sc);
static void        _orbDrawMarkers(RA8875 &tft, const OrbScene &sc);
static void        _orbDrawMarkerDotsOnly(RA8875 &tft, const OrbScene &sc);
static void        _orbDrawIncl(RA8875 &tft, const OrbScene &sc);
static bool        _orbVesselPxPlan(const OrbScene &sc, float nu_rad,
                                    int16_t &out_x, int16_t &out_y);
static void        _orbPatchArcPlan(RA8875 &tft, const OrbScene &sc, float nu_rad);
static void        _orbRepaintPlan(RA8875 &tft, const OrbScene &sc);
static void        _orbRepaintIncl(RA8875 &tft, const OrbScene &sc);

// ── Geometry helpers ──────────────────────────────────────────────────────────────────
static void _orbEllipseShape(float PeA_m, float ApA_m, float bodyR_m,
                              float &sma_m, float &ecc) {
    float rPe = fmaxf(PeA_m + bodyR_m, 1.0f);
    float rAp = fmaxf(ApA_m + bodyR_m, rPe);
    sma_m = (rPe + rAp) * 0.5f;
    ecc   = (rAp - rPe) / (rAp + rPe);
}

static void _orbEscapeShape(float PeA_m, float ecc, float bodyR_m,
                             float &sma_m) {
    float rPe = fmaxf(PeA_m + bodyR_m, 1.0f);
    sma_m = rPe / (ecc - 1.0f);
}

// Body lookup by SOI name. Returns index into ORB_BODIES or 4 (Kerbin) fallback.
static uint8_t _orbBodyIdx() {
    const char *soi = state.gameSOI.c_str();
    for (uint8_t i = 0; i < sizeof(ORB_BODIES)/sizeof(ORB_BODIES[0]); i++) {
        if (strcmp(ORB_BODIES[i].name, soi) == 0) return i;
    }
    return 4;
}

// Compute the complete scene state from AppState. Returns by out-parameter so
// drawScreen_ORB can compute once and pass to all draw functions.
static void _orbComputeScene(OrbScene &sc) {
    uint8_t          bodyIdx = _orbBodyIdx();
    const OrbBody   &body    = ORB_BODIES[bodyIdx];

    sc.bodyIdx  = bodyIdx;
    sc.bodyR_m  = body.radius_m;
    sc.atmoH_m  = body.atmo_m;
    sc.soi_m    = body.soi_m;
    // Body colors come from the shared lookups in AAA_Screens.ino so ORB and
    // LNCH (Circularization) cannot drift apart.
    sc.bodyCol  = kspBodyColor(body.name);
    sc.atmoCol  = kspAtmoColor(body.name);

    float ecc   = fmaxf(0.0f, state.eccentricity);
    bool isEsc  = (ecc >= 1.0f) || (state.apoapsis < 0.0f);
    float PeA_m = fmaxf(0.0f, state.periapsis);
    float ApA_m = fmaxf(0.0f, state.apoapsis);

    if (isEsc) {
        ecc = fmaxf(ecc, 1.001f);
        _orbEscapeShape(PeA_m, ecc, sc.bodyR_m, sc.sma_m);
    } else {
        _orbEllipseShape(PeA_m, ApA_m, sc.bodyR_m, sc.sma_m, ecc);
    }

    sc.ecc         = ecc;
    sc.isEscape    = isEsc;
    sc.argOfPe_rad = state.argOfPe * (PI / 180.0f);
    sc.rPe_m       = fmaxf(PeA_m + sc.bodyR_m, 1.0f);
    sc.incl_deg    = state.inclination;

    const float PRE_SCALE_PX = 167.0f;
    sc.r_pe_px = isEsc
                 ? (sc.rPe_m / (3.0f * sc.bodyR_m)) * PRE_SCALE_PX
                 : (float)ORB_MAX_R;

    float bodyCapPx = (sc.bodyR_m / sc.rPe_m) * sc.r_pe_px;
    float body_px_f, atmo_px_f;
    if (isEsc) {
        body_px_f = fmaxf(4.0f, bodyCapPx);
        atmo_px_f = (sc.atmoH_m > 0.0f)
                    ? fmaxf(body_px_f + 4.0f,
                            body_px_f * (sc.bodyR_m + sc.atmoH_m) / sc.bodyR_m)
                    : 0.0f;
    } else {
        float bodyCapPxEll = (sc.bodyR_m / sc.rPe_m) * (float)ORB_MAX_R;
        body_px_f = fmaxf(4.0f,
                          fminf((sc.bodyR_m / sc.sma_m) * (float)ORB_MAX_R, bodyCapPxEll));
        atmo_px_f = (sc.atmoH_m > 0.0f)
                    ? fminf((sc.bodyR_m + sc.atmoH_m) / sc.sma_m * (float)ORB_MAX_R,
                            bodyCapPxEll * (sc.bodyR_m + sc.atmoH_m) / sc.bodyR_m)
                    : 0.0f;
    }
    sc.body_px = (uint16_t)body_px_f;
    sc.atmo_px = (uint16_t)atmo_px_f;

    if (isEsc) {
        sc.focusX = ORB_PCX;
        sc.focusY = ORB_CY;
    } else {
        float f_px = (float)ORB_MAX_R * ecc;
        sc.focusX = ORB_PCX + (int16_t)(f_px * cosf(sc.argOfPe_rad));
        sc.focusY = ORB_CY  - (int16_t)(f_px * sinf(sc.argOfPe_rad));
    }

    // Altitude at current true anomaly — for useSoiView gate.
    float nu_rad = state.trueAnomaly * (PI / 180.0f);
    while (nu_rad >  PI) nu_rad -= 2.0f * PI;
    while (nu_rad < -PI) nu_rad += 2.0f * PI;
    float altSL_m = -1.0f;
    if (isEsc) {
        float asymAng = acosf(-1.0f / ecc);
        if (fabsf(nu_rad) < asymAng - 0.05f) {
            float l_m = sc.rPe_m * (ecc + 1.0f);
            altSL_m   = l_m / (1.0f + ecc * cosf(nu_rad)) - sc.bodyR_m;
        }
    } else {
        float l_m = sc.sma_m * (1.0f - ecc*ecc);
        altSL_m   = l_m / (1.0f + ecc * cosf(nu_rad)) - sc.bodyR_m;
    }

    sc.useSoiView = isEsc && sc.soi_m > 0.0f
                          && altSL_m >= 0.0f && altSL_m > 2.0f * sc.bodyR_m;

    // Orbit colour
    bool peInAtmo = !isEsc && ((sc.atmoH_m > 0.0f && PeA_m < sc.atmoH_m) || PeA_m < 0.0f);
    bool apInAtmo = !isEsc && ((sc.atmoH_m > 0.0f && ApA_m < sc.atmoH_m) || ApA_m < 0.0f);
    if      (isEsc)                sc.orbitCol = TFT_ORANGE;
    else if (peInAtmo || apInAtmo) sc.orbitCol = TFT_YELLOW;
    else                           sc.orbitCol = TFT_DARK_GREEN;

    // INCL geometry
    sc.bodyCY = (int16_t)constrain((int)sc.focusY,
                                   (int)(INCL_CLIP_Y0 + INC_L + 2),
                                   (int)(INCL_CLIP_Y1 - INC_L - 2));
    float i_rad = sc.incl_deg * (PI / 180.0f);
    sc.dx = INC_L * cosf(i_rad);
    sc.dy = INC_L * sinf(i_rad);
}

// Build the quantized signature for change-detection.
static OrbSceneSig _orbSceneSig(const OrbScene &sc) {
    OrbSceneSig sig;
    sig.bodyIdx    = sc.bodyIdx;
    sig.isEscape   = sc.isEscape;
    sig.useSoiView = sc.useSoiView;
    sig.orbitCol   = sc.orbitCol;

    // argOfPe: wrap to [0, 360) then quantize
    float argDeg = state.argOfPe;
    while (argDeg <   0.0f) argDeg += 360.0f;
    while (argDeg >= 360.0f) argDeg -= 360.0f;
    sig.argPe_q = (int16_t)(argDeg / SIG_ARGPE_DEG);

    sig.ecc_q  = (int16_t)(sc.ecc      / SIG_ECC);
    sig.incl_q = (int16_t)(sc.incl_deg / SIG_INCL_DEG);

    // Apsides as fraction of SMA — handles huge SOI range (Moho → Jool) uniformly.
    // Quantize the fraction, not the absolute distance.
    if (sc.sma_m > 1.0f) {
        sig.rPe_q = (int16_t)((sc.rPe_m / sc.sma_m) / SIG_APS_FRAC);
        if (sc.isEscape) {
            sig.rAp_q = -1;
        } else {
            float rAp_m = 2.0f * sc.sma_m - sc.rPe_m;
            sig.rAp_q = (int16_t)((rAp_m / sc.sma_m) / SIG_APS_FRAC);
        }
    } else {
        sig.rPe_q = 0; sig.rAp_q = 0;
    }
    return sig;
}

// ── PLAN panel draw — ellipse or hyperbola, focus-centred ─────────────────────────────
static void _orbDrawHyperbola(RA8875 &tft,
                               int16_t focusX, int16_t focusY,
                               float ecc, float argOfPe_rad,
                               float r_pe_px, uint16_t col) {
    float l       = r_pe_px * (ecc + 1.0f);
    float asymAng = acosf(-1.0f / ecc);
    float cosA    = cosf(argOfPe_rad), sinA = sinf(argOfPe_rad);

    // Sweep outward until arc endpoints exit panel bounds.
    float thetaMax = asymAng - 0.01f;
    for (uint16_t i = 1; i < 10000; i++) {
        float t = (float)i / 10000.0f * asymAng;
        float r = l / (1.0f + ecc * cosf(t));
        if (r <= 0.0f) break;
        bool hit = false;
        for (int8_t sign = -1; sign <= 1; sign += 2) {
            float phi = sign * t + argOfPe_rad;
            float sx  = focusX + r * cosf(phi);
            float sy  = focusY - r * sinf(phi);
            if (sx < PLAN_X0 || sx > PLAN_X1 || sy < PLAN_CLIP_Y0 || sy > PLAN_CLIP_Y1)
                { hit = true; break; }
        }
        if (hit) { thetaMax = (float)(i - 1) / 10000.0f * asymAng; break; }
    }
    if (thetaMax < 0.05f) thetaMax = 0.05f;

    const uint8_t STEPS = 120;
    float prevSX = 0, prevSY = 0;
    bool started = false;
    for (uint8_t i = 0; i <= STEPS; i++) {
        float theta = -thetaMax + (float)i / STEPS * 2.0f * thetaMax;
        float r = l / (1.0f + ecc * cosf(theta));
        if (r <= 0.0f || r > 500.0f) { started = false; continue; }
        float px = r * cosf(theta);
        float py = r * sinf(theta);
        float rx = px * cosA - py * sinA;
        float ry = px * sinA + py * cosA;
        float sx = focusX + rx;
        float sy = focusY - ry;
        if (started)
            tft.drawLine((int16_t)prevSX, (int16_t)prevSY,
                         (int16_t)sx,     (int16_t)sy, col);
        prevSX = sx; prevSY = sy;
        started = true;
    }

    // Asymptotes clipped to panel bounds
    for (uint8_t side = 0; side < 2; side++) {
        float aAng = (side == 0) ? asymAng : -asymAng;
        float dx0 = cosf(aAng);
        float dy0 = sinf(aAng);
        float dx  = dx0 * cosA - dy0 * sinA;
        float dys = -(dx0 * sinA + dy0 * cosA);
        float t_max = 300.0f;
        if (fabsf(dx)  > 1e-4f) t_max = fminf(t_max, dx  > 0 ? (PLAN_X1-focusX)/dx  : (PLAN_X0-focusX)/dx);
        if (fabsf(dys) > 1e-4f) t_max = fminf(t_max, dys > 0 ? (PLAN_Y1-focusY)/dys : (PLAN_Y0-focusY)/dys);
        t_max = fmaxf(0.0f, t_max);
        for (uint8_t seg = 0; seg < 6; seg++) {
            float t0 = (float)seg / 6.0f * t_max;
            float t1 = fminf((float)(seg + 0.4f) / 6.0f * t_max, t_max);
            tft.drawLine(
                (int16_t)(focusX + t0*dx), (int16_t)(focusY + t0*dys),
                (int16_t)(focusX + t1*dx), (int16_t)(focusY + t1*dys),
                TFT_DARK_GREY);
        }
    }
}

// Draw ellipse at ORB_PCX, ORB_CY, rotated by argOfPe.
// 120 steps = ~8 px chords at 150 px radius. Visually smooth without being
// wasteful. 90 steps showed visible waviness on the curvy side; 180 was
// smooth but added ~15 ms per repaint.
static void _orbDrawEllipse(RA8875 &tft, float ecc, float argOfPe_rad, uint16_t col) {
    float a_px = (float)ORB_MAX_R;
    float b_px = a_px * sqrtf(fmaxf(0.0f, 1.0f - ecc*ecc));
    float cosA = cosf(argOfPe_rad), sinA = sinf(argOfPe_rad);
    const uint16_t STEPS = 120;
    float prevSX = 0, prevSY = 0;
    bool started = false;
    for (uint16_t i = 0; i <= STEPS; i++) {
        float t  = (float)i / STEPS * 2.0f * PI;
        float px = a_px * cosf(t);
        float py = b_px * sinf(t);
        float rx = px * cosA - py * sinA;
        float ry = px * sinA + py * cosA;
        float sx = ORB_PCX + rx;
        float sy = ORB_CY  - ry;
        if (started)
            tft.drawLine((int16_t)prevSX, (int16_t)prevSY,
                         (int16_t)sx,     (int16_t)sy, col);
        prevSX = sx; prevSY = sy;
        started = true;
    }
}

// Draw SOI view — body at panel centre, SOI ring dashed, orbit arc, Pe marker.
// No vessel dot here — Layer 3 handles that uniformly.
static void _orbDrawSOIView(RA8875 &tft, const OrbScene &sc) {
    const float SOI_PX = 155.0f;
    const int16_t FX = ORB_PCX, FY = ORB_CY;

    float scale   = SOI_PX / sc.soi_m;
    float body_px = sc.bodyR_m * scale;
    float atmo_px = (sc.atmoH_m > 0.0f)
                    ? fmaxf(body_px + 2.0f, (sc.bodyR_m + sc.atmoH_m) * scale)
                    : 0.0f;
    float r_pe_px = sc.rPe_m * scale;

    // SOI boundary — dashed circle
    for (uint16_t deg = 0; deg < 360; deg += 8)
        tft.drawLine(
            FX + (int16_t)(SOI_PX * cosf(deg * PI/180.0f)),
            FY - (int16_t)(SOI_PX * sinf(deg * PI/180.0f)),
            FX + (int16_t)(SOI_PX * cosf((deg+4) * PI/180.0f)),
            FY - (int16_t)(SOI_PX * sinf((deg+4) * PI/180.0f)),
            TFT_GREY);

    // Atmosphere disc
    if (atmo_px > body_px && sc.atmoCol != 0)
        tft.fillCircle(FX, FY, (uint16_t)atmo_px, sc.atmoCol);

    // Orbit arc at SOI scale
    _orbDrawHyperbola(tft, FX, FY, sc.ecc, sc.argOfPe_rad, r_pe_px, TFT_ORANGE);

    // Body disc
    tft.fillCircle(FX, FY, (uint16_t)fmaxf(3.0f, body_px), sc.bodyCol);

    // Pe marker at SOI-scaled position
    float cosA = cosf(sc.argOfPe_rad), sinA = sinf(sc.argOfPe_rad);
    int16_t pe_x = FX + (int16_t)(r_pe_px * cosA);
    int16_t pe_y = FY - (int16_t)(r_pe_px * sinA);
    if (pe_x >= PLAN_X0 && pe_x <= PLAN_X1 && pe_y >= PLAN_CLIP_Y0 && pe_y <= PLAN_CLIP_Y1) {
        tft.fillCircle(pe_x, pe_y, 2, TFT_MAGENTA);
        int16_t lx = (int16_t)constrain((int)(pe_x + 10.0f * cosA),
                                        (int)PLAN_X0, (int)(PLAN_X1 - 8));
        int16_t ly = (int16_t)constrain((int)(pe_y - 10.0f * sinA),
                                        (int)PLAN_CLIP_Y0, (int)(PLAN_CLIP_Y1 - 4));
        tft.setFont(&Roboto_Black_12);
        tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        tft.setCursor(lx - 8, ly - 6); tft.print("Pe");
    }
}

// Draw Pe / Ap / AN / DN markers on the planar orbit.
// No cached pixel positions stored — scene repaint always covers the full panel.
// Marker DOT bounds use PLAN_Y0/Y1 (panel interior, 92..400). Marker LABEL
// bounds use PLAN_CLIP_Y0/Y1 (slightly inset, 94..398) so label text doesn't
// print too close to panel edges. Dots can be at the very top of the panel
// (e.g. Pe at argOfPe=90° lives at y=83 which is above PLAN_CLIP_Y0=94 but
// within the panel drawing area because PLAN extends up into the header strip
// y=62..92 for graphic content — only chrome text is excluded there).
static void _orbDrawMarkers(RA8875 &tft, const OrbScene &sc) {
    const int8_t  DOT_R   = 3;
    // Dot visibility bounds — loose (panel interior plus header strip for top).
    const int16_t DOT_Y_MIN = HDR_Y0;       // 62 — dots can be at very top
    const int16_t DOT_Y_MAX = PLAN_Y1 - 2;  // 398
    // Label placement bounds — tighter inset.
    const int16_t LX_MIN = PLAN_X0,  LX_MAX = PLAN_X1 - 8;
    const int16_t LY_MIN = PLAN_CLIP_Y0,  LY_MAX = PLAN_CLIP_Y1 - 4;
    const float   OFFSET = 14.0f;

    float cosA = cosf(sc.argOfPe_rad), sinA = sinf(sc.argOfPe_rad);

    // ── AN / DN — only when inclined and not escape ──────────────────────────────────
    if (!sc.isEscape && sc.incl_deg > 0.5f) {
        float f_px_eq = (float)ORB_MAX_R * sc.ecc;
        int16_t fX_eq = ORB_PCX + (int16_t)(f_px_eq * cosA);
        int16_t fY_eq = ORB_CY  - (int16_t)(f_px_eq * sinA);
        float l = (float)ORB_MAX_R * (1.0f - sc.ecc*sc.ecc);

        int16_t pe_sx = ORB_PCX + (int16_t)((float)ORB_MAX_R * cosA);
        int16_t pe_sy = ORB_CY  - (int16_t)((float)ORB_MAX_R * sinA);
        int16_t ap_sx = ORB_PCX - (int16_t)((float)ORB_MAX_R * cosA);
        int16_t ap_sy = ORB_CY  + (int16_t)((float)ORB_MAX_R * sinA);

        auto nearMarker = [](int16_t ax, int16_t ay, int16_t bx, int16_t by) -> bool {
            int dx = ax-bx, dy = ay-by;
            return (dx*dx + dy*dy) < 144;
        };

        // AN on east side of focus along equatorial line
        float r_an = l / (1.0f + sc.ecc * cosf(sc.argOfPe_rad));
        int16_t an_x = fX_eq + (int16_t)(r_an);
        int16_t an_y = fY_eq;
        if (an_x >= PLAN_X0 && an_x <= PLAN_X1 && an_y >= DOT_Y_MIN && an_y <= DOT_Y_MAX) {
            tft.fillCircle(an_x, an_y, DOT_R, TFT_CYAN);
            if (!nearMarker(an_x, an_y, pe_sx, pe_sy) &&
                !nearMarker(an_x, an_y, ap_sx, ap_sy)) {
                int16_t lx = (int16_t)constrain(an_x + 6,  (int)LX_MIN, (int)LX_MAX);
                int16_t ly = (int16_t)constrain(an_y - 14, (int)LY_MIN, (int)LY_MAX);
                tft.setFont(&Roboto_Black_12);
                tft.setTextColor(TFT_CYAN, TFT_BLACK);
                tft.setCursor(lx, ly); tft.print("AN");
            }
        }

        // DN on west side of focus
        float denom_dn = 1.0f - sc.ecc * cosf(sc.argOfPe_rad);
        if (denom_dn > 0.01f) {
            float r_dn = l / denom_dn;
            int16_t dn_x = fX_eq - (int16_t)(r_dn);
            int16_t dn_y = fY_eq;
            if (dn_x >= PLAN_X0 && dn_x <= PLAN_X1 && dn_y >= DOT_Y_MIN && dn_y <= DOT_Y_MAX) {
                tft.fillCircle(dn_x, dn_y, DOT_R, TFT_YELLOW);
                if (!nearMarker(dn_x, dn_y, pe_sx, pe_sy) &&
                    !nearMarker(dn_x, dn_y, ap_sx, ap_sy)) {
                    int16_t lx = (int16_t)constrain(dn_x - 22, (int)LX_MIN, (int)LX_MAX);
                    int16_t ly = (int16_t)constrain(dn_y - 14, (int)LY_MIN, (int)LY_MAX);
                    tft.setFont(&Roboto_Black_12);
                    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                    tft.setCursor(lx, ly); tft.print("DN");
                }
            }
        }
    }

    // ── Pe ────────────────────────────────────────────────────────────────────────────
    float pe_r   = sc.isEscape ? sc.r_pe_px : (float)ORB_MAX_R;
    int16_t pe_x = ORB_PCX + (int16_t)(pe_r * cosA);
    int16_t pe_y = ORB_CY  - (int16_t)(pe_r * sinA);

    // Hide Pe only when it's inside the physical body (impact trajectory).
    // Compare in METRES, not pixels — the pixel form confused body and atmo
    // at certain eccentricities because body_px includes a scale term that
    // grows when rPe is near bodyR, making rPe_px ≤ body_px even for Pe
    // above the surface but inside the atmosphere. Using rPe_m > bodyR_m is
    // physically rigorous: Pe is visible iff the periapsis is above the
    // ground. (Atmosphere is just visual context — Pe inside atmo is still
    // a valid orbit.)
    bool  showPe = sc.isEscape || (sc.rPe_m > sc.bodyR_m);

    if (showPe && pe_x >= PLAN_X0 && pe_x <= PLAN_X1 &&
                  pe_y >= DOT_Y_MIN && pe_y <= DOT_Y_MAX) {
        tft.fillCircle(pe_x, pe_y, DOT_R, TFT_MAGENTA);
        int16_t lx = (int16_t)constrain((int)(pe_x + OFFSET * cosA),
                                        (int)LX_MIN, (int)LX_MAX);
        int16_t ly = (int16_t)constrain((int)(pe_y - OFFSET * sinA),
                                        (int)LY_MIN, (int)LY_MAX);
        tft.setFont(&Roboto_Black_12);
        tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        tft.setCursor(lx - 8, ly - 6); tft.print("Pe");
    }

    // ── Ap ────────────────────────────────────────────────────────────────────────────
    if (!sc.isEscape) {
        int16_t ap_x = ORB_PCX - (int16_t)((float)ORB_MAX_R * cosA);
        int16_t ap_y = ORB_CY  + (int16_t)((float)ORB_MAX_R * sinA);
        if (ap_x >= PLAN_X0 && ap_x <= PLAN_X1 &&
            ap_y >= DOT_Y_MIN && ap_y <= DOT_Y_MAX) {
            tft.fillCircle(ap_x, ap_y, DOT_R, TFT_BLUE);
            int16_t lx = (int16_t)constrain((int)(ap_x - OFFSET * cosA),
                                            (int)LX_MIN, (int)LX_MAX);
            int16_t ly = (int16_t)constrain((int)(ap_y + OFFSET * sinA),
                                            (int)LY_MIN, (int)LY_MAX);
            tft.setFont(&Roboto_Black_12);
            tft.setTextColor(TFT_BLUE, TFT_BLACK);
            tft.setCursor(lx - 8, ly - 6); tft.print("Ap");
        }
    }
}

// Restore just the 4 marker DOTS (no labels) at their current positions.
// Called after the arc-patch in the vessel-update path. When the vessel passes
// through a marker dot, the patch paints orbit-colour through the dot; this
// helper re-paints the dot on top. Labels are NOT touched because the patch
// can't reach them (labels are 14 px offset from dots, patch extends ~7 px
// from old dot) and we want to avoid the black-background-rectangle of text
// drawing stomping orbit-line pixels.
static void _orbDrawMarkerDotsOnly(RA8875 &tft, const OrbScene &sc) {
    const int8_t DOT_R = 3;
    // Match the loose bounds used by _orbDrawMarkers for the dots — the tight
    // PLAN_CLIP_Y0 was for labels, not dots. Using it here suppressed Pe/Ap
    // dots when argOfPe ≈ 90° put them near the panel top edge.
    const int16_t DOT_Y_MIN = HDR_Y0;
    const int16_t DOT_Y_MAX = PLAN_Y1 - 2;
    float cosA = cosf(sc.argOfPe_rad), sinA = sinf(sc.argOfPe_rad);

    // AN / DN dots (when inclined, not escape)
    if (!sc.isEscape && sc.incl_deg > 0.5f) {
        float f_px_eq = (float)ORB_MAX_R * sc.ecc;
        int16_t fX_eq = ORB_PCX + (int16_t)(f_px_eq * cosA);
        int16_t fY_eq = ORB_CY  - (int16_t)(f_px_eq * sinA);
        float l = (float)ORB_MAX_R * (1.0f - sc.ecc*sc.ecc);

        float r_an = l / (1.0f + sc.ecc * cosf(sc.argOfPe_rad));
        int16_t an_x = fX_eq + (int16_t)(r_an);
        int16_t an_y = fY_eq;
        if (an_x >= PLAN_X0 && an_x <= PLAN_X1 &&
            an_y >= DOT_Y_MIN && an_y <= DOT_Y_MAX)
            tft.fillCircle(an_x, an_y, DOT_R, TFT_CYAN);

        float denom_dn = 1.0f - sc.ecc * cosf(sc.argOfPe_rad);
        if (denom_dn > 0.01f) {
            float r_dn = l / denom_dn;
            int16_t dn_x = fX_eq - (int16_t)(r_dn);
            int16_t dn_y = fY_eq;
            if (dn_x >= PLAN_X0 && dn_x <= PLAN_X1 &&
                dn_y >= DOT_Y_MIN && dn_y <= DOT_Y_MAX)
                tft.fillCircle(dn_x, dn_y, DOT_R, TFT_YELLOW);
        }
    }

    // Pe dot. Same visibility rule as _orbDrawMarkers — show iff Pe is above
    // the body surface (rPe_m > bodyR_m) or trajectory is escape.
    float pe_r   = sc.isEscape ? sc.r_pe_px : (float)ORB_MAX_R;
    int16_t pe_x = ORB_PCX + (int16_t)(pe_r * cosA);
    int16_t pe_y = ORB_CY  - (int16_t)(pe_r * sinA);
    bool  showPe = sc.isEscape || (sc.rPe_m > sc.bodyR_m);
    if (showPe && pe_x >= PLAN_X0 && pe_x <= PLAN_X1 &&
                  pe_y >= DOT_Y_MIN && pe_y <= DOT_Y_MAX)
        tft.fillCircle(pe_x, pe_y, DOT_R, TFT_MAGENTA);

    // Ap dot
    if (!sc.isEscape) {
        int16_t ap_x = ORB_PCX - (int16_t)((float)ORB_MAX_R * cosA);
        int16_t ap_y = ORB_CY  + (int16_t)((float)ORB_MAX_R * sinA);
        if (ap_x >= PLAN_X0 && ap_x <= PLAN_X1 &&
            ap_y >= DOT_Y_MIN && ap_y <= DOT_Y_MAX)
            tft.fillCircle(ap_x, ap_y, DOT_R, TFT_BLUE);
    }
}

// ── INCL panel draw — tilted orbit line, body, Pe/Ap markers ─────────────────────────
// No vessel dot here — Layer 3 handles that uniformly.
static void _orbDrawIncl(RA8875 &tft, const OrbScene &sc) {
    int16_t n_x = (int16_t)(INC_CX + sc.dx), n_y = (int16_t)(sc.bodyCY - sc.dy);
    int16_t s_x = (int16_t)(INC_CX - sc.dx), s_y = (int16_t)(sc.bodyCY + sc.dy);

    // Dashed equatorial line
    for (int16_t x = INCL_X0; x < INCL_X1; x += 10)
        tft.drawLine(x, sc.bodyCY, min((int16_t)(x + 6), INCL_X1), sc.bodyCY, TFT_GREY);

    // Orbit line (2px thick)
    tft.drawLine(s_x, s_y,     n_x, n_y,     sc.orbitCol);
    tft.drawLine(s_x, s_y + 1, n_x, n_y + 1, sc.orbitCol);

    // Body disc on top of orbit line
    tft.fillCircle(INC_CX, sc.bodyCY, INC_BR, sc.bodyCol);

    // N / S labels
    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(INC_CX - 4, sc.bodyCY - INC_BR - 14); tft.print("N");
    tft.setCursor(INC_CX - 4, sc.bodyCY + INC_BR + 2);  tft.print("S");

    // AN label on WEST side of body disc (only when inclined)
    if (sc.incl_deg > 0.5f) {
        tft.setFont(&Roboto_Black_12);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(INC_CX - INC_BR - 18, sc.bodyCY - 6); tft.print("AN");
    }

    // Note: numeric inclination value was here; moved to the readout strip
    // (top row, "Inc: NN.N°" at INC_RDY0) so the inclination value updates
    // at the text cadence rather than requiring a scene repaint.

    // Pe on orbit line — suppressed if projects within body disc
    float argPe_rad = sc.argOfPe_rad;
    float t_pe = sinf(argPe_rad);
    int16_t pe_x = (int16_t)(INC_CX + t_pe * sc.dx);
    int16_t pe_y = (int16_t)(sc.bodyCY - t_pe * sc.dy);
    float pe_dist2 = (t_pe * sc.dx) * (t_pe * sc.dx) + (t_pe * sc.dy) * (t_pe * sc.dy);
    if (pe_dist2 > (float)((INC_BR+4)*(INC_BR+4)) &&
        pe_x >= INCL_X0 && pe_x <= INCL_X1 && pe_y >= INCL_CLIP_Y0 && pe_y <= INCL_CLIP_Y1) {
        tft.fillCircle(pe_x, pe_y, 3, TFT_MAGENTA);
        tft.setFont(&Roboto_Black_12);
        tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        int16_t lx = (pe_x >= INC_CX)
            ? (int16_t)constrain(pe_x + 6,  INCL_X0+2, INCL_X1-22)
            : (int16_t)constrain(pe_x - 22, INCL_X0+2, INCL_X1-22);
        tft.setCursor(lx, (int16_t)constrain(pe_y - 14, INCL_CLIP_Y0+2, INCL_CLIP_Y1-14));
        tft.print("Pe");
    }

    // Ap on orbit line
    if (!sc.isEscape) {
        float t_ap = -t_pe;
        int16_t ap_x = (int16_t)(INC_CX + t_ap * sc.dx);
        int16_t ap_y = (int16_t)(sc.bodyCY - t_ap * sc.dy);
        float ap_dist2 = (t_ap * sc.dx) * (t_ap * sc.dx) + (t_ap * sc.dy) * (t_ap * sc.dy);
        if (ap_dist2 > (float)((INC_BR+4)*(INC_BR+4)) &&
            ap_x >= INCL_X0 && ap_x <= INCL_X1 && ap_y >= INCL_CLIP_Y0 && ap_y <= INCL_CLIP_Y1) {
            tft.fillCircle(ap_x, ap_y, 3, TFT_BLUE);
            tft.setFont(&Roboto_Black_12);
            tft.setTextColor(TFT_BLUE, TFT_BLACK);
            int16_t lx = (ap_x >= INC_CX)
                ? (int16_t)constrain(ap_x + 6,  INCL_X0+2, INCL_X1-22)
                : (int16_t)constrain(ap_x - 22, INCL_X0+2, INCL_X1-22);
            tft.setCursor(lx, (int16_t)constrain(ap_y - 14, INCL_CLIP_Y0+2, INCL_CLIP_Y1-14));
            tft.print("Ap");
        }
    }
}

// ── Layer 3 — Vessel dot with arc-patch erase (D1) ────────────────────────────────────
// Computes vessel pixel position on the PLAN panel. Returns false if vessel is
// hidden (inside body, past asymptote, or off panel).
static bool _orbVesselPxPlan(const OrbScene &sc, float nu_rad,
                              int16_t &out_x, int16_t &out_y) {
    float r_px;
    if (sc.isEscape) {
        float asymAng = acosf(-1.0f / sc.ecc);
        if (fabsf(nu_rad) >= asymAng - 0.05f) return false;
        float l = sc.r_pe_px * (sc.ecc + 1.0f);
        r_px = l / (1.0f + sc.ecc * cosf(nu_rad));
    } else {
        float a = (float)ORB_MAX_R;
        r_px = a * (1.0f - sc.ecc*sc.ecc) / (1.0f + sc.ecc * cosf(nu_rad));
    }
    if (r_px <= (float)sc.body_px + 1.0f) return false;

    float phi = nu_rad + sc.argOfPe_rad;
    int16_t sx = sc.focusX + (int16_t)(r_px * cosf(phi));
    int16_t sy = sc.focusY - (int16_t)(r_px * sinf(phi));
    if (sx < PLAN_X0 || sx > PLAN_X1 || sy < PLAN_CLIP_Y0 || sy > PLAN_CLIP_Y1) return false;

    out_x = sx; out_y = sy;
    return true;
}

// Patch the orbit line locally around true anomaly nu_rad.
// Used after erasing the old vessel dot to restore the orbit line underneath.
//
// Critical: the patch MUST retrace the same pixel path as _orbDrawEllipse
// so no orbit-colour stragglers are left behind. `_orbDrawEllipse` uses the
// parametric form `a*cos(t), b*sin(t)` (t = eccentric anomaly) with 120 fixed
// steps around the full 2π range; each drawLine produces a specific pixel
// set that differs slightly from any other parameterization's trace of the
// same geometric ellipse.
//
// So: for the ellipse case, we convert nu → E (eccentric anomaly), snap to
// the nearest ellipse step boundaries, and re-issue drawLines between those
// exact boundary points. This is pixel-identical to the original ellipse
// trace.
//
// For hyperbolic/SOI cases, there's no such periodic step structure — the
// hyperbola/SOI drawings are small and any mismatch is tolerable. Fall back
// to the focal-polar form for those.
static void _orbPatchArcPlan(RA8875 &tft, const OrbScene &sc, float nu_rad) {
    // ── Ellipse case: retrace ellipse's exact parametric vertices ────────────────────
    if (!sc.useSoiView && !sc.isEscape) {
        const uint16_t STEPS = 120;  // MUST match _orbDrawEllipse
        float a_px = (float)ORB_MAX_R;
        float e = sc.ecc;
        float b_px = a_px * sqrtf(fmaxf(0.0f, 1.0f - e*e));
        float cosA = cosf(sc.argOfPe_rad), sinA = sinf(sc.argOfPe_rad);

        // Convert true anomaly nu → eccentric anomaly E via the standard
        // Keplerian relation: tan(E/2) = sqrt((1-e)/(1+e)) * tan(nu/2).
        // The atan-based inversion gives the correct branch as long as nu is
        // already normalized to (-π, π] — which oldNu is, because atan2f
        // returns values in that range and subtracting argOfPe (in similar
        // range) gives a value within (-2π, 2π). For safety we normalize first.
        float nu_norm = nu_rad;
        while (nu_norm >  PI) nu_norm -= 2.0f * PI;
        while (nu_norm < -PI) nu_norm += 2.0f * PI;

        float E_center;
        if (e < 1e-6f) {
            E_center = nu_norm;  // circle — E == nu
        } else {
            float half = sqrtf((1.0f - e) / (1.0f + e)) * tanf(nu_norm * 0.5f);
            E_center = 2.0f * atanf(half);
            // Within nu_norm ∈ (-π, π], E_center from this formula is in the
            // same branch. No correction needed — both quantities wrap at ±π
            // together.
        }

        // Snap to the ellipse's step grid. The ellipse is drawn at:
        //   t_i = i * 2π / STEPS, i = 0..STEPS
        // Pick indices covering E_center ± 3 steps (6 segments of ~5-8 px each
        // at r=150, total ~30-48 px — well over the 10 px erase diameter plus
        // margin for integer-rounding jitter on the oldNu estimate).
        float step_rad = 2.0f * PI / STEPS;
        // Normalize E_center to [0, 2π)
        float E_norm = E_center;
        while (E_norm < 0.0f) E_norm += 2.0f * PI;
        while (E_norm >= 2.0f * PI) E_norm -= 2.0f * PI;
        int16_t center_idx = (int16_t)(E_norm / step_rad + 0.5f);
        const int16_t HALF_SEGS = 3;  // 6 segments total ≈ 30-48 px patch

        float prev_sx = 0, prev_sy = 0;
        bool  started = false;
        for (int16_t i = center_idx - HALF_SEGS; i <= center_idx + HALF_SEGS; i++) {
            // Wrap i to [0, STEPS] range. Using modulo to keep the trace valid
            // even if the vessel is near nu=π (i.e., near the Pe/Ap line at
            // the wrap boundary).
            int16_t idx = i;
            while (idx < 0) idx += STEPS;
            while (idx > (int16_t)STEPS) idx -= STEPS;
            float t = (float)idx * step_rad;

            // Exact same parametric form as _orbDrawEllipse
            float px = a_px * cosf(t);
            float py = b_px * sinf(t);
            float rx = px * cosA - py * sinA;
            float ry = px * sinA + py * cosA;
            float sx = (float)ORB_PCX + rx;
            float sy = (float)ORB_CY  - ry;

            if (started)
                tft.drawLine((int16_t)prev_sx, (int16_t)prev_sy,
                             (int16_t)sx,     (int16_t)sy,
                             sc.orbitCol);
            prev_sx = sx; prev_sy = sy;
            started = true;
        }
        return;
    }

    // ── Hyperbola / SOI-view case: focal-polar form, adaptive DELTA ──────────────────
    float scale_m_to_px = 0.0f;
    int16_t fx, fy;
    if (sc.useSoiView) {
        const float SOI_PX = 155.0f;
        scale_m_to_px = SOI_PX / sc.soi_m;
        fx = ORB_PCX; fy = ORB_CY;
    } else {
        fx = sc.focusX; fy = sc.focusY;
    }

    // Compute local r_px at nu_rad to choose DELTA adaptively.
    float r_px_local;
    if (sc.useSoiView) {
        float l_m = sc.rPe_m * (sc.ecc + 1.0f);
        float denom = 1.0f + sc.ecc * cosf(nu_rad);
        float r_m  = (fabsf(denom) > 0.01f) ? l_m / denom : sc.rPe_m;
        r_px_local = r_m * scale_m_to_px;
    } else {  // isEscape
        float l_px  = sc.r_pe_px * (sc.ecc + 1.0f);
        float denom = 1.0f + sc.ecc * cosf(nu_rad);
        r_px_local = (fabsf(denom) > 0.01f) ? l_px / denom : sc.r_pe_px;
    }
    if (r_px_local < 10.0f) r_px_local = 10.0f;

    const float TARGET_HALF_ARC_PX = 10.0f;
    float DELTA = TARGET_HALF_ARC_PX / r_px_local;
    if (DELTA < 0.05f) DELTA = 0.05f;
    if (DELTA > 0.30f) DELTA = 0.30f;

    auto pointAt = [&](float t, float &out_sx, float &out_sy) -> bool {
        float r_px;
        if (sc.useSoiView) {
            float asymAng = acosf(-1.0f / sc.ecc);
            if (fabsf(t) >= asymAng - 0.05f) return false;
            float l_m = sc.rPe_m * (sc.ecc + 1.0f);
            float r_m = l_m / (1.0f + sc.ecc * cosf(t));
            if (r_m <= 0.0f) return false;
            r_px = r_m * scale_m_to_px;
        } else {  // isEscape
            float asymAng = acosf(-1.0f / sc.ecc);
            if (fabsf(t) >= asymAng - 0.05f) return false;
            float l_px = sc.r_pe_px * (sc.ecc + 1.0f);
            r_px = l_px / (1.0f + sc.ecc * cosf(t));
            if (r_px <= 0.0f) return false;
        }
        float phi = t + sc.argOfPe_rad;
        out_sx = fx + r_px * cosf(phi);
        out_sy = fy - r_px * sinf(phi);
        return true;
    };

    const uint8_t SEGS = 3;
    float prev_sx = 0, prev_sy = 0;
    bool  started = false;
    for (uint8_t i = 0; i <= SEGS; i++) {
        float t = nu_rad - DELTA + (float)i * (2.0f * DELTA / SEGS);
        float sx, sy;
        if (!pointAt(t, sx, sy)) { started = false; continue; }
        if (started)
            tft.drawLine((int16_t)prev_sx, (int16_t)prev_sy,
                         (int16_t)sx,     (int16_t)sy,
                         sc.orbitCol);
        prev_sx = sx; prev_sy = sy;
        started = true;
    }
}

// ── Per-frame state ───────────────────────────────────────────────────────────────────
static PrintState _orbPS[7];  // readout strip PrintStates (slot 6 = Inc on INCL)

// Vessel dot cache (both panels).
static bool    _planDotValid = false;
static int16_t _planDotX     = 0;
static int16_t _planDotY     = 0;
static bool    _inclDotValid = false;
static int16_t _inclDotX     = 0;
static int16_t _inclDotY     = 0;

// Scene cache + signature for change detection.
static bool        _sceneValid = false;
static OrbScene    _lastScene;
static OrbSceneSig _lastSig;
static uint32_t    _lastSceneRepaint = 0;

// Pending scene change held off by the minimum-interval gate.
static bool        _pendingSig = false;

// Body name cache — tracks last-drawn bodyIdx so the name is only redrawn when
// the SOI actually changes. The name lives in the header strip (above the
// scene repaint region), so scene repaints don't disturb it.
static int16_t     _lastBodyIdxDrawn = -1;

// Readout string cache — tracks last-drawn value strings so printValue is only
// called when the text actually changes. printValue with PrintState is
// flicker-free only when the string is unchanged and the call is skipped; when
// called every frame on a changing string, the character-cell erase phase is
// visible.
static String      _lastReadout[7];
// Last-drawn T+Pe/T+Ap label: 0=Pe, 1=Ap, -1=never drawn.
static int8_t      _lastTLabel = -1;

// Diagnostics
static uint32_t    _vesselUpdateCount = 0;

// Draw (or refresh) the SOI body name in the header strip.
// Called from chromeScreen_ORB (initial draw) and from drawScreen_ORB when
// bodyIdx changes. Position: right-justified in a box over the INCL panel
// header area. Uses textRight which clears its own background — this gives
// a clean redraw when the body actually changes.
static void _orbDrawBodyName(RA8875 &tft, uint8_t bodyIdx) {
    textRight(tft, &Roboto_Black_20,
              362, ORB_TITLE_TOP + 5, 360, 24,
              String(ORB_BODIES[bodyIdx].name),
              TFT_LIGHT_GREY, TFT_BLACK);
    _lastBodyIdxDrawn = (int16_t)bodyIdx;
}

// ── Scene redraw — full panel clear + redraw from scratch ─────────────────────────────
static void _orbRepaintPlan(RA8875 &tft, const OrbScene &sc) {
    uint32_t t0 = micros();
    // Erase bound extends up into the header strip (y=62..92) so atmo/body
    // pixels that naturally draw above y=92 (e.g. argOfPe near 90°) get
    // cleared. No chrome text lives here anymore (ORBIT/INCL labels removed)
    // so we can wipe freely.
    tft.fillRect(PLAN_X0, PLAN_FILL_Y0,
                 (uint16_t)(PLAN_X1 - PLAN_X0),
                 (uint16_t)(PLAN_Y1 - PLAN_FILL_Y0), TFT_BLACK);
    uint32_t t1 = micros();

    if (sc.useSoiView) {
        _orbDrawSOIView(tft, sc);
    } else {
        // Draw order: atmo → body → orbit line → markers.
        // Orbit line drawn AFTER atmo and body so a sub-atmospheric Pe (impact
        // trajectory, orbit inside atmosphere) remains visible as a line
        // crossing through/over the atmosphere and body. The orbit line is
        // the key readout; it must be on top of the body graphics.
        if (sc.atmo_px > sc.body_px && sc.atmoCol != 0)
            tft.fillCircle(sc.focusX, sc.focusY, sc.atmo_px, sc.atmoCol);
        tft.fillCircle(sc.focusX, sc.focusY, sc.body_px, sc.bodyCol);
        if (sc.isEscape) {
            _orbDrawHyperbola(tft, sc.focusX, sc.focusY,
                              sc.ecc, sc.argOfPe_rad, sc.r_pe_px, sc.orbitCol);
        } else {
            _orbDrawEllipse(tft, sc.ecc, sc.argOfPe_rad, sc.orbitCol);
        }
        _orbDrawMarkers(tft, sc);
    }
    uint32_t t2 = micros();

    if (debugMode) {
        Serial.print(F("[ORB] PLAN repaint  fill="));
        Serial.print(t1 - t0);
        Serial.print(F("us  draw="));
        Serial.print(t2 - t1);
        Serial.print(F("us  total="));
        Serial.print(t2 - t0);
        Serial.print(F("us  soi="));
        Serial.print(sc.useSoiView ? 1 : 0);
        Serial.print(F("  esc="));
        Serial.println(sc.isEscape ? 1 : 0);
    }

    // Scene repaint erases the vessel dot along with everything else.
    _planDotValid = false;
}

static void _orbRepaintIncl(RA8875 &tft, const OrbScene &sc) {
    uint32_t t0 = micros();
    // INCL fillRect covers only the graphics area (y=92..374). The body name
    // in the header strip above is drawn in chrome and refreshed only when
    // bodyIdx changes. The INCL graphic never extends into the header strip
    // (bodyCY=233 ± INC_L=130 gives y=103..363), so no flicker.
    tft.fillRect(INCL_X0, INCL_FILL_Y0,
                 (uint16_t)(INCL_X1 - INCL_X0),
                 (uint16_t)(INCL_Y1 - INCL_FILL_Y0), TFT_BLACK);
    uint32_t t1 = micros();

    _orbDrawIncl(tft, sc);
    uint32_t t2 = micros();

    if (debugMode) {
        Serial.print(F("[ORB] INCL repaint  fill="));
        Serial.print(t1 - t0);
        Serial.print(F("us  draw="));
        Serial.print(t2 - t1);
        Serial.print(F("us  total="));
        Serial.println(t2 - t0);
    }

    _inclDotValid = false;
}

// ── Public entry points ───────────────────────────────────────────────────────────────
static void chromeScreen_ORB(RA8875 &tft) {
    // Invalidate all caches on screen entry — force scene repaint and dot redraw
    // on the first updateScreen call.
    _sceneValid        = false;
    _planDotValid      = false;
    _inclDotValid      = false;
    _lastSceneRepaint  = 0;
    _pendingSig        = false;
    _vesselUpdateCount = 0;
    _lastBodyIdxDrawn  = -1;
    _lastTLabel        = -1;
    for (uint8_t i = 0; i < 7; i++) { _orbPS[i] = PrintState{}; _lastReadout[i] = String("\x01"); }

    // Panel divider
    tft.drawLine(360, ORB_TITLE_TOP, 360, ORB_SCREEN_H, TFT_GREY);
    tft.drawLine(361, ORB_TITLE_TOP, 361, ORB_SCREEN_H, TFT_GREY);

    // Panel labels "ORBIT" and "INCL" are intentionally absent — the screen
    // title already says "ORBIT" and the left/right functional split is clear
    // from the readouts and graphics. Removing them also eliminates the body-
    // disc overlap problem at extreme argOfPe values.

    // Body name — right-justified over INCL panel, in the header strip.
    // Drawn once in chrome, refreshed only when bodyIdx changes. The body
    // graphic is at x=520..560 so this name at x=640..719 is never overlapped.
    _orbDrawBodyName(tft, _orbBodyIdx());

    // Readout strip labels. PLAN side is 3 rows; INCL side is 4 rows (Inc
    // moved here from the graphic). All use Roboto_Black_20 except where noted.
    // Alt.SL / PRD / Arg.Pe / T+Pe-T+Ap labels are white; Pe and Ap use their
    // dot colours (magenta / blue) as mnemonic colour coding; Inc is also white.
    tft.setFont(&Roboto_Black_20);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(6,   ORB_RDY1); tft.print("Alt.SL");
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.setCursor(6,   ORB_RDY2); tft.print("Pe");
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    tft.setCursor(6,   ORB_RDY3); tft.print("Ap");

    // INCL readout labels — Inc (top), PRD, Arg.Pe. T+Pe/T+Ap is dynamic.
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(INC_LABEL_X, INC_RDY0); tft.print("Inc:");
    tft.setCursor(INC_LABEL_X, INC_RDY1); tft.print("PRD");
    tft.setCursor(INC_LABEL_X, INC_RDY2); tft.print("Arg.Pe");
    // T+Pe / T+Ap label is drawn on demand from drawScreen_ORB via _lastTLabel cache
}

static void drawScreen_ORB(RA8875 &tft) {
    uint32_t frameStart = debugMode ? micros() : 0;

    // ── Compute current scene + signature ────────────────────────────────────────────
    OrbScene    sc;
    _orbComputeScene(sc);
    OrbSceneSig sig = _orbSceneSig(sc);

    uint32_t now = millis();

    // ── Scene-change gating ──────────────────────────────────────────────────────────
    // The repaint itself takes ~50-100 ms on this hardware, so a gate shorter
    // than that doesn't actually gate anything — by the time the next call to
    // drawScreen_ORB comes in, the interval has already elapsed.
    //
    // The interval of 500 ms is a floor on how often the scene can repaint:
    // rapid sustained changes (burn, maneuver) cap at 2 Hz, which matches the
    // "professional look" requirement.
    //
    // Critical subtlety: the post-interval check compares the current sig
    // against the LAST DRAWN sig, not against whatever sig triggered the
    // pending flag. This means transient flips that settle back during the
    // hold-off window do NOT force a repaint — the screen is already showing
    // the current state. Only signatures that still differ after the interval
    // trigger a repaint.
    bool sceneNeedsRepaint = false;
    if (!_sceneValid) {
        sceneNeedsRepaint = true;
    } else if (sig != _lastSig) {
        _pendingSig = true;
    } else {
        // Current sig matches what's drawn — clear the pending flag. A prior
        // transient change that triggered _pendingSig has since resolved.
        _pendingSig = false;
    }
    if (_pendingSig
        && (sig != _lastSig)                                // still stale
        && (now - _lastSceneRepaint >= SCENE_MIN_INTERVAL_MS)) {
        sceneNeedsRepaint = true;
    }

    // ── Layer 2: scene repaint ───────────────────────────────────────────────────────
    if (sceneNeedsRepaint) {
        // Split PLAN and INCL — both always repaint together for now since the
        // signature lumps both panels. Could be split later if profiling shows
        // INCL is cheap to repaint and benefits from more frequent updates.
        _orbRepaintPlan(tft, sc);
        _orbRepaintIncl(tft, sc);
        _lastScene        = sc;
        _lastSig          = sig;
        _sceneValid       = true;
        _lastSceneRepaint = now;
        _pendingSig       = false;
    }

    // Body name — drawn once in chrome, then refreshed only when the SOI body
    // changes. Lives in the protected header strip (y=62..92), so scene
    // repaints never disturb it.
    if ((int16_t)sc.bodyIdx != _lastBodyIdxDrawn) {
        _orbDrawBodyName(tft, sc.bodyIdx);
    }

    // ── Layer 3: vessel dot on PLAN panel ────────────────────────────────────────────
    // Use the cached scene so vessel position matches what's drawn on screen.
    // (If sig changed but repaint was gated off, the cached scene is still the
    // one that's actually visible.)
    const OrbScene &drawSc = _sceneValid ? _lastScene : sc;

    float nu_rad = state.trueAnomaly * (PI / 180.0f);
    while (nu_rad >  PI) nu_rad -= 2.0f * PI;
    while (nu_rad < -PI) nu_rad += 2.0f * PI;

    uint32_t vt0 = 0, vt1 = 0, vt2 = 0, vt3 = 0;

    int16_t vx, vy;
    bool vesselVisiblePlan = _orbVesselPxPlan(drawSc, nu_rad, vx, vy);

    // Trigger redraw when: no dot currently drawn (covers hidden→visible AND first frame),
    // OR the new position differs from the drawn position by VESSEL_PX_THRESH in either axis.
    // Hidden→hidden case: !_planDotValid is true but vesselVisiblePlan is false, so we
    // enter the block, skip the erase (no old dot), and skip the draw — cheap no-op.
    bool vesselMovedPlan = !_planDotValid
                        || abs((int)vx - (int)_planDotX) >= VESSEL_PX_THRESH
                        || abs((int)vy - (int)_planDotY) >= VESSEL_PX_THRESH;

    if (vesselMovedPlan) {
        if (debugMode) vt0 = micros();

        // Erase old dot (if any), then restore markers and orbit line around
        // the erase region.
        if (_planDotValid) {
            tft.fillCircle(_planDotX, _planDotY, 5, TFT_BLACK);
            if (debugMode) vt1 = micros();

            // Order:
            //  1. Erase (above) — blacks out the 5-px-radius old-dot region.
            //     If the dot was on a marker label, the label pixels are
            //     erased; if on a marker dot, the dot pixels are erased.
            //  2. Redraw full markers (dot + label). If a label was partially
            //     erased, it's restored. The label's text-background rect may
            //     stomp orbit-line pixels where the label overlaps the orbit,
            //     but only inside the label's bbox.
            //  3. Arc patch. Paints orbit-colour over ~14 px of arc around
            //     the old dot, restoring the orbit line continuity across the
            //     erase region INCLUDING any black pixels the label's
            //     background rect just placed over the line.
            //  4. Dots-only marker restore. The patch in step 3 may have
            //     painted orbit-colour through a marker dot (if the vessel
            //     passed through the dot). Re-paint just the dots (no labels,
            //     to avoid re-triggering step 3's label-bbox issue). Dots are
            //     small and localised — this is cheap.
            float oldNu = atan2f((float)(drawSc.focusY - _planDotY),
                                  (float)(_planDotX - drawSc.focusX))
                        - drawSc.argOfPe_rad;
            if (!drawSc.useSoiView) _orbDrawMarkers(tft, drawSc);
            _orbPatchArcPlan(tft, drawSc, oldNu);
            if (!drawSc.useSoiView) _orbDrawMarkerDotsOnly(tft, drawSc);
            if (debugMode) vt2 = micros();
        } else {
            if (debugMode) { vt1 = vt0; vt2 = vt0; }
        }

        // Draw new dot
        if (vesselVisiblePlan) {
            tft.fillCircle(vx, vy, 4, TFT_NEON_GREEN);
            _planDotX     = vx;
            _planDotY     = vy;
            _planDotValid = true;
        } else {
            _planDotValid = false;
        }
        if (debugMode) vt3 = micros();
    }

    // ── Layer 3: vessel dot on INCL panel ────────────────────────────────────────────
    // INCL vessel is at t_v = sin(argPe + nu) along the tilted line.
    float u_rad = drawSc.argOfPe_rad + nu_rad;
    float t_v   = sinf(u_rad);
    int16_t ivx = (int16_t)(INC_CX + t_v * drawSc.dx);
    int16_t ivy = (int16_t)(drawSc.bodyCY - t_v * drawSc.dy);
    bool inclVisible = (ivx >= INCL_X0 && ivx <= INCL_X1 &&
                        ivy >= INCL_CLIP_Y0 && ivy <= INCL_CLIP_Y1);

    bool inclMoved = !_inclDotValid
                  || abs((int)ivx - (int)_inclDotX) >= VESSEL_PX_THRESH
                  || abs((int)ivy - (int)_inclDotY) >= VESSEL_PX_THRESH;

    if (inclMoved) {
        if (_inclDotValid) {
            // Erase old dot — on a 2-px-thick straight line, simplest is to
            // redraw the two parallel line segments locally across a small box
            // around the old dot. Full-line redraw is cheap for straight lines.
            tft.fillCircle(_inclDotX, _inclDotY, 5, TFT_BLACK);

            // Re-draw the local piece of the tilted 2-px line.
            // Project the old dot onto the line parameter to find the local span.
            // Simpler approach: redraw a small segment from (dot-6px) to (dot+6px)
            // along the line direction. This always restores the 2-px line.
            float lineLen = sqrtf(drawSc.dx*drawSc.dx + drawSc.dy*drawSc.dy);
            if (lineLen > 1e-3f) {
                float ux =  drawSc.dx / lineLen;
                float uy = -drawSc.dy / lineLen;
                int16_t a_x = _inclDotX - (int16_t)(8.0f * ux);
                int16_t a_y = _inclDotY - (int16_t)(8.0f * uy);
                int16_t b_x = _inclDotX + (int16_t)(8.0f * ux);
                int16_t b_y = _inclDotY + (int16_t)(8.0f * uy);
                tft.drawLine(a_x, a_y,     b_x, b_y,     drawSc.orbitCol);
                tft.drawLine(a_x, a_y + 1, b_x, b_y + 1, drawSc.orbitCol);
            }

            // Redraw the body disc if the patch area overlapped it — cheap safety.
            int dxb = _inclDotX - INC_CX, dyb = _inclDotY - drawSc.bodyCY;
            if (dxb*dxb + dyb*dyb < (INC_BR + 10) * (INC_BR + 10))
                tft.fillCircle(INC_CX, drawSc.bodyCY, INC_BR, drawSc.bodyCol);
        }
        if (inclVisible) {
            tft.fillCircle(ivx, ivy, 4, TFT_NEON_GREEN);
            _inclDotX     = ivx;
            _inclDotY     = ivy;
            _inclDotValid = true;
        } else {
            _inclDotValid = false;
        }
    }

    // Periodic vessel-update timing print
    if (debugMode && (vesselMovedPlan || inclMoved)) {
        _vesselUpdateCount++;
        if (_vesselUpdateCount % DBG_VESSEL_EVERY == 0) {
            Serial.print(F("[ORB] vessel #"));
            Serial.print(_vesselUpdateCount);
            if (vesselMovedPlan && _planDotValid) {
                Serial.print(F("  plan erase="));
                Serial.print(vt1 - vt0);
                Serial.print(F("us  patch="));
                Serial.print(vt2 - vt1);
                Serial.print(F("us  draw="));
                Serial.print(vt3 - vt2);
                Serial.print(F("us"));
            }
            Serial.print(F("  sinceRepaint="));
            Serial.print(now - _lastSceneRepaint);
            Serial.print(F("ms  pending="));
            Serial.println(_pendingSig ? 1 : 0);
        }
    }

    // ── Readout values — cached to prevent per-frame redraw flicker ──────────────────
    // printValue with PrintState is flicker-free when the string changes width,
    // but when called every frame on an unchanged string the character-cell
    // erase flickers the glyphs visibly. Compare strings before calling.
    const tFont *F = &Roboto_Black_20;
    const uint16_t RH = 26;

    // Compute altSL_m from current true anomaly for display
    float altSL_m = -1.0f;
    if (drawSc.isEscape) {
        float asymAng = acosf(-1.0f / drawSc.ecc);
        if (fabsf(nu_rad) < asymAng - 0.05f) {
            float l_m = drawSc.rPe_m * (drawSc.ecc + 1.0f);
            altSL_m   = l_m / (1.0f + drawSc.ecc * cosf(nu_rad)) - drawSc.bodyR_m;
        }
    } else {
        float l_m = drawSc.sma_m * (1.0f - drawSc.ecc*drawSc.ecc);
        altSL_m   = l_m / (1.0f + drawSc.ecc * cosf(nu_rad)) - drawSc.bodyR_m;
    }

    float PeA_m  = fmaxf(0.0f, state.periapsis);
    float ApA_m  = fmaxf(0.0f, state.apoapsis);
    // timeToPe / timeToAp are used raw (not fmaxf-clamped) in the row-5 block
    // below, because a negative value there means "passed / unavailable" and
    // needs to be distinguishable from a small positive value.

    // Row 0 — Alt.SL
    {
        String v = (altSL_m >= 0.0f) ? formatAlt(altSL_m) : String("---");
        if (v != _lastReadout[0]) {
            printValue(tft, F, 110, ORB_RDY1, 246, RH, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _orbPS[0]);
            _lastReadout[0] = v;
        }
    }
    // Row 1 — Pe
    // Hide when Pe is inside the body (impact trajectory, periapsis below
    // surface). Uses the same physical-altitude rule as the marker visibility
    // check: show iff rPe_m > bodyR_m, which is equivalent to state.periapsis
    // > 0 (the value here PeA_m is already state.periapsis clamped to ≥ 0).
    {
        bool  peHid  = !drawSc.isEscape && (drawSc.rPe_m <= drawSc.bodyR_m);
        String v = peHid ? String("---") : formatAlt(PeA_m);
        if (v != _lastReadout[1]) {
            printValue(tft, F, 110, ORB_RDY2, 246, RH, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _orbPS[1]);
            _lastReadout[1] = v;
        }
    }
    // Row 2 — Ap
    {
        String v = drawSc.isEscape ? String("\x80") : formatAlt(ApA_m);
        if (v != _lastReadout[2]) {
            printValue(tft, F, 110, ORB_RDY3, 246, RH, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _orbPS[2]);
            _lastReadout[2] = v;
        }
    }
    // Row 3 — PRD (orbital period)
    {
        String v = drawSc.isEscape ? String("\x80")
                                   : (state.orbitalPeriod > 0.0f ? formatTime(state.orbitalPeriod)
                                                                 : String("---"));
        if (v != _lastReadout[3]) {
            printValue(tft, F, INC_VALUE_X, INC_RDY1, 267, RH, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _orbPS[3]);
            _lastReadout[3] = v;
        }
    }
    // Row 4 — Arg.Pe
    {
        char buf[10]; dtostrf(state.argOfPe, 1, 1, buf);
        String v = String(buf) + String("\xb0");
        if (v != _lastReadout[4]) {
            printValue(tft, F, INC_VALUE_X, INC_RDY2, 267, RH, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _orbPS[4]);
            _lastReadout[4] = v;
        }
    }
    // Row 5 — T+Pe / T+Ap (dynamic label + value)
    // Pick whichever is next in time, i.e. whichever positive value is smaller.
    // If one is ≤ 0 (already passed or unavailable), use the other. If both
    // are ≤ 0, show "---". For escape orbits Ap doesn't exist, so always show
    // T+Pe regardless of its value.
    {
        // Use the RAW values here, not the fmaxf-clamped ones, so we can
        // distinguish "passed/unavailable" (≤0) from "future" (>0).
        float rawPe = state.timeToPe;
        float rawAp = state.timeToAp;

        bool showPe;
        if (drawSc.isEscape) {
            showPe = true;  // no Ap on escape
        } else if (rawPe > 0.0f && rawAp > 0.0f) {
            showPe = (rawPe <= rawAp);  // both valid — pick nearest
        } else if (rawPe > 0.0f) {
            showPe = true;   // only Pe valid
        } else if (rawAp > 0.0f) {
            showPe = false;  // only Ap valid
        } else {
            showPe = true;   // neither valid — default to Pe, will show "---"
        }

        int8_t nowLabel = showPe ? 0 : 1;
        if (nowLabel != _lastTLabel) {
            // Label flipped — redraw label. fillRect only fires on the flip,
            // not every frame, so no flicker.
            tft.fillRect(INC_LABEL_X, INC_RDY3, 80, RH, TFT_BLACK);
            tft.setFont(F);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setCursor(INC_LABEL_X, INC_RDY3);
            tft.print(showPe ? "T+Pe" : "T+Ap");
            _lastTLabel = nowLabel;
        }
        float t = showPe ? rawPe : rawAp;
        String v = (t > 0.0f) ? formatTime(t) : String("---");
        if (v != _lastReadout[5]) {
            printValue(tft, F, INC_VALUE_X, INC_RDY3, 267, RH, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _orbPS[5]);
            _lastReadout[5] = v;
        }
    }

    // Row 6 (INCL top) — Inc (inclination, degrees)
    // Label "Inc:" is white chrome (drawn in chromeScreen_ORB). Value is the
    // inclination in degrees to 1 decimal place, dark-green like the other
    // readouts.
    {
        char buf[10]; dtostrf(drawSc.incl_deg, 1, 1, buf);
        String v = String(buf) + String("\xb0");
        if (v != _lastReadout[6]) {
            printValue(tft, F, INC_VALUE_X, INC_RDY0, 267, RH, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _orbPS[6]);
            _lastReadout[6] = v;
        }
    }

    // Optional: total frame timing (very lightweight — only prints on scene repaint)
    if (debugMode && sceneNeedsRepaint) {
        uint32_t frameEnd = micros();
        Serial.print(F("[ORB] frame total="));
        Serial.print(frameEnd - frameStart);
        Serial.println(F("us"));
    }
}
