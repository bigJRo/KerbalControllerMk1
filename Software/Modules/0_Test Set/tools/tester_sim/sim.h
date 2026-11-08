// Tester simulation: shared API between the simulation core and the per-module
// operator scenarios.
#pragma once
#include <stdint.h>
#include <string>
#include <vector>

enum SimCpu { CPU_TESTER, CPU_MODULE };
SimCpu sim_cpu();

struct SimText { int x, y, w, h; std::string s; };

namespace sim {
// clock
uint32_t now_ms();

// module inputs
void kbc_set(uint8_t idx, bool on);          // KerbalButtonCore input level (KBC index)
void pin_set(uint8_t pin, bool level);       // module digital input level
void analog_set(uint8_t pin, int v);         // module analog input (0..1023)
int  analog_get(uint8_t pin);
void enc_click(uint8_t pinA, uint8_t pinB, int dir);   // one detent: dir +1 = CW

// module outputs
bool pin_out(uint8_t pin);
int  pwm_out(uint8_t pin);
bool led(uint8_t pin, uint8_t idx, uint8_t rgb[3]);    // last shown NeoPixel colour
std::string seg7();                          // "8.8.8.8." / " 200" / "" (shut down)
int  seg7_intensity();

// connection
void unplug();
void plug();
bool plugged();

// screen
std::string text_in(int y0, int y1);         // all text whose top lies in [y0, y1)
std::vector<SimText> buttons();              // button-row labels
bool has_button(const char* label);
bool tap_button(const char* label);
void tap(int x, int y);
void screenshot(const std::string& name);

// operator action queue
typedef void (*ActFn)(int arg);
void at(uint32_t delay_ms, ActFn fn, int arg);   // run fn(arg) delay_ms from now
bool busy();

// fault injection / options (from --opt key=value)
std::string opt(const char* key, const char* dflt = "");
int  opt_int(const char* key, int dflt);

void log(const char* fmt, ...);
}

// One per module: the simulated operator. The core reads the screen and calls
// these; anything not handled falls back to generic rules (tap PASS on visual
// checks, press/release for walk prompts through input()).
struct Scenario {
    virtual ~Scenario() {}
    virtual void begin() {}
    /** Perform the named input action ("Press", "Set", "Release", "HOLD" ...).
        Return false if the label is unknown. */
    virtual bool input(const std::string& verb, const std::string& label) = 0;
    /** Answer a PASS/FAIL visual check. 1 = PASS, 0 = FAIL, -1 = generic rule. */
    virtual int  visual(const std::string& instr) { (void)instr; return -1; }
    /** Full control of a prompt the generic rules do not cover. */
    virtual bool special(const std::string& heading, const std::string& instr, const std::string& row0) {
        (void)heading; (void)instr; (void)row0; return false;
    }
    /** Called every 1 ms of simulated time (physics). */
    virtual void tick() {}
    /** LED pin and index for a button label (LED walk checks), or false. */
    virtual bool ledOf(const std::string& label, uint8_t& pin, uint8_t& idx) { (void)label; (void)pin; (void)idx; return false; }
};
Scenario* make_scenario(const std::string& name);
