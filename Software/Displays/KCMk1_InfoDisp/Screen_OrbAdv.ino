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

   Layout (720x480 content, header strip y=62..92, readout area y=92..456):
     Two columns of 7 rows each, Roboto_Black_28, 40 px row pitch starting at
     y=100. Left column is shape/size quantities; right column is orientation/
     position quantities. Labels white, values dark-green — matches the basic
     ORB screen's conventions.

   Flicker management:
     Each value uses its own PrintState + cached String so printValue only
     redraws when the displayed text changes (same pattern as basic ORB).
     No scene graphics = no repaint gate needed. Chrome draws all labels
     once; draw() only touches values.

   Formatting:
     - Distances (SMA, Pe, Ap, Alt): formatAlt() — auto-switches m/km/Mm.
     - Velocity: integer m/s, "m/s" suffix.
     - Angles (Inc, LAN, ArgPe, TA, MA): 1 decimal, "°" suffix.
     - Eccentricity: 4 decimal places.
     - Times (T+Pe, T+Ap, PRD): formatTime() — HH:MM:SS with Kerbin 6-hr day.

   Hidden cases:
     - Escape orbit: Ap, PRD, T+Ap show "\x80" (infinity glyph); T+Pe still
       valid and MA still defined.
     - Pe below surface: Pe shows "---".
     - T+Pe or T+Ap negative / unavailable: shows "---".

****************************************************************************************/
#include "KCMk1_InfoDisp.h"

// Note: chromeScreen_OrbAdv and drawScreen_OrbAdv are forward-declared in
// KCMk1_InfoDisp.h for cross-file visibility (AAA_Screens.ino dispatch needs
// them, and Arduino's auto-prototyper doesn't handle alphabetical-merge-order
// reliably for cross-file references).

// ── Layout constants ─────────────────────────────────────────────────────────────────
// Font: Roboto_Black_28. Row pitch 40 px gives ~12 px between rows — readable
// at 28-pt glyph height of ~22 px. 7 rows × 40 = 280 px, fits between y=100
// (below header strip) and y=380 (well above the readout strip bottom y=456).
// Value column x-offsets pushed right to clear the wider 28-pt labels:
// "ArgPe:" at 28-pt is ~85 px wide, so value-start at x=130 / x=486 leaves
// ~35 px breathing room after the labels at x=10 / x=366.
static const int16_t ADV_TITLE_TOP  = 62;
static const int16_t ADV_ROW_PITCH  = 40;
static const int16_t ADV_ROW_H      = 32;   // printValue clear height (matches font)
static const int16_t ADV_ROW_Y0     = 100;  // first row top

// Column layout
static const int16_t ADV_L_LABEL_X  = 10;
static const int16_t ADV_L_VALUE_X  = 130;
static const int16_t ADV_L_VALUE_W  = 220;

static const int16_t ADV_R_LABEL_X  = 366;
static const int16_t ADV_R_VALUE_X  = 486;
static const int16_t ADV_R_VALUE_W  = 230;

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
void chromeScreen_OrbAdv(RA8875 &tft) {
    // Invalidate caches so every value prints on first draw.
    for (uint8_t i = 0; i < ADV_SLOT_COUNT; i++) {
        _advPS[i] = PrintState{};
        _advLastValue[i] = String("\x01");
    }

    // Panel divider — matches basic ORB for visual continuity
    tft.drawLine(360, ADV_TITLE_TOP, 360, 480, TFT_GREY);
    tft.drawLine(361, ADV_TITLE_TOP, 361, 480, TFT_GREY);

    // Draw all labels once. All labels are white on black; values are drawn
    // by drawScreen_OrbAdv on each update (dark green).
    tft.setFont(&Roboto_Black_28);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Left column labels
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 0 * ADV_ROW_PITCH); tft.print("SMA:");
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 1 * ADV_ROW_PITCH); tft.print("Ecc:");
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 2 * ADV_ROW_PITCH); tft.print("Pe:");
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 3 * ADV_ROW_PITCH); tft.print("Ap:");
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 4 * ADV_ROW_PITCH); tft.print("Alt:");
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 5 * ADV_ROW_PITCH); tft.print("Vel:");
    tft.setCursor(ADV_L_LABEL_X, ADV_ROW_Y0 + 6 * ADV_ROW_PITCH); tft.print("PRD:");

    // Right column labels
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 0 * ADV_ROW_PITCH); tft.print("Inc:");
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 1 * ADV_ROW_PITCH); tft.print("LAN:");
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 2 * ADV_ROW_PITCH); tft.print("ArgPe:");
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 3 * ADV_ROW_PITCH); tft.print("TA:");
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 4 * ADV_ROW_PITCH); tft.print("MA:");
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 5 * ADV_ROW_PITCH); tft.print("T+Pe:");
    tft.setCursor(ADV_R_LABEL_X, ADV_ROW_Y0 + 6 * ADV_ROW_PITCH); tft.print("T+Ap:");
}

void drawScreen_OrbAdv(RA8875 &tft) {
    const tFont *F = &Roboto_Black_28;

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
        String v = isEscape ? String("\x80") : formatTime(state.orbitalPeriod);
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
        String v = (state.timeToPe > 0.0f) ? formatTime(state.timeToPe) : String("---");
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
        else if (state.timeToAp > 0.0f) v = formatTime(state.timeToAp);
        else                            v = String("---");
        if (v != _advLastValue[ADV_TAP]) {
            printValue(tft, F, ADV_R_VALUE_X, ADV_ROW_Y0 + 6 * ADV_ROW_PITCH,
                       ADV_R_VALUE_W, ADV_ROW_H, "",
                       v, TFT_DARK_GREEN, TFT_BLACK, TFT_BLACK, _advPS[ADV_TAP]);
            _advLastValue[ADV_TAP] = v;
        }
    }
}
