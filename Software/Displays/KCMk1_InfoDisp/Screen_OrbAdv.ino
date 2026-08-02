/***************************************************************************************
   KCMk1 Information Display — ORB ADVANCED MODE
   --------------------------------------------------------------------------------------

   Purpose:
     Text-only full-numeric readout of every orbital element available from
     KerbalSimpit's ORBIT_INFO, APSIDES, APSIDES_TIME, ALTITUDE, and VELOCITY
     messages. Intended as a deep-dive view for orbital-mechanics work — the
     default APSIDES view (Screen_ORB.ino) remains the pilot-friendly graphic
     view.

   Access:
     Reached by tapping the title bar while on the ORBIT screen. State lives
     in _orbAdvancedMode (defined in Screen_ORB.ino). Chrome + draw dispatch
     happens in AAA_Screens.ino based on that flag.

   Layout (rev-2, 940x600 content):
     Two columns of 7 rows each, Roboto_Black_36, 73 px row pitch starting at
     y=90 so the 7 rows fill the full height (last row bottom ~y=571). The
     panel divider sits at x=470 (matching the basic ORB screen); the left
     column fills x=[0,470], the right column x=[470,940]. Left column is
     shape/size quantities; right column is orientation/position quantities.
     Labels white, values dark-green, right-justified to an 8 px inset from
     each half's edge — matches the basic ORB screen's conventions.

   Flicker management:
     Each value uses its own PrintState + cached String so printValue only
     redraws when the displayed text changes (same pattern as basic ORB).
     No scene graphics = no repaint gate needed. Chrome draws all labels
     once; draw() only touches values.

   Formatting:
     - Distances (SMA, Pe, Ap, Alt): formatAlt() — auto-switches m/km/Mm.
     - Velocity: integer m/s, "m/s" suffix.
     - Angles (Inc, LAN, ArgPe, True Anom, Mean Anom): 1 decimal, "°" suffix.
     - Eccentricity: 4 decimal places.
     - Times (T+Pe, T+Ap, Period): formatTimeCompact() — MM:SS under 1h, compact Hh/Dd above.

   Hidden cases:
     - Escape orbit: Ap, Period, T+Ap show "\x80" (infinity glyph); T+Pe still
       valid and Mean Anom still defined.
     - Pe below surface: Pe shows "---".
     - T+Pe or T+Ap negative / unavailable: shows "---".

****************************************************************************************/
#include "KCMk1_InfoDisp.h"

// Note: chromeScreen_OrbAdv and drawScreen_OrbAdv are forward-declared in
// KCMk1_InfoDisp.h for cross-file visibility (AAA_Screens.ino dispatch needs
// them, and Arduino's auto-prototyper doesn't handle alphabetical-merge-order
// reliably for cross-file references).

// ── Layout constants ─────────────────────────────────────────────────────────────────
// Font: Roboto_Black_36 (cap 43, line_space 48). Row pitch 73 px gives ~25 px
// between rows; 7 rows from y=90 span to a last-row bottom of ~571, filling the
// full 600 px height. Value columns are right-justified to an 8 px inset from
// each half's edge (left → x=462 at the divider, right → x=930 at the content
// edge). Widest labels: "SMA:" 91 px (left), "ArgPe:" 114 px (right) — both
// clear their value columns.
static const int16_t ADV_TITLE_TOP  = TITLE_TOP;
static const int16_t ADV_ROW_PITCH  = 73;
static const int16_t ADV_ROW_H      = 48;   // printValue clear height (matches font)
static const int16_t ADV_ROW_Y0     = 90;   // first row top

// Column layout — divider at x=470 (matches basic ORB)
static const int16_t ADV_L_LABEL_X  = 14;
static const int16_t ADV_L_VALUE_X  = 150;
static const int16_t ADV_L_VALUE_W  = 320;  // right edge 150+320-8 = 462

static const int16_t ADV_R_LABEL_X  = 480;
// Value column shifted right so the widest labels ("True Anom:" / "Mean Anom:",
// ~207px, ending ~x=687) clear the value region (regionX = 690+9 = 699). The
// wide time values (T+Pe/T+Ap) belong to short labels, so they still fit.
static const int16_t ADV_R_VALUE_X  = 690;
static const int16_t ADV_R_VALUE_W  = 248;  // right edge 690+248-8 = 930

// Row slot indices (left column 0..6, right column 7..13)
enum {
    ADV_SMA = 0, ADV_ECC, ADV_PE_L, ADV_AP_L, ADV_ALT, ADV_VEL, ADV_PRD,
    ADV_INC = 7, ADV_LAN, ADV_ARGPE, ADV_TA, ADV_MA, ADV_TPE, ADV_TAP,
    ADV_SLOT_COUNT = 14
};

