Skip to content
Chat history
You said:
We're going to start with this code an adapt it for another module:
ChatGPT said:

Great! Please go ahead and share the code you'd like to start with. Once I have that, you can tell me what kind of module we're adapting it into (e.g., what its function will be, any changes to the I2C address, LED behavior, button layout, etc.), and I’ll help you adapt it accordingly.
You said:
/********************************************************************************************************************************
  UI Control Module for Kerbal Controller

  Adapted from the Vehicle Control Module by J. Rostoker for Jeb's Controller Works.
  Handles UI-related commands, LED status feedback, and I2C communication with the host.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>          // I2C communication
#include <ShiftIn.h>       // Shift register input
#include <tinyNeoPixel.h>  // NeoPixel support for ATtiny series
#include <avr/pgmspace.h>  // PROGMEM access for ATtiny

/***************************************************************************************
  Pin Definitions (ATtiny816 w/ megaTinyCore Pin Mapping)
****************************************************************************************/
#define SDA 9  // PB1, I2C data pin
#define SCL 8  // PB0, I2C clock pin

#define INT_OUT 4      //  PA4, Interrupt output to notify host of button state change
#define load 7         // PA7, Shift register load pin
#define clockEnable 6  // PA6, Shift register clock enable (active low)
#define clockIn 13     // PB5, Shift register clock input
#define dataInPin 5    // PA5, Shift register data pin
#define neopixCmd 12   // PB4, NeoPixel data output pin

#define led_13 11  // PB3, Lock LED 1
#define led_14 15  // PC2, Lock LED 2
#define led_15 10  // PB2, Lock LED 3
#define led_16 14  // PC1, Lock LED 4

/***************************************************************************************
  Constants and Globals
****************************************************************************************/
constexpr uint8_t panel_addr = 0x20;  // I2C slave address for UIControl
constexpr uint8_t NUM_LEDS = 12;      // Number of addressable NeoPixels
constexpr uint8_t NUM_BUTTONS = 16;   // Number of input buttons

tinyNeoPixel leds = tinyNeoPixel(NUM_LEDS, neopixCmd, NEO_GRB);  // LED object

struct buttonPixel {
  uint8_t r, g, b;  // RGB color values
};

/***************************************************************************************
  Lookup table of RGB color definitions for LED feedback
  Stored in PROGMEM to save precious SRAM on the ATtiny816
  Each entry is a custom-named color used to indicate status of individual functions
  Indexes are mapped via the ColorIndex enum for readability
****************************************************************************************/
const buttonPixel colorTable[] PROGMEM = {
  { 128, 0, 0 },      // RED: General warning or critical function
  { 128, 96, 0 },     // AMBER: Secondary status or transitional state
  { 128, 50, 0 },     // ORANGE: UI prompt or toggle
  { 128, 108, 0 },    // GOLDEN_YELLOW: Status indicator (e.g., deployed)
  { 64, 128, 160 },   // SKY_BLUE: Represents atmosphere or power
  { 0, 128, 0 },      // GREEN: Safe or confirmed
  { 64, 128, 96 },    // MINT: Passive/idle
  { 128, 0, 128 },    // MAGENTA: Communication
  { 0, 128, 128 },    // CYAN: Cooling or environmental
  { 64, 128, 0 },     // LIME: Active
  { 255, 255, 255 },  // WHITE: All-clear or generic
  { 0, 0, 0 },        // BLACK: Off or disabled
  { 32, 32, 32 },     // DIM_GRAY: Neutral, inactive but valid
  { 0, 0, 255 },      // BLUE: Default function
  { 255, 0, 0 },      // BRIGHT_RED: Emergency
  { 0, 255, 0 },      // BRIGHT_GREEN: Enabled
  { 0, 0, 255 },      // BRIGHT_BLUE: Attention
  { 255, 255, 0 },    // YELLOW: Alert
  { 0, 255, 255 },    // AQUA: Coolant/temperature
  { 255, 0, 255 }     // FUCHSIA: Comms or science
};

