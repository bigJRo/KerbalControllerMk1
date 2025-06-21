/********************************************************************************************************************************
  UI Control Module for Kerbal Controller Mk1
  References UntitledSpaceCraft module code by CodapopKSP (https://github.com/CodapopKSP/UntitledSpaceCraft)
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/

#include <Wire.h>          // I2C library
#include <ShiftIn.h>       // Input Shift Register
#include <tinyNeoPixel.h>  // NeoPixel library for megaAVR (e.g., ATtiny816)

/***************************************************************************************
  Pin Definitions (ATtiny816 w/ megaTinyCore Pin Mapping)
****************************************************************************************/
#define SDA          9   // PB1
#define SCL          8   // PB0

#define INT_OUT      4   // PA4
#define load         7   // PA7
#define clockEnable  6   // PA6
#define clockIn      13  // PB5
#define dataInPin    5   // PA5
#define neopixCmd    12  // PB4

#define led_13       11  // PB3
#define led_14       15  // PC2
#define led_15       10  // PB2
#define led_16       14  // PC1

/***************************************************************************************
  Definitions
****************************************************************************************/
#define panel_addr     0x20
#define NUM_LEDS       12
#define NUM_BUTTONS    16

tinyNeoPixel leds = tinyNeoPixel(NUM_LEDS, neopixCmd, NEO_GRB);

struct buttonPixel {
  char cmd[16];
  uint8_t r, g, b;
};

const buttonPixel pixel_Default = { "Default", 32, 32, 32 };

const buttonPixel pixel_Array[NUM_LEDS] = {
  { "Map Enable",     0, 255,   0 },
  { "Map Cycle -",    0, 255,   0 },
  { "Map Cycle +",    0, 255,   0 },
  { "Nav Mode",       0, 255,   0 },
  { "IVA",            0, 255,   0 },
  { "Cycle Cam",      0, 255,   0 },
  { "Cycle Ship -",   0, 255,   0 },
  { "Cycle Ship +",   0, 255,   0 },
  { "Reset Focus",    0, 255,   0 },
  { "Screen Shot",    0,   0, 255 },
  { "UI",          255, 191,   0 },
  { "DEBUG",       255,   0,   0 }
};

const uint8_t discreteLEDs[4] = { led_13, led_14, led_15, led_16 };

uint8_t button_bytes[2] = {0, 0};
uint8_t x[NUM_BUTTONS] = {0};
ShiftIn<2> shift;

volatile bool updateLED = false;
uint16_t led_bits = 0;
uint16_t prev_led_bits = 0;

/***************************************************************************************
  Setup
****************************************************************************************/
void setup() {
  pinMode(INT_OUT, OUTPUT);
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(discreteLEDs[i], OUTPUT);
    digitalWrite(discreteLEDs[i], LOW);
  }
  digitalWrite(INT_OUT, HIGH);

  shift.begin(load, clockEnable, dataInPin, clockIn);

  leds.begin();
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds.setPixelColor(i, pixel_Default.r, pixel_Default.g, pixel_Default.b);
  }
  leds.show();

  Wire.begin(SDA, SCL, panel_addr);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

/***************************************************************************************
  Loop
****************************************************************************************/
void loop() {
  if (updateLED) {
    handle_ledUpdate();
    updateLED = false;
  }

  if (shift.update()) {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
      x[i] = shift.state(i);
    }

    button_bytes[0] = 0;
    button_bytes[1] = 0;
    for (uint8_t i = 0; i < 8; i++) {
      button_bytes[0] |= (x[i] & 1) << i;
      button_bytes[1] |= (x[i + 8] & 1) << i;
    }

    digitalWrite(INT_OUT, LOW);  // Notify master
  }
}

/***************************************************************************************
  Handle LED Updates
****************************************************************************************/
void handle_ledUpdate() {
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    bool newState = bitRead(led_bits, i);
    bool oldState = bitRead(prev_led_bits, i);

    if (newState != oldState) {
      if (i < NUM_LEDS) {
        const buttonPixel& px = newState ? pixel_Array[i] : pixel_Default;
        leds.setPixelColor(i, px.r, px.g, px.b);
      } else if (i < NUM_BUTTONS) {
        digitalWrite(discreteLEDs[i - NUM_LEDS], newState ? HIGH : LOW);
      }
    }
  }

  leds.show();
  prev_led_bits = led_bits;
}

/***************************************************************************************
  I2C Events
****************************************************************************************/
void requestEvent() {
  Wire.write(button_bytes[0]);
  Wire.write(button_bytes[1]);
}

void receiveEvent(int howMany) {
  if (Wire.available() >= 2) {
    uint8_t lsb = Wire.read();
    uint8_t msb = Wire.read();
    led_bits = (msb << 8) | lsb;
    updateLED = true;
  }
}
