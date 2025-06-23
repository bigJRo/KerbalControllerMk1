/********************************************************************************************************************************
  Rotation Contol Module for Kerbal Controller Mk1

  Handles joystick analog input, RGB LED feedback, and I2C communication with the host.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>          // I2C communication
#include <tinyNeoPixel.h>  // NeoPixel support for ATtiny series
#include <avr/pgmspace.h>  // PROGMEM access for ATtiny

/***************************************************************************************
  Pin Definitions (ATtiny816 w/ megaTinyCore Pin Mapping)
****************************************************************************************/
#define SDA PIN_PB1  // I2C data pin
#define SCL PIN_PB0  // I2C clock pin

#define INT_OUT PIN_PA4     // Interrupt output to notify host of button state change
#define BUTTON_JOY PIN_A5  // Joystick Button Input
#define A3 PIN_PA6          // Joystick axis 3
#define A2 PIN_PA7         // Joystick axis 2
#define A1 PIN_PB5        // Joystick axis 1
#define BUTTON02 PIN_PB3   // RGB Button 2
#define BUTTON01 PIN_PC0   // RGB Button 1

#define neopixCmd PIN_PC1  // NeoPixel data output pin

/***************************************************************************************
  Constants and Globals
****************************************************************************************/
constexpr uint8_t panel_addr = 0x2C;  // I2C slave address for Rotation Control module
constexpr uint8_t NUM_LEDS = 2;       // Number of NeoPixels (attached to RGB buttons)
constexpr uint8_t NUM_BUTTONS = 3;    // Total input buttons (1 joystick + 2 RGB buttons)

unsigned long lastDebounceTime[NUM_BUTTONS] = {0, 0, 0};  // Per-button debounce timing
const unsigned long debounceDelay = 20;  // Debounce delay threshold in ms

uint8_t buttonStates[NUM_BUTTONS] = {0, 0, 0};            // Stable debounced states
uint8_t lastButtonReadings[NUM_BUTTONS] = {0, 0, 0};      // Most recent raw readings
uint8_t buttonBits = 0;                                   // Packed bitfield for I2C transmission

int16_t axis1 = 0;  // Joystick X-axis analog reading
int16_t axis2 = 0;  // Joystick Y-axis analog reading
int16_t axis3 = 0;  // Joystick Z-axis (twist/throttle) reading

tinyNeoPixel leds = tinyNeoPixel(NUM_LEDS, neopixCmd, NEO_GRB);  // NeoPixel chain object

// RGB color structure
struct buttonPixel {
  uint8_t r, g, b;
};

volatile bool updateLED = false;  // Flag indicating pending LED update
uint8_t led_bits = 0;             // RGB LED control bitfield (2 bits)
uint8_t prev_led_bits = 0;        // Previous LED state to detect changes

/***************************************************************************************
  Lookup table of RGB color definitions for LED feedback
  Stored in PROGMEM to save precious SRAM on the ATtiny816
  Each entry is a custom-named color used to indicate status of individual functions
  Indexes are mapped via the ColorIndex enum for readability
****************************************************************************************/
const buttonPixel colorTable[] PROGMEM = {
  { 128, 0, 0 },      // RED
  { 128, 96, 0 },     // AMBER
  { 128, 50, 0 },     // ORANGE
  { 128, 108, 0 },    // GOLDEN_YELLOW
  { 64, 128, 160 },   // SKY_BLUE
  { 0, 128, 0 },      // GREEN
  { 64, 128, 96 },    // MINT
  { 128, 0, 128 },    // MAGENTA
  { 0, 128, 128 },    // CYAN
  { 64, 128, 0 },     // LIME
  { 255, 255, 255 },  // WHITE
  { 0, 0, 0 },        // BLACK
  { 32, 32, 32 },     // DIM_GRAY
  { 0, 0, 255 },      // BLUE
  { 255, 0, 0 },      // BRIGHT_RED
  { 0, 255, 0 },      // BRIGHT_GREEN
  { 0, 0, 255 },      // BRIGHT_BLUE
  { 255, 255, 0 },    // YELLOW
  { 0, 255, 255 },    // AQUA
  { 255, 0, 255 }     // FUCHSIA
};

// Named index for color lookup
enum ColorIndex : uint8_t {
  RED, AMBER, ORANGE, GOLDEN_YELLOW, SKY_BLUE, GREEN, MINT, MAGENTA, CYAN, LIME,
  WHITE, BLACK, DIM_GRAY, BLUE, BRIGHT_RED, BRIGHT_GREEN, BRIGHT_BLUE, YELLOW, AQUA, FUCHSIA
};