// Human-readable color index enum
enum ColorIndex : uint8_t {
  RED,
  AMBER,
  ORANGE,
  GOLDEN_YELLOW,
  SKY_BLUE,
  GREEN,
  MINT,
  MAGENTA,
  CYAN,
  LIME,
  WHITE,
  BLACK,
  DIM_GRAY,
  BLUE,
  BRIGHT_RED,
  BRIGHT_GREEN,
  BRIGHT_BLUE,
  YELLOW,
  AQUA,
  FUCHSIA
};

// Helper to fetch RGB color struct from PROGMEM
buttonPixel getColorFromTable(ColorIndex  index) {
  buttonPixel px;
  const uint8_t* ptr = (const uint8_t*)colorTable + index * sizeof(buttonPixel);
  px.r = pgm_read_byte(ptr);
  px.g = pgm_read_byte(ptr + 1);
  px.b = pgm_read_byte(ptr + 2);
  return px;
}

/***************************************************************************************
  Maps physical LED index (0–11) to a color index from colorTable[]
  Used to assign a default visual feedback color per functional button
  All 12 LEDs correspond to direct command buttons
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  AMBER,     // 0: Map Enable
  GREEN,     // 1: Cycle Map -
  GREEN,     // 2: Cycle Map +
  BLUE,      // 3: Navball Mode
  AMBER,     // 4: IVA
  SKY_BLUE,  // 5: Cycle Cam
  GREEN,     // 6: Cycle Ship -
  GREEN,     // 7: Cycle Ship +
  GREEN,     // 8: Reset Focus
  MAGENTA,   // 9: Screen Shot
  AMBER,     //10: UI
  RED        //11: DEBUG
};

/***************************************************************************************
  Defines functional label for each button index (0–11)
  These strings are not used directly in logic, but help map indices to in-game actions
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "Map Enable",      // 0
  "Cycle Map -",     // 1
  "Cycle Map +",     // 2
  "Navball Mode",    // 3
  "IVA",             // 4
  "Cycle Cam",       // 5
  "Cycle Ship -",    // 6
  "Cycle Ship +",    // 7
  "Reset Focus",     // 8
  "Screen Shot",     // 9
  "UI",              //10
  "DEBUG",           //11
  "",                //12 → unused or reserved
  "",                //13 → unused or reserved
  "",                //14 → unused or reserved
  ""                 //15 → unused or reserved
};

// Discrete output pins mapped to final 4 button functions
const uint8_t discreteLEDs[4] = { led_13, led_14, led_15, led_16 };

uint16_t button_state_bits = 0;  // Bitfield for all buttons
ShiftIn<2> shift;                // 16-bit shift register interface

volatile bool updateLED = false;  // Flag from I2C input event
uint16_t led_bits = 0;            // Desired LED state
uint16_t prev_led_bits = 0;       // Previous LED state (to avoid redundant updates)

/***************************************************************************************
  Overlay Color Helper: handles logic for LEDs 8–11
****************************************************************************************/
buttonPixel overlayColor(bool overlayEnabled, bool modeActive, bool localActive, uint8_t colorIndex) {
  if (!overlayEnabled || !modeActive) return getColorFromTable(BLACK);
  if (localActive) return getColorFromTable(colorIndex);
  return getColorFromTable(DIM_GRAY);
}


/***************************************************************************************
  Setup
****************************************************************************************/
void setup() {
  pinMode(INT_OUT, OUTPUT);
  digitalWrite(INT_OUT, HIGH);  // Idle high

  // Initialize discrete output LEDs
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(discreteLEDs[i], OUTPUT);
    digitalWrite(discreteLEDs[i], LOW);
  }

  shift.begin(load, clockEnable, dataInPin, clockIn);  // Setup shift register

  leds.begin();
  delay(10);  // Ensure NeoPixel hardware has time to initialize

  // Set initial colors: white for active slots, black for others
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    buttonPixel px = (i < 8) ? getColorFromTable(WHITE) : getColorFromTable(BLACK);
    leds.setPixelColor(i, px.r, px.g, px.b);
  }
  leds.show();

  // Start I2C and register callbacks
  Wire.begin(SDA, SCL, panel_addr);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

