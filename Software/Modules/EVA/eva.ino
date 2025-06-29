/********************************************************************************************************************************
  EVA Module for Kerbal Controller

  EVA Display Module for Kerbal Controller Mk1 - ItsyBitsy M4
  Handles 6 RGB LED buttons, ST7789 round LCD, and I²C communication with LVGL GUI
  Written by J. Rostoker for Jeb's Controller Works
********************************************************************************************************************************/
#include <Wire.h>
#include <LovyanGFX.hpp>
#include <Adafruit_NeoPixel.h>
#include <lvgl.h>
#include <math.h>

#include "futura_12.c"
#include "futura_bold_20.c"
#include "futura_bold_28.c"
LV_FONT_DECLARE(futura_12);
LV_FONT_DECLARE(futura_bold_20);
LV_FONT_DECLARE(futura_bold_28);

/***************************************************************************************
   Pin Definitions
****************************************************************************************/
#define INT_OUT 5    // I2C interrupt output to host (active low)
#define neopixCmd 2  // NeoPixel control pin
#define BUTTON01 7   // Button 1 input
#define BUTTON02 9   // Button 2 input
#define BUTTON03 10  // Button 3 input
#define BUTTON04 11  // Button 4 input
#define BUTTON05 12  // Button 5 input
#define BUTTON06 13  // Button 6 input
#define TFT_CS 9     // ST7789 chip select
#define TFT_DC 8     // ST7789 data/command
#define TFT_RST 7    // ST7789 reset
#define BACKLIGHT 5  // ST7789 backlight control

/***************************************************************************************
   Global Constants and Variables
****************************************************************************************/
#define NUM_BUTTONS 6
#define NUM_PIXELS 6
#define LED_POWER_MASK 0x8000  // MSB used to signal power-saving mode

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
constexpr int CENTER_X = DISPLAY_WIDTH / 2;   // X center of circular gauge
constexpr int CENTER_Y = DISPLAY_HEIGHT / 2;  // Y center of circular gauge
constexpr float PI_F = 3.1415926;
constexpr float START_ANGLE = 135.0;  // Start angle of fuel arc
constexpr float SWEEP_ANGLE = 270.0;  // Sweep angle of full gauge

// LED Color Definitions
#define COLOR_OFF pixels.Color(0, 0, 0)
#define COLOR_DIM pixels.Color(32, 32, 32)
#define COLOR_GREEN pixels.Color(0, 128, 0)

const uint8_t buttonPins[NUM_BUTTONS] = { BUTTON01, BUTTON02, BUTTON03, BUTTON04, BUTTON05, BUTTON06 };
bool buttonStates[NUM_BUTTONS] = { false };
bool lastReadings[NUM_BUTTONS] = { false };
unsigned long lastDebounceTimes[NUM_BUTTONS] = { 0 };
const unsigned long debounceDelay = 50;        // 50ms debounce
uint8_t button_event_bits = 0;                 // Bitmask of button presses
bool moduleActive = true;                      // Module enabled by host
uint8_t eva_fuel = 0;                          // Current fuel percentage (0–100)
uint16_t ledStates = 0;                        // Bitfield of NeoPixel modes and power state
bool updateDisplay = false;                    // Flag to refresh gauge and LEDs
uint32_t lastPixelColors[NUM_PIXELS] = { 0 };  // Pixel cache to avoid redundant updates

Adafruit_NeoPixel pixels(NUM_PIXELS, neopixCmd, NEO_GRB + NEO_KHZ800);

// Custom LovyanGFX setup for ST7789 round display
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX() {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = VSPI_HOST;
      cfg.spi_mode = 3;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = 1;
      cfg.pin_sclk = SCK;
      cfg.pin_mosi = MOSI;
      cfg.pin_miso = MISO;
      cfg.pin_dc = TFT_DC;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = TFT_CS;
      cfg.pin_rst = TFT_RST;
      cfg.pin_busy = -1;
      cfg.memory_width = DISPLAY_WIDTH;
      cfg.memory_height = DISPLAY_HEIGHT;
      cfg.panel_width = DISPLAY_WIDTH;
      cfg.panel_height = DISPLAY_HEIGHT;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance);
  }
};

