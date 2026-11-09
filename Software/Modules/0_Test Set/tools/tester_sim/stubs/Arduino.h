// Host stand-in for the Arduino core, shared by the tester (XIAO RA4M1) and the
// module firmware (ATtiny816, megaTinyCore) in the tester simulation. Pin calls
// are routed by sim_cpu(): the tester and the module have separate pin spaces.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <limits.h>

typedef uint8_t byte;
typedef bool boolean;

#define HIGH 1
#define LOW  0
#define INPUT        0
#define OUTPUT       1
#define INPUT_PULLUP 2
#define HEX 16
#define DEC 10
#define BIN 2
#define F(s) (s)
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
#define bitRead(v, b) (((v) >> (b)) & 1)
#define _BV(b) (1u << (b))

uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t v);
int  digitalRead(uint8_t pin);
int  analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int v);
void cli();
void sei();
inline void interrupts() { sei(); }
inline void noInterrupts() { cli(); }
extern uint8_t SREG;

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
template <typename T, typename L, typename H>
inline T constrain(T x, L lo, H hi) { return x < (T)lo ? (T)lo : (x > (T)hi ? (T)hi : x); }
#ifndef min
template <typename A, typename B> inline A min(A a, B b) { return a < (A)b ? a : (A)b; }
template <typename A, typename B> inline A max(A a, B b) { return a > (A)b ? a : (A)b; }
#endif

class SimPrint {
public:
    virtual void out(const char* s) = 0;
    void begin(unsigned long) {}
    size_t print(const char* s) { out(s); return strlen(s); }
    size_t print(char c) { char b[2] = {c, 0}; out(b); return 1; }
    size_t print(int v, int base = DEC) { return printNum((long)v, base); }
    size_t print(unsigned int v, int base = DEC) { return printNum((long)v, base); }
    size_t print(long v, int base = DEC) { return printNum(v, base); }
    size_t print(unsigned long v, int base = DEC) { return printNum((long)v, base); }
    size_t print(unsigned char v, int base = DEC) { return printNum((long)v, base); }
    size_t print(double v, int d = 2) { char b[32]; snprintf(b, sizeof b, "%.*f", d, v); out(b); return strlen(b); }
    template <typename T> size_t println(T v) { size_t n = print(v); out("\n"); return n + 1; }
    template <typename T> size_t println(T v, int b) { size_t n = print(v, b); out("\n"); return n + 1; }
    size_t println() { out("\n"); return 1; }
    size_t write(uint8_t c) { return print((char)c); }
    operator bool() const { return true; }
private:
    size_t printNum(long v, int base) {
        char b[40];
        if (base == HEX) snprintf(b, sizeof b, "%lX", (unsigned long)v);
        else snprintf(b, sizeof b, "%ld", v);
        out(b);
        return strlen(b);
    }
};
class SimSerial : public SimPrint { public: void out(const char* s) override; };
extern SimSerial Serial;

// ---------------- Tester (XIAO) pin names ----------------
#define D0 0
#define D1 1
#define D2 2
#define D3 3
#define D4 4
#define D5 5
#define D6 6
#define D7 7
#define D8 8
#define D9 9
#define D10 10

// ---------------- Module (ATtiny816) pin names and ports ----------------
#define PIN_PA0 0
#define PIN_PA1 1
#define PIN_PA2 2
#define PIN_PA3 3
#define PIN_PA4 4
#define PIN_PA5 5
#define PIN_PA6 6
#define PIN_PA7 7
#define PIN_PB0 8
#define PIN_PB1 9
#define PIN_PB2 10
#define PIN_PB3 11
#define PIN_PB4 12
#define PIN_PB5 13
#define PIN_PB6 14
#define PIN_PB7 15
#define PIN_PC0 16
#define PIN_PC1 17
#define PIN_PC2 18
#define PIN_PC3 19
#define PIN_PC4 20
#define PIN_PC5 21

#define PIN0_bm 0x01
#define PIN1_bm 0x02
#define PIN2_bm 0x04
#define PIN3_bm 0x08
#define PIN4_bm 0x10
#define PIN5_bm 0x20
#define PIN6_bm 0x40
#define PIN7_bm 0x80
#define PORT_ISC_RISING_gc 0x02
#define PORT_ISC_FALLING_gc 0x03
#define PORT_ISC_BOTHEDGES_gc 0x01
#define PORT_PULLUPEN_bm 0x08

uint8_t sim_port_in(int port);
void    sim_port_outset(int port, uint8_t mask);
void    sim_port_outclr(int port, uint8_t mask);
uint8_t sim_port_flags(int port);
void    sim_port_flags_clear(int port, uint8_t mask);

struct SimRegIn     { int p; operator uint8_t() const { return sim_port_in(p); } };
struct SimRegOutSet { int p; SimRegOutSet& operator=(uint8_t m) { sim_port_outset(p, m); return *this; } };
struct SimRegOutClr { int p; SimRegOutClr& operator=(uint8_t m) { sim_port_outclr(p, m); return *this; } };
struct SimRegFlags  {
    int p;
    operator uint8_t() const { return sim_port_flags(p); }
    SimRegFlags& operator=(uint8_t m) { sim_port_flags_clear(p, m); return *this; }
};
struct SimPort {
    SimRegIn IN; SimRegOutSet OUTSET; SimRegOutClr OUTCLR; SimRegFlags INTFLAGS;
    uint8_t OUT, DIR, DIRSET, DIRCLR;
    uint8_t PIN0CTRL, PIN1CTRL, PIN2CTRL, PIN3CTRL, PIN4CTRL, PIN5CTRL, PIN6CTRL, PIN7CTRL;
};
extern SimPort PORTA, PORTB, PORTC;
#define ISR(vec) extern "C" void sim_isr_##vec()