/***************************************************************************************
  Main Loop
****************************************************************************************/
void loop() {
  // Update LEDs if new data has been received over I2C
  if (updateLED) {
    handle_ledUpdate();
    updateLED = false;
  }

  // Check for button state changes
  if (shift.update()) {
    uint16_t newState = 0;
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
      newState |= (shift.state(i) ? 1 : 0) << i;
    }
    button_state_bits = newState;

    digitalWrite(INT_OUT, LOW);  // Signal to master that buttons have changed
  }
}

/***************************************************************************************
  LED Update Handler
****************************************************************************************/
void handle_ledUpdate() {
  uint16_t bits = led_bits;
  uint16_t prevBits = prev_led_bits;
  bool updated = false;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    bool newState = bitRead(bits, i);
    bool oldState = bitRead(prevBits, i);
    if (newState == oldState) continue;  // Skip unchanged states

    // Assign defined color if LED bit is set, or dim gray if not
    buttonPixel px = newState ? getColorFromTable(pixel_Array[i]) : getColorFromTable(DIM_GRAY);
    leds.setPixelColor(i, px.r, px.g, px.b);
    updated = true;
  }

  // Update discrete lock-state LEDs (LED13–LED16) if present
  for (uint8_t i = NUM_LEDS; i < NUM_BUTTONS; i++) {
    bool newState = bitRead(bits, i);
    digitalWrite(discreteLEDs[i - NUM_LEDS], newState ? HIGH : LOW);
  }

  if (updated) leds.show();
  prev_led_bits = bits;
}

/***************************************************************************************
  I2C Event Handlers

  ATtiny816 acts as an I2C SLAVE device at address 0x23.
  The protocol between master and slave uses 4 bytes:

  - Master reads 4 bytes:
    [0] = button state bits 0–7
    [1] = button state bits 8–15
    [2] = LED control bits LSB (LED0–7)
    [3] = LED control bits MSB (LED8–15)

  - Master writes 2 bytes:
    [0] = LED control bits LSB
    [1] = LED control bits MSB

  The LED bits control color changes or status indication depending on the bit state.

****************************************************************************************/
void requestEvent() {
  // Respond to master read request with 4-byte status report
  uint8_t response[4] = {
    (uint8_t)(button_state_bits & 0xFF),  // Bits 0–7 of button state
    (uint8_t)(button_state_bits >> 8),    // Bits 8–15 of button state
    (uint8_t)(led_bits & 0xFF),           // LED LSB (control for LEDs 0–7)
    (uint8_t)(led_bits >> 8)              // LED MSB (control for LEDs 8–15)
  };
  Wire.write(response, sizeof(response));  // Send full report
}

void receiveEvent(int howMany) {
  // Master has sent LED state change request
  if (Wire.available() >= 2) {
    uint8_t lsb = Wire.read();  // Lower 8 bits of new LED bitfield
    uint8_t msb = Wire.read();  // Upper 8 bits of new LED bitfield
    led_bits = (msb << 8) | lsb;

    updateLED = true;  // Set flag to process LED changes in main loop
  }
}
ChatGPT said:

Perfect — this is the finalized UI Control Module code you've archived before.

To begin adapting this into a new module, I just need a few details:
🛠️ New Module Info Needed

    Module Name: What should this module be called (e.g., "Throttle Control", "Science Module", etc.)?

    I2C Address: What is the new panel_addr (e.g., 0x21, 0x22, etc.)?

    Number of LEDs: How many NeoPixels will be used?

    Number of Buttons: How many shift register buttons will this module handle?

    LED-to-Function Mapping:

        What does each LED represent?

        What default colors should be assigned per LED?

    Command Labels:

        What strings should be assigned to each button (up to 16)?

        Which buttons are unused?

    Special Logic:

        Any custom logic for handle_ledUpdate()?

        Should any LED colors vary based on internal state or toggle conditions?

    Discrete LEDs (optional):

        Are led_13–led_16 used for anything in this module?

Once you provide that, I’ll generate a customized .ino file in the same format as your other modules.
You said:
1. Module Name: Action Group Control
ChatGPT said:

Great — module name "Action Group Control" is set.