// Fetch RGB triplet from PROGMEM
buttonPixel getColorFromTable(ColorIndex index) {
  buttonPixel px;
  const uint8_t* ptr = (const uint8_t*)colorTable + index * sizeof(buttonPixel);
  px.r = pgm_read_byte(ptr);
  px.g = pgm_read_byte(ptr + 1);
  px.b = pgm_read_byte(ptr + 2);
  return px;
}

/***************************************************************************************
  Maps physical LED index (0–7) to a color index from colorTable[]
  Used to assign a default visual feedback color per functional button
  All 12 NeoPixels correspond to command buttons (0–1)
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  GREEN,  // LED 0: Reset Trim
  AMBER   // LED 1: Trim
};

/***************************************************************************************
  Defines functional label for each button index (0–2)
  These strings are not used directly in logic, but help map indices to in-game actions
  ****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "Reset Trim",  // 0
  "Trim",        // 1
  "Airbrake",    // 2
};

/***************************************************************************************
  Setup
****************************************************************************************/
void setup() {
  pinMode(INT_OUT, OUTPUT);
  digitalWrite(INT_OUT, HIGH);  // Idle state for interrupt line

  // Button and analog inputs
  pinMode(BUTTON01, INPUT);
  pinMode(BUTTON02, INPUT);
  pinMode(BUTTON_JOY, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);

  // Initialize NeoPixels
  leds.begin();
  delay(10);  // Allow hardware to stabilize

  // Set initial LED states (white for populated slots)
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    buttonPixel px = getColorFromTable(WHITE);
    leds.setPixelColor(i, px.r, px.g, px.b);
  }
  leds.show();

  // Set up I2C and define handlers
  Wire.begin(SDA, SCL, panel_addr);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

/***************************************************************************************
  Main Loop
****************************************************************************************/
void loop() {
  // Check for updated LED command from master
  if (updateLED) {
    handle_ledUpdate();
    updateLED = false;
  }

  // Read and debounce all 3 button inputs
  uint8_t rawButtons[NUM_BUTTONS] = {
    digitalRead(BUTTON01),
    digitalRead(BUTTON02),
    digitalRead(BUTTON_JOY)
  };

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    if (rawButtons[i] != lastButtonReadings[i]) {
      lastDebounceTime[i] = millis();
      lastButtonReadings[i] = rawButtons[i];
    }
    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (rawButtons[i] != buttonStates[i]) {
        buttonStates[i] = rawButtons[i];
        digitalWrite(INT_OUT, LOW);  // Assert INT to host on valid state change
      }
    }
  }

  // Pack debounced button states into bitfield
  buttonBits = 0;
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    if (buttonStates[i]) buttonBits |= (1 << i);
  }

  // Read raw analog values from all 3 joystick axes
  axis1 = analogRead(A1);
  axis2 = analogRead(A2);
  axis3 = analogRead(A3);
}

/***************************************************************************************
  LED Update Handler – Reacts to host's I2C write and updates RGB LEDs accordingly
****************************************************************************************/
void handle_ledUpdate() {
  bool updated = false;
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    bool newState = bitRead(led_bits, i);
    bool oldState = bitRead(prev_led_bits, i);
    if (newState != oldState) {
      buttonPixel px = newState ? getColorFromTable(pixel_Array[i]) : getColorFromTable(DIM_GRAY);
      leds.setPixelColor(i, px.r, px.g, px.b);
      updated = true;
    }
  }
  if (updated) leds.show();
  prev_led_bits = led_bits;
}

/***************************************************************************************
  I2C Event Handlers

  Master Read (7 bytes):
    [0] = buttonBits
    [1–2] = axis1 (MSB, LSB)
    [3–4] = axis2 (MSB, LSB)
    [5–6] = axis3 (MSB, LSB)

  Master Write (1 byte):
    [0] = LED bitfield for 2 RGB buttons
****************************************************************************************/
void requestEvent() {
  uint8_t response[7] = {
    buttonBits,
    (uint8_t)(axis1 >> 8), (uint8_t)(axis1 & 0xFF),
    (uint8_t)(axis2 >> 8), (uint8_t)(axis2 & 0xFF),
    (uint8_t)(axis3 >> 8), (uint8_t)(axis3 & 0xFF)
  };
  Wire.write(response, sizeof(response));

  digitalWrite(INT_OUT, HIGH);
}

void receiveEvent(int howMany) {
  if (Wire.available() >= 1) {
    led_bits = Wire.read();
    updateLED = true;
  }
}