// ── Per-frame state ──────────────────────────────────────────────────────────────────
static PrintState _advPS[ADV_SLOT_COUNT];
static String     _advLastValue[ADV_SLOT_COUNT];

// ── Public entry points ──────────────────────────────────────────────────────────────
void chromeScreen_OrbAdv(KCM_TFT &tft) {
    // Invalidate caches so every value prints on first draw.
    for (uint8_t i = 0; i < ADV_SLOT_COUNT; i++) {
        _advPS[i] = PrintState{};
        _advLastValue[i] = String("\x01");
    }

    // Panel divider — matches basic ORB for visual continuity (x=470, full height)
    tft.drawLine(CONTENT_W / 2,     ADV_TITLE_TOP, CONTENT_W / 2,     SCREEN_H, TFT_GREY);
    tft.drawLine(CONTENT_W / 2 + 1, ADV_TITLE_TOP, CONTENT_W / 2 + 1, SCREEN_H, TFT_GREY);

    // Draw all labels once. All labels are white on black; values are drawn
    // by drawScreen_OrbAdv on each update (dark green).
    tft.setFont(Roboto_Black_36);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Left column labels
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 0 * ADV_ROW_PITCH); tft.print("SMA:");
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 1 * ADV_ROW_PITCH); tft.print("Ecc:");
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 2 * ADV_ROW_PITCH); tft.print("PeA:");
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 3 * ADV_ROW_PITCH); tft.print("ApA:");
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 4 * ADV_ROW_PITCH); tft.print("Alt.SL:");
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 5 * ADV_ROW_PITCH); tft.print("Vel:");
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 6 * ADV_ROW_PITCH); tft.print("Period:");

    // Right column labels
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 0 * ADV_ROW_PITCH); tft.print("Inc:");
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 1 * ADV_ROW_PITCH); tft.print("LAN:");
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 2 * ADV_ROW_PITCH); tft.print("ArgPe:");
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 3 * ADV_ROW_PITCH); tft.print("True Anom:");
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 4 * ADV_ROW_PITCH); tft.print("Mean Anom:");
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 5 * ADV_ROW_PITCH); tft.print("T+Pe:");
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 6 * ADV_ROW_PITCH); tft.print("T+Ap:");
}