Next, could you fill in the rest of the info so I can tailor the code properly? Here’s a quick checklist:
🔧 Remaining Config Details for Action Group Control Module

    I2C Address
        → What address should this module use? (e.g., 0x21, 0x22…)

    Number of LEDs
        → How many NeoPixels are in use?

    Number of Buttons
        → Total number of buttons on the module?

    LED-to-Function Mapping
        → For each LED (up to 12), list:

        Function name (e.g., "Action Group 1")

        Desired color (use any of the ColorIndex values like GREEN, RED, etc.)

    Command Labels
        → List up to 16 button labels (some can be blank if unused)

    Custom LED Logic?
        → Does this module need different behavior in handle_ledUpdate()?

    Discrete LEDs?
        → Are the 4 discrete LEDs (led_13–led_16) used? If so, how?

Once I have these, I’ll generate a ready-to-use .ino file for the Action Group Control Module.
You said:
2. I2C Address = 0x29
ChatGPT said:

Got it — the Action Group Control Module will use I2C address 0x29.

To proceed, could you please provide the rest of the configuration:
Remaining Needed:

✅ Module Name: Action Group Control
✅ I2C Address: 0x29

⏳ 3. Number of NeoPixels (LEDs)
⏳ 4. Number of Buttons
⏳ 5. LED-to-Function Mapping (Function names + ColorIndex values)
⏳ 6. Command Labels (up to 16 button names)
⏳ 7. Any special LED update behavior in handle_ledUpdate()?
⏳ 8. Use of discrete LEDs (led_13–led_16)?

Once you fill those in, I’ll deliver the customized .ino file for this module.
You said:
3. Number of NeoPixels (LEDs) = 12
4. Number of Buttons = 16
ChatGPT said:

Perfect — you've now provided:

✅ Module Name: Action Group Control
✅ I2C Address: 0x29
✅ NeoPixels: 12
✅ Buttons: 16

Next steps:
🔜 Still Needed:
5. LED-to-Function Mapping

For each of the 12 LEDs, please provide:

    Function label (e.g., “AG1”, “Toggle Gear”, etc.)

    Default color (choose from predefined ColorIndex values like GREEN, RED, BLUE, etc.)

Example format:

0: "AG1", GREEN  
1: "AG2", GREEN  
2: "AG3", GREEN  
...

6. Button Labels

Provide up to 16 button names. The first 12 will correspond to NeoPixel functions; the final 4 may control discrete outputs or be reserved. Blank entries are fine for unused buttons.
7. Special LED Logic?

Do any of the LEDs require custom behavior (e.g., flashing, conditional dimming, dual-state logic)? Or should it use the same handle_ledUpdate() logic as the UI module?
8. Discrete LEDs (led_13–led_16)?

Should we use these 4 extra pins? If yes:

    What do they indicate?

    Which buttons (if any) are tied to them?

Once I have these, I’ll generate the full module .ino.
You said:
0: "AG1", GREEN  
1: "AG2", GREEN  
2: "AG3", GREEN  
this pattern continues through 11: "AG12", GREEN  
ChatGPT said:

Great — thanks!

You've now defined the full LED-to-function mapping:
✅ 5. LED-to-Function Mapping

0: "AG1",  GREEN  
1: "AG2",  GREEN  
2: "AG3",  GREEN  
3: "AG4",  GREEN  
4: "AG5",  GREEN  
5: "AG6",  GREEN  
6: "AG7",  GREEN  
7: "AG8",  GREEN  
8: "AG9",  GREEN  
9: "AG10", GREEN  
10: "AG11", GREEN  
11: "AG12", GREEN  

Now just need:
🔜 Final Items Needed
6. Button Labels

Do you want them to match the above (i.e., "AG1" through "AG12"), and how should buttons 12–15 be labeled (if used)?
Example:

const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "AG1", "AG2", ..., "AG12", "", "", "", ""
};

7. Special LED Logic?

Should handle_ledUpdate() behave exactly as it does in the UI Control Module (bitwise match = active color, else dim gray)?
8. Discrete LEDs (led_13–led_16)?

Are these used at all in this module? If so:

    Which buttons do they correspond to?

    What do they represent?

