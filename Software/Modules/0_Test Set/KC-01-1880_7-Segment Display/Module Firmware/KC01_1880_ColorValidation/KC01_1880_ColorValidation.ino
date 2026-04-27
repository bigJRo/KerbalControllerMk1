/*
 * KC-01-1880  NeoPixel Colour Validation
 * Board:      ATtiny816  (megaTinyCore, 20 MHz internal)
 *
 * Cycles through every KMC_* palette colour across all 3 NeoPixels.
 * Press BTN_EN to advance to the next colour.
 * The 7-segment display shows the current colour index.
 *
 * Libraries required:
 *   - Kerbal7SegmentCore  (v1.1.0)
 *   - KerbalModuleCommon
 *   - tinyNeoPixel        (bundled with megaTinyCore)
 */

#include <KerbalModuleCommon.h>
#include <Kerbal7SegmentCore.h>

#define I2C_ADDRESS    0x2A
#define MODULE_TYPE_ID KMC_TYPE_GPWS_INPUT

const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {
    { BTN_MODE_MOMENTARY, {K7SC_OFF, K7SC_OFF, K7SC_OFF}, 0, 0 },
    { BTN_MODE_MOMENTARY, {K7SC_OFF, K7SC_OFF, K7SC_OFF}, 0, 0 },
    { BTN_MODE_MOMENTARY, {K7SC_OFF, K7SC_OFF, K7SC_OFF}, 0, 0 },
};

// ── Colour table — RGB values passed directly to setPixelColor ──
// No toGRBW() needed: tinyNeoPixel NEO_GRB handles byte reordering internally.
struct ColorEntry {
    const char* name;
    RGBColor    color;
};

static const ColorEntry COLORS[] = {
    { "OFF",          KMC_OFF           },
    { "GREEN",        KMC_GREEN         },
    { "RED",          KMC_RED           },
    { "AMBER",        KMC_AMBER         },
    { "WHITE_COOL",   KMC_WHITE_COOL    },
    { "WHITE_NEUT",   KMC_WHITE_NEUTRAL },
    { "WHITE_WARM",   KMC_WHITE_WARM    },
    { "WHITE_SOFT",   KMC_WHITE_SOFT    },
    { "ORANGE",       KMC_ORANGE        },
    { "YELLOW",       KMC_YELLOW        },
    { "GOLD",         KMC_GOLD          },
    { "CHARTREUSE",   KMC_CHARTREUSE    },
    { "LIME",         KMC_LIME          },
    { "MINT",         KMC_MINT          },
    { "BLUE",         KMC_BLUE          },
    { "SKY",          KMC_SKY           },
    { "TEAL",         KMC_TEAL          },
    { "CYAN",         KMC_CYAN          },
    { "PURPLE",       KMC_PURPLE        },
    { "INDIGO",       KMC_INDIGO        },
    { "VIOLET",       KMC_VIOLET        },
    { "CORAL",        KMC_CORAL         },
    { "ROSE",         KMC_ROSE          },
    { "PINK",         KMC_PINK          },
    { "MAGENTA",      KMC_MAGENTA       },
};

static const uint8_t NUM_COLORS = sizeof(COLORS) / sizeof(COLORS[0]);
static uint8_t _colorIdx = 0;

static void showColor(uint8_t idx)
{
    RGBColor c = COLORS[idx].color;
    for (uint8_t i = 0; i < K7SC_NEO_COUNT; i++) {
        // Pass R,G,B directly — NEO_GRB reordering handled by tinyNeoPixel
        buttonsSetPixelColor(i, c.r, c.g, c.b);
    }
    buttonsShow();
    displaySetValue(idx);
}

void setup()
{
    k7scBegin(I2C_ADDRESS, MODULE_TYPE_ID, KMC_CAP_DISPLAY, btnConfigs, 0);
    showColor(_colorIdx);
}

void loop()
{
    k7scUpdate();
    if (buttonsGetEncoderPress()) {
        _colorIdx = (_colorIdx + 1) % NUM_COLORS;
        showColor(_colorIdx);
    }
}