void drawScreen_OrbAdv(KCM_TFT &tft) {
    const tFont *F = &Roboto_Black_36;

    // Escape detection — matches basic ORB logic. An escape (open) trajectory
    // has no Ap and no period.
    bool isEscape = (state.eccentricity >= 1.0f) || (state.apoapsis < 0.0f);

    // ── Left column ──────────────────────────────────────────────────────────────────

    // SMA
    {
        String v = formatAlt(state.semiMajorAxis);
        if (v != _advLastValue[ADV_SMA]) {
            printValue(tft, F, ADV_L_VALUE_X, ADV_ROW_Y0 + 0 * ADV_ROW_PITCH,
                       ADV_L_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_SMA]);
            _advLastValue[ADV_SMA] = v;
        }
    }

    // Eccentricity — 4 decimals, small and precise
    {
        char buf[12];
        dtostrf(state.eccentricity, 1, 4, buf);
        String v = String(buf);
        if (v != _advLastValue[ADV_ECC]) {
            printValue(tft, F, ADV_L_VALUE_X, ADV_ROW_Y0 + 1 * ADV_ROW_PITCH,
                       ADV_L_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_ECC]);
            _advLastValue[ADV_ECC] = v;
        }
    }

    // Periapsis altitude — "---" if below surface (state.periapsis < 0)
    {
        String v = (state.periapsis >= 0.0f) ? formatAlt(state.periapsis) : String("---");
        if (v != _advLastValue[ADV_PE_L]) {
            printValue(tft, F, ADV_L_VALUE_X, ADV_ROW_Y0 + 2 * ADV_ROW_PITCH,
                       ADV_L_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_PE_L]);
            _advLastValue[ADV_PE_L] = v;
        }
    }

    // Apoapsis altitude — infinity glyph (\x80) on escape
    {
        String v = isEscape ? String("\x80") : formatAlt(state.apoapsis);
        if (v != _advLastValue[ADV_AP_L]) {
            printValue(tft, F, ADV_L_VALUE_X, ADV_ROW_Y0 + 3 * ADV_ROW_PITCH,
                       ADV_L_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_AP_L]);
            _advLastValue[ADV_AP_L] = v;
        }
    }

    // Current altitude (sea level)
    {
        String v = formatAlt(state.altitude);
        if (v != _advLastValue[ADV_ALT]) {
            printValue(tft, F, ADV_L_VALUE_X, ADV_ROW_Y0 + 4 * ADV_ROW_PITCH,
                       ADV_L_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_ALT]);
            _advLastValue[ADV_ALT] = v;
        }
    }

    // Orbital velocity — integer m/s
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d m/s", (int)roundf(state.orbitalVel));
        String v = String(buf);
        if (v != _advLastValue[ADV_VEL]) {
            printValue(tft, F, ADV_L_VALUE_X, ADV_ROW_Y0 + 5 * ADV_ROW_PITCH,
                       ADV_L_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_VEL]);
            _advLastValue[ADV_VEL] = v;
        }
    }

    // Orbital period — formatTime, infinity on escape (no closed orbit)
    {
        String v = isEscape ? String("\x80") : formatTimeCompact(state.orbitalPeriod);
        if (v != _advLastValue[ADV_PRD]) {
            printValue(tft, F, ADV_L_VALUE_X, ADV_ROW_Y0 + 6 * ADV_ROW_PITCH,
                       ADV_L_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_PRD]);
            _advLastValue[ADV_PRD] = v;
        }
    }

    // ── Right column ─────────────────────────────────────────────────────────────────

    // Inclination — 1 decimal, degree suffix
    {
        char buf[12]; dtostrf(state.inclination, 1, 1, buf);
        String v = String(buf) + String("\xb0");
        if (v != _advLastValue[ADV_INC]) {
            printValue(tft, F, ADV_R_VALUE_X, ADV_ROW_Y0 + 0 * ADV_ROW_PITCH,
                       ADV_R_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_INC]);
            _advLastValue[ADV_INC] = v;
        }
    }

    // LAN — Longitude of Ascending Node
    {
        char buf[12]; dtostrf(state.LAN, 1, 1, buf);
        String v = String(buf) + String("\xb0");
        if (v != _advLastValue[ADV_LAN]) {
            printValue(tft, F, ADV_R_VALUE_X, ADV_ROW_Y0 + 1 * ADV_ROW_PITCH,
                       ADV_R_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_LAN]);
            _advLastValue[ADV_LAN] = v;
        }
    }

    // ArgPe — Argument of Periapsis
    {
        char buf[12]; dtostrf(state.argOfPe, 1, 1, buf);
        String v = String(buf) + String("\xb0");
        if (v != _advLastValue[ADV_ARGPE]) {
            printValue(tft, F, ADV_R_VALUE_X, ADV_ROW_Y0 + 2 * ADV_ROW_PITCH,
                       ADV_R_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_ARGPE]);
            _advLastValue[ADV_ARGPE] = v;
        }
    }

    // TA — True Anomaly
    {
        char buf[12]; dtostrf(state.trueAnomaly, 1, 1, buf);
        String v = String(buf) + String("\xb0");
        if (v != _advLastValue[ADV_TA]) {
            printValue(tft, F, ADV_R_VALUE_X, ADV_ROW_Y0 + 3 * ADV_ROW_PITCH,
                       ADV_R_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_TA]);
            _advLastValue[ADV_TA] = v;
        }
    }

    // MA — Mean Anomaly
    {
        char buf[12]; dtostrf(state.meanAnomaly, 1, 1, buf);
        String v = String(buf) + String("\xb0");
        if (v != _advLastValue[ADV_MA]) {
            printValue(tft, F, ADV_R_VALUE_X, ADV_ROW_Y0 + 4 * ADV_ROW_PITCH,
                       ADV_R_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_MA]);
            _advLastValue[ADV_MA] = v;
        }
    }

    // T+Pe — time until next periapsis (show "---" if Simpit value is ≤ 0)
    {
        String v = (state.timeToPe > 0.0f) ? formatTimeCompact(state.timeToPe) : String("---");
        if (v != _advLastValue[ADV_TPE]) {
            printValue(tft, F, ADV_R_VALUE_X, ADV_ROW_Y0 + 5 * ADV_ROW_PITCH,
                       ADV_R_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_TPE]);
            _advLastValue[ADV_TPE] = v;
        }
    }

    // T+Ap — time until next apoapsis. Infinity on escape (no Ap), "---" if ≤ 0.
    {
        String v;
        if (isEscape)                   v = String("\x80");
        else if (state.timeToAp > 0.0f) v = formatTimeCompact(state.timeToAp);
        else                            v = String("---");
        if (v != _advLastValue[ADV_TAP]) {
            printValue(tft, F, ADV_R_VALUE_X, ADV_ROW_Y0 + 6 * ADV_ROW_PITCH,
                       ADV_R_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_TAP]);
            _advLastValue[ADV_TAP] = v;
        }
    }
}