Once you confirm these last details, I’ll generate the full ActionGroupControl.ino file for you.
You said:
6. Button Labels: Have them match the above, 7. Special LED Logic?: No special logic, 8. Discrete LEDs are not used in this module
ChatGPT said:

Perfect — all module details are now defined:
✅ Final Configuration for Action Group Control Module

    Name: Action Group Control

    I2C Address: 0x29

    NeoPixels: 12 (for AG1–AG12)

    Buttons: 16

    LED-to-Function Mapping: All GREEN, AG1–AG12

    Button Labels: "AG1" to "AG12", last 4 unused

    LED Logic: Same as UI module (DIM_GRAY if bit off, assigned color if on)

    Discrete LEDs: Not used (led_13–led_16 remain unconnected)

I’ll now generate the .ino file using the same formatting and style as your previous modules.

Here's the ActionGroupControl.ino file, ready to use:

/********************************************************************************************************************************
  Action Group Control Module for Kerbal Controller

  Adapted from the UI Control Module by J. Rostoker for Jeb's Controller Works.
  Handles Action Group toggling, LED status feedback, and I2C communication with the host.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>          // I2C communication
#include <ShiftIn.h>       // Shift register input
#include <tinyNeoPixel.h>  // NeoPixel support for ATtiny series
#include <avr/pgmspace.h>  // PROGMEM access for ATtiny

/***************************************************************************************
  Pin Definitions (ATtiny816 w/ megaTinyCore Pin Mapping)
****************************************************************************************/
#define SDA 9  // PB1, I2C data pin
#define SCL 8  // PB0, I2C clock pin

#define INT_OUT 4      // PA4, Interrupt output to notify host of button state change
#define load 7         // PA7, Shift register load pin
#define clockEnable 6  // PA6, Shift register clock enable (active low)
#define clockIn 13     // PB5, Shift register clock input
#define dataInPin 5    // PA5, Shift register data pin
#define neopixCmd 12   // PB4, NeoPixel data output pin

/***************************************************************************************
  Constants and Globals
****************************************************************************************/
constexpr uint8_t panel_addr = 0x29;  // I2C slave address for Action Group Control
constexpr uint8_t NUM_LEDS = 12;      // Number of addressable NeoPixels
constexpr uint8_t NUM_BUTTONS = 16;   // Number of input buttons

tinyNeoPixel leds = tinyNeoPixel(NUM_LEDS, neopixCmd, NEO_GRB);  // LED object

struct buttonPixel {
  uint8_t r, g, b;  // RGB color values
};

/***************************************************************************************
  Lookup table of RGB color definitions for LED feedback
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

enum ColorIndex : uint8_t {
  RED,
  AMBER,
  ORANGE,
  GOLDEN_YELLOW,
  SKY_BLUE,
  GREEN,
  MINT,
  MAGENTA,
  CYAN,
  LIME,
  WHITE,
  BLACK,
  DIM_GRAY,
  BLUE,
  BRIGHT_RED,
  BRIGHT_GREEN,
  BRIGHT_BLUE,
  YELLOW,
  AQUA,
  FUCHSIA
};

buttonPixel getColorFromTable(ColorIndex index) {
  buttonPixel px;
  const uint8_t* ptr = (const uint8_t*)colorTable + index * sizeof(buttonPixel);
  px.r = pgm_read_byte(ptr);
  px.g = pgm_read_byte(ptr + 1);
  px.b = pgm_read_byte(ptr + 2);
  return px;
}

/***************************************************************************************
  LED Default Color Mapping for AG1–AG12 (All GREEN)
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  GREEN, GREEN, GREEN, GREEN,
  GREEN, GREEN, GREEN, GREEN,
  GREEN, GREEN, GREEN, GREEN
};

/***************************************************************************************
  Button Function Labels (AG1–AG12), last 4 unused
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "AG1", "AG2", "AG3", "AG4",
  "AG5", "AG6", "AG7", "AG8",
  "AG9", "AG10", "AG11", "AG12",
  "", "", "", ""
};

uint16_t button_state_bits = 0;
ShiftIn<2> shift;

volatile bool updateLED = false;
uint16_t led_bits = 0;
uint16_t prev_led_bits = 0;

/***************************************************************************************
  Setup
****************************************************************************************/
void setup() {
  pinMode(INT_OUT, OUTPUT);
  digitalWrite(INT_OUT, HIGH);

  shift.begin(load, clockEnable, dataInPin, clockIn);

  leds.begin();
  delay(10);
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds.setPixelColor(i, getColorFromTable(DIM_GRAY).r,
                          getColorFromTable(DIM_GRAY).g,
                          getColorFromTable(DIM_GRAY).b);
  }
  leds.show();

  Wire.begin(SDA, SCL, panel_addr);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