LGFX tft;

/***************************************************************************************
   LVGL Display Flush Callback
****************************************************************************************/
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint16_t w = area->x2 - area->x1 + 1;
  uint16_t h = area->y2 - area->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.writePixels((lgfx::rgb565_t *)color_p, w * h);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

/***************************************************************************************
   LVGL Gauge UI Setup
****************************************************************************************/
lv_obj_t *gauge_arc;
lv_obj_t *gauge_label;
lv_obj_t *title_label;

// Draws the fixed outer tick marks (major every 10%, minor every 2.5%)
void drawGaugeTicks() {
  constexpr int radius = 90;
  constexpr int majorLength = 12;
  constexpr int minorLength = 6;

  for (float pct = 0; pct <= 100; pct += 2.5) {
    float angle_deg = START_ANGLE - (pct / 100.0) * SWEEP_ANGLE;
    float angle_rad = angle_deg * PI_F / 180.0;

    int x0 = CENTER_X + cosf(angle_rad) * radius;
    int y0 = CENTER_Y - sinf(angle_rad) * radius;
    int len = ((int)pct % 10 == 0) ? majorLength : minorLength;
    int x1 = CENTER_X + cosf(angle_rad) * (radius - len);
    int y1 = CENTER_Y - sinf(angle_rad) * (radius - len);

    static lv_point_t line_points[2];
    line_points[0].x = x0;
    line_points[0].y = y0;
    line_points[1].x = x1;
    line_points[1].y = y1;

    lv_obj_t *line = lv_line_create(lv_scr_act());
    lv_line_set_points(line, line_points, 2);
    lv_obj_set_style_line_width(line, 2, 0);

    if (pct <= 10) lv_obj_set_style_line_color(line, lv_palette_main(LV_PALETTE_RED), 0);
    else if (pct <= 25) lv_obj_set_style_line_color(line, lv_palette_main(LV_PALETTE_YELLOW), 0);
    else lv_obj_set_style_line_color(line, lv_color_white(), 0);

    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);

    if ((int)pct % 10 == 0) {
      char buf[4];
      snprintf(buf, sizeof(buf), "%d", (int)pct);
      int lx = CENTER_X + cosf(angle_rad) * (radius - majorLength - 10);
      int ly = CENTER_Y - sinf(angle_rad) * (radius - majorLength - 10);
      lv_obj_t *label = lv_label_create(lv_scr_act());
      lv_label_set_text(label, buf);
      lv_obj_set_style_text_color(label, lv_color_white(), 0);
      lv_obj_set_style_text_font(label, &futura_12, 0);
      lv_obj_align(label, LV_ALIGN_TOP_LEFT, lx - 6, ly - 6);
    }
  }
}

// Create the circular fuel gauge with title and dynamic label
void setupGaugeUI() {
  gauge_arc = lv_arc_create(lv_scr_act());
  lv_obj_set_size(gauge_arc, 180, 180);
  lv_obj_center(gauge_arc);
  lv_arc_set_bg_angles(gauge_arc, 135, 45);
  lv_arc_set_rotation(gauge_arc, 135);
  lv_arc_set_value(gauge_arc, 0);
  lv_obj_clear_flag(gauge_arc, LV_OBJ_FLAG_CLICKABLE);

  gauge_label = lv_label_create(lv_scr_act());
  lv_obj_align_to(gauge_label, gauge_arc, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text_fmt(gauge_label, "0%%");
  lv_obj_set_style_text_font(gauge_label, &futura_bold_28, 0);

  title_label = lv_label_create(lv_scr_act());
  lv_label_set_text(title_label, "EVA Fuel");
  lv_obj_set_style_text_font(title_label, &futura_bold_20, 0);
  lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
  lv_obj_align(title_label, LV_ALIGN_BOTTOM_MID, 0, -4);

  drawGaugeTicks();
}

// Update the gauge arc and label based on fuel percentage
static inline void updateFuelGauge(uint8_t percent) {
  if (percent > 100) percent = 100;
  lv_arc_set_value(gauge_arc, percent);
  lv_label_set_text_fmt(gauge_label, "%d%%", percent);
}