/***************************************************************************************
  Main Loop
****************************************************************************************/
void loop() {
  if (updateLED) {
    handle_ledUpdate();
    updateLED = false;
  }

  if (shift.update()) {
    uint16_t newState = 0;
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
      newState |= (shift.state(i) ? 1 : 0) << i;
    }
    button_state_bits = newState;
    digitalWrite(INT_OUT, LOW);  // Notify master
  }
}

/***************************************************************************************
  LED Update Handler
****************************************************************************************/
void handle_ledUpdate() {
  uint16_t bits = led_bits;
  uint16_t prevBits = prev_led_bits;
  bool updated = false;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    bool newState = bitRead(bits, i);
    bool oldState = bitRead(prevBits, i);
    if (newState == oldState) continue;

    buttonPixel px = newState ? getColorFromTable(pixel_Array[i])
                              : getColorFromTable(DIM_GRAY);
    leds.setPixelColor(i, px.r, px.g, px.b);
    updated = true;
  }

  if (updated) leds.show();
  prev_led_bits = bits;
}

/***************************************************************************************
  I2C Event Handlers
****************************************************************************************/
void requestEvent() {
  uint8_t response[4] = {
    (uint8_t)(button_state_bits & 0xFF),
    (uint8_t)(button_state_bits >> 8),
    (uint8_t)(led_bits & 0xFF),
    (uint8_t)(led_bits >> 8)
  };
  Wire.write(response, sizeof(response));
}

void receiveEvent(int howMany) {
  if (Wire.available() >= 2) {
    uint8_t lsb = Wire.read();
    uint8_t msb = Wire.read();
    led_bits = (msb << 8) | lsb;
    updateLED = true;
  }
}

Would you like me to prepare a README.md file for this module in the same style as the others?
You said:
please provide an updated pixel_Array with the full readability comments similar to the one in the UI Control module example
ChatGPT said:

Absolutely — here’s the pixel_Array section rewritten with full readability comments to match your established documentation style:

/***************************************************************************************
  Maps physical LED index (0–11) to a color index from colorTable[]
  Used to assign a default visual feedback color per functional button
  All 12 LEDs correspond to Action Group toggle buttons (AG1–AG12)
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  GREEN,  //  0: AG1
  GREEN,  //  1: AG2
  GREEN,  //  2: AG3
  GREEN,  //  3: AG4
  GREEN,  //  4: AG5
  GREEN,  //  5: AG6
  GREEN,  //  6: AG7
  GREEN,  //  7: AG8
  GREEN,  //  8: AG9
  GREEN,  //  9: AG10
  GREEN,  // 10: AG11
  GREEN   // 11: AG12
};

Would you like me to update the full .ino file with this comment block in place? I can also regenerate the matching README.md if you'd like to complete documentation for this module.
You said:
please provide and updated commandNames with the full readability comments similar to the one in the UI Control module example
ChatGPT said:

Certainly! Here’s the commandNames array updated to include full per-index readability comments, following the same format as the UI Control module:

/***************************************************************************************
  Defines functional label for each button index (0–15)
  These strings are not used directly in logic, but help map indices to in-game actions
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "AG1",   //  0: Action Group 1
  "AG2",   //  1: Action Group 2
  "AG3",   //  2: Action Group 3
  "AG4",   //  3: Action Group 4
  "AG5",   //  4: Action Group 5
  "AG6",   //  5: Action Group 6
  "AG7",   //  6: Action Group 7
  "AG8",   //  7: Action Group 8
  "AG9",   //  8: Action Group 9
  "AG10",  //  9: Action Group 10
  "AG11",  // 10: Action Group 11
  "AG12",  // 11: Action Group 12
  "",      // 12 → unused
  "",      // 13 → unused
  "",      // 14 → unused
  ""       // 15 → unused
};

Would you like me to integrate both updated sections into the complete .ino file and re-output it as a final copy? I can also include a README.md for completeness if desired.
You said:
please integrate both updated sections into a complete .ino file
ChatGPT said:

Here is the final integrated ActionGroupControl.ino file, with updated pixel_Array and commandNames sections, matching the format and documentation style used in your other modules:

/********************************************************************************************************************************
  Action Group Control Module for Kerbal Controller

  Adapted from the UI Control Module by J. Rostoker for Jeb's Controller Works.
  Handles Action Group toggling, LED status feedback, and I2C communication with the host.
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>          // I2C communication
#include <ShiftIn.h>       // Shift register input
#include <tinyNeoPixel.h>  // NeoPixel support for ATtiny series
#include <avr/pgmspace.h>  // PROGMEM access for ATtiny

/***************************************************************************************
  Pin Definitions (ATtiny816 w/ megaTinyCore Pin Mapping)
****************************************************************************************/
#define SDA 9  // PB1, I2C data pin
#define SCL 8  // PB0, I2C clock pin

#define INT_OUT 4      // PA4, Interrupt output to notify host of button state change
#define load 7         // PA7, Shift register load pin
#define clockEnable 6  // PA6, Shift register clock enable (active low)
#define clockIn 13     // PB5, Shift register clock input
#define dataInPin 5    // PA5, Shift register data pin
#define neopixCmd 12   // PB4, NeoPixel data output pin

/***************************************************************************************
  Constants and Globals
****************************************************************************************/
constexpr uint8_t panel_addr = 0x29;  // I2C slave address for Action Group Control
constexpr uint8_t NUM_LEDS = 12;      // Number of addressable NeoPixels
constexpr uint8_t NUM_BUTTONS = 16;   // Number of input buttons

tinyNeoPixel leds = tinyNeoPixel(NUM_LEDS, neopixCmd, NEO_GRB);  // LED object

struct buttonPixel {
  uint8_t r, g, b;  // RGB color values
};

/***************************************************************************************
  Lookup table of RGB color definitions for LED feedback
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

enum ColorIndex : uint8_t {
  RED,
  AMBER,
  ORANGE,
  GOLDEN_YELLOW,
  SKY_BLUE,
  GREEN,
  MINT,
  MAGENTA,
  CYAN,
  LIME,
  WHITE,
  BLACK,
  DIM_GRAY,
  BLUE,
  BRIGHT_RED,
  BRIGHT_GREEN,
  BRIGHT_BLUE,
  YELLOW,
  AQUA,
  FUCHSIA
};

buttonPixel getColorFromTable(ColorIndex index) {
  buttonPixel px;
  const uint8_t* ptr = (const uint8_t*)colorTable + index * sizeof(buttonPixel);
  px.r = pgm_read_byte(ptr);
  px.g = pgm_read_byte(ptr + 1);
  px.b = pgm_read_byte(ptr + 2);
  return px;
}

/***************************************************************************************
  Maps physical LED index (0–11) to a color index from colorTable[]
  Used to assign a default visual feedback color per functional button
  All 12 LEDs correspond to Action Group toggle buttons (AG1–AG12)
****************************************************************************************/
constexpr ColorIndex pixel_Array[NUM_LEDS] = {
  GREEN,  //  0: AG1
  GREEN,  //  1: AG2
  GREEN,  //  2: AG3
  GREEN,  //  3: AG4
  GREEN,  //  4: AG5
  GREEN,  //  5: AG6
  GREEN,  //  6: AG7
  GREEN,  //  7: AG8
  GREEN,  //  8: AG9
  GREEN,  //  9: AG10
  GREEN,  // 10: AG11
  GREEN   // 11: AG12
};

/***************************************************************************************
  Defines functional label for each button index (0–15)
  These strings are not used directly in logic, but help map indices to in-game actions
****************************************************************************************/
const char commandNames[NUM_BUTTONS][16] PROGMEM = {
  "AG1",   //  0: Action Group 1
  "AG2",   //  1: Action Group 2
  "AG3",   //  2: Action Group 3
  "AG4",   //  3: Action Group 4
  "AG5",   //  4: Action Group 5
  "AG6",   //  5: Action Group 6
  "AG7",   //  6: Action Group 7
  "AG8",   //  7: Action Group 8
  "AG9",   //  8: Action Group 9
  "AG10",  //  9: Action Group 10
  "AG11",  // 10: Action Group 11
  "AG12",  // 11: Action Group 12
  "",      // 12 → unused
  "",      // 13 → unused
  "",      // 14 → unused
  ""       // 15 → unused
};

uint16_t button_state_bits = 0;
ShiftIn<2> shift;

volatile bool updateLED = false;
uint16_t led_bits = 0;
uint16_t prev_led_bits = 0;

/***************************************************************************************
  Setup
****************************************************************************************/
void setup() {
  pinMode(INT_OUT, OUTPUT);
  digitalWrite(INT_OUT, HIGH);

  shift.begin(load, clockEnable, dataInPin, clockIn);

  leds.begin();
  delay(10);
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds.setPixelColor(i, getColorFromTable(DIM_GRAY).r,
                          getColorFromTable(DIM_GRAY).g,
                          getColorFromTable(DIM_GRAY).b);
  }
  leds.show();

  Wire.begin(SDA, SCL, panel_addr);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
}

/***************************************************************************************
  Main Loop
****************************************************************************************/
void loop() {
  if (updateLED) {
    handle_ledUpdate();
    updateLED = false;
  }

  if (shift.update()) {
    uint16_t newState = 0;
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
      newState |= (shift.state(i) ? 1 : 0) << i;
    }
    button_state_bits = newState;
    digitalWrite(INT_OUT, LOW);  // Notify master
  }
}

/***************************************************************************************
  LED Update Handler
****************************************************************************************/
void handle_ledUpdate() {
  uint16_t bits = led_bits;
  uint16_t prevBits = prev_led_bits;
  bool updated = false;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    bool newState = bitRead(bits, i);
    bool oldState = bitRead(prevBits, i);
    if (newState == oldState) continue;  // Skip unchanged states

    // Assign defined color if LED bit is set, or dim gray if not
    buttonPixel px = newState ? getColorFromTable(pixel_Array[i]) : getColorFromTable(DIM_GRAY);
    leds.setPixelColor(i, px.r, px.g, px.b);
    updated = true;
  }

  if (updated) leds.show();
  prev_led_bits = bits;
}

/***************************************************************************************
  I2C Event Handlers

  ATtiny816 acts as an I2C SLAVE device at address 0x23.
  The protocol between master and slave uses 4 bytes:

  - Master reads 4 bytes:
    [0] = button state bits 0–7
    [1] = button state bits 8–15
    [2] = LED control bits LSB (LED0–7)
    [3] = LED control bits MSB (LED8–15)

  - Master writes 2 bytes:
    [0] = LED control bits LSB
    [1] = LED control bits MSB

  The LED bits control color changes or status indication depending on the bit state.

****************************************************************************************/
void requestEvent() {
  // Respond to master read request with 4-byte status report
  uint8_t response[4] = {
    (uint8_t)(button_state_bits & 0xFF),  // Bits 0–7 of button state
    (uint8_t)(button_state_bits >> 8),    // Bits 8–15 of button state
    (uint8_t)(led_bits & 0xFF),           // LED LSB (control for LEDs 0–7)
    (uint8_t)(led_bits >> 8)              // LED MSB (control for LEDs 8–15)
  };
  Wire.write(response, sizeof(response));  // Send full report
}

void receiveEvent(int howMany) {
  // Master has sent LED state change request
  if (Wire.available() >= 2) {
    uint8_t lsb = Wire.read();  // Lower 8 bits of new LED bitfield
    uint8_t msb = Wire.read();  // Upper 8 bits of new LED bitfield
    led_bits = (msb << 8) | lsb;

    updateLED = true;  // Set flag to process LED changes in main loop
  }
}
