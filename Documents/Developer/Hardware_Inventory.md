# KCMk1 Hardware Inventory (working notes)

| | |
|---|---|
| **Document type** | Developer — working notes |
| **Status** | In progress: we are going through every board and panel together. This becomes the source for the Hardware Reference rewrite |
| **Source** | The schematics only. Where an older document disagrees, it is noted under *Differences* |

---

## 1. Boards confirmed

| Board | Schematic | PCB | Revision | Role |
|---|---|---|---|---|
| Ctrl Module | KC-01-1701 | KC-01-1702 | V2.0 | Master Teensy 4.1, power latch, INA228, EMC2101, I2C accelerators, USB hub |
| Ctrl Module Extension | KC-01-1711 | KC-01-1712 | V2.0 | Passive: splits the 50-pin interboard cable to the B side, fan, expansion terminal |
| Panel Hub | KC-01-1731 | KC-01-1732 | V2.0 | Input-panel hub: 8 module slots + safe switch |
| 7" TFT Display Driver | KC-01-1911 | KC-01-1912 | V2.1 | One 7" display. On each display panel one driver also acts as the hub: 3 module slots + safe switch |
| Button Module | KC-01-1801 | KC-01-1802 | V2.0 | 12 RGB buttons + 4 panel inputs |
| Wide Button Module | KC-01-1811 | KC-01-1812 | V2.0 | 12 RGB buttons + 8 panel inputs |
| Throttle Module | KC-01-1871 | KC-01-1872 | V2.0 | Motorised fader, touch sensor, 5 illuminated panel inputs |
| Joystick Module | KC-01-1831 | KC-01-1832 | V2.0 | 3-axis stick + stick button (terminal), 2 RGB buttons |
| 7-Segment Display Module | KC-01-1841 | KC-01-1842 | V2.0 | 4-digit display, encoder with push, 3 RGB buttons |

**Retired:** Display Hub (KC-01-1721 / 1722). Its job is done by the 7" TFT Display Driver.

## 2. How the boards connect

```
Ctrl Module (KC-01-1701)
 ├─ CN3  34-pin ── A2 ── Panel Hub ............ INT_1–INT_8,  INT_SAFE
 ├─ CN4  34-pin ── A1 ── 7" Display Driver .... INT_9–INT_12, INT_SAFE2
 ├─ CN1  fan (PWM + tach)
 └─ CN2  50-pin ── Ctrl Module Extension (KC-01-1711)
                    ├─ CN4  34-pin ── B2 ── Panel Hub ........... B_INT_1–B_INT_8,  B_INT_SAFE
                    ├─ CN5  34-pin ── B1 ── 7" Display Driver ... B_INT_9–B_INT_12, B_INT_SAFE2
                    ├─ CN3  fan (PWM only)
                    └─ CN2  10-pin terminal ── expansion unit
```

The A and B sides are wired the same way: an 8-line connector for the input panel's Panel Hub, and a 4-line connector for the display panel's hub driver.

## 3. 34-pin panel connector

Both kinds of panel connector share one pinout. A display-driver connector carries only four of the eight interrupt lines.

| Pins | Signal |
|---|---|
| 1–8 | V_12 |
| 9–16, 20, 30–34 | GND |
| 17 / 18 | SDA_BUS / SCL_BUS |
| 19 | RST |
| 21–28 | INT_1 … INT_8 (display connector: 21–24 only; 25–28 not connected) |
| 29 | INT_SAFE |

**Line mapping:**

| Panel connector | Pins 21–24 | Pins 25–28 | Pin 29 |
|---|---|---|---|
| A2 (Ctrl CN3) | INT_1–4 | INT_5–8 | INT_SAFE |
| A1 (Ctrl CN4) | INT_9–12 | — | INT_SAFE2 |
| B2 (Ext CN4) | B_INT_1–4 | B_INT_5–8 | B_INT_SAFE |
| B1 (Ext CN5) | B_INT_9–12 | — | B_INT_SAFE2 |

**On a display driver** the four lines are used like this:
- **Pins 21–23:** its module slots P2, P3 and P4.
- **Pin 24:** the driver's own interrupt.

So on A1, INT_12 is the hub display, and INT_9–INT_11 are its slots.

## 4. Ctrl Module Extension (KC-01-1711 V2.0)

| Connector | What | Notes |
|---|---|---|
| CN1 | 50-pin interboard, to the Ctrl Module | Same pinout as Ctrl Module CN2 |
| CN4 | B2 panel connection (34-pin, 8 lines) | Silkscreen "B2 PANEL CONNECTION" |
| CN5 | B1 panel connection (34-pin, 4 lines) | Silkscreen "B1 PANEL CONNECTION" |
| CN3 | Fan, 4-pin | 1 FAN_PWM, 2 not connected, 3 V_12, 4 GND. No tach: only the Ctrl Module's fan reports RPM |
| CN2 | Expansion terminal, 10-pin (DB125-3.81) | 1–2 V_12, 3–4 GND, 5 EXT_INT_2, 6 EXT_INT_1, 7 RST, 8 SCL_EXT, 9 SDA_EXT, 10 not connected |

The board has no active components or protection; every signal passes straight through.

## 5. Module boards

### 5.1 Common to every ATtiny816 module

- **Slot connector P1 (16-pin):** pin 13 is INT, driven from the ATtiny through a 10 kΩ / 20 kΩ divider; pin 14 (RST) and pin 16 are not connected.
- **Power and I2C:** each board makes its own 5 V from the 12 V slot supply. An I2C level shifter sits between the 3.3 V bus and the 5 V ATtiny.
- **No address pins.** The I2C address and role are set by the firmware flashed (UPDI header H1). For example, UI Control and Action Control are the same board (1801) with different firmware.
- **Panel inputs** (screw terminals) are active-high. Each terminal has a +5 V pin, and each input a 10 kΩ pull-down: a switch from +5 V to the input reads 1 when closed.

### 5.2 Button Module (KC-01-1801 V2.0)

| Inputs | Where |
|---|---|
| B0–B11 | RGB buttons on the board (one NeoPixel chain) |
| B12, B13 | Terminal CN2 (4-pin): 1–2 +5 V, 3 B12, 4 B13 |
| B14, B15 | Terminal CN1 (4-pin): 1 B15, 2 B14, 3–4 +5 V |

### 5.3 Wide Button Module (KC-01-1811 V2.0)

| Inputs | Where |
|---|---|
| B0–B11 | RGB buttons on the board |
| B12–B15 | Not connected |
| B16–B19 | Terminal U16 (6-pin): 1–2 +5 V, 3 B19, 4 B18, 5 B17, 6 B16 |
| B20–B23 | Terminal U17 (6-pin): 1 B23, 2 B22, 3 B21, 4 B20, 5–6 +5 V |

### 5.4 Throttle Module (KC-01-1871 V2.0)

- **Fader:** motorised 10 kΩ slide pot (wiper to the ATtiny), L293D H-bridge on its own 9 V supply, AT42QT1010 touch sensor on the fader track, direction LEDs.
- **Panel inputs:** five 4-pin terminals, each pin 1 +5 V, pin 2 input, pin 3 +5 V (lamp), pin 4 lamp return:
  - U5: THRTL_ENA;
  - U7: THRTL_100;
  - U8: THRTL_UP;
  - U10: THRTL_DOWN;
  - U11: THRTL_00.
- **Lamps:** all five share one enable (LED_ENA).
- **THRTL_ENA** is not used by the module firmware ("not wired in the final build").

### 5.5 Joystick Module (KC-01-1831 V2.0)

- **On the board:** BTN01, BTN02 (RGB buttons).
- **Stick, on terminal U6 (6-pin):**

  | Pin | Signal |
  |---|---|
  | 6 | +5 V |
  | 5 | AXIS1 |
  | 4 | AXIS2 |
  | 3 | AXIS3 |
  | 2 | BUTTON_JOY |
  | 1 | GND |

### 5.6 7-Segment Display Module (KC-01-1841 V2.0)

- **Display:** 4-digit 7-segment (FJ4401AG), driven by a MAX7219.
- **Controls:** rotary encoder with push button; three buttons, each with an SK6812 LED.
- **Panel inputs:** none.

## 6. Differences from older documents

| # | Topic | Older document says | Schematic shows |
|---|---|---|---|
| 1 | ST7789 status display | Hardware Reference §5.1: on the Ctrl Module | None; SPI pins unconnected |
| 2 | EXT_INT protection | 47 Ω on the EXT_INT_2 output; pull-up on EXT_INT_1 | 47 Ω and pull-up both on EXT_INT_1; nothing on EXT_INT_2, on either board |
| 3 | Display Hub | KC-01-1722 for A1 / B1 | Retired; the 7" display driver is the hub (confirmed) |
| 4 | Safe switch on the 16-pin module slot | Hardware Reference §12: INT_SAFE on the slot | Slot pin 16 not connected; the safe switch has its own screw terminal. The "*Safe Switch Connection" notes beside slot P1 (Panel Hub) and P2 (display driver) are left over |
| 5 | Safe switch terminal V_EN | — | Pins 2, 3, 5 tied to a V_EN net that nothing drives |
| 6 | Fan headers | — | Pin order 1 PWM, 2 tach / NC, 3 12 V, 4 GND: the reverse of the usual PC fan numbering. Worth checking against the fans' cable |
| 7 | Throttle Module PCB number | Module UI Reference and the Hardware Reference change log: KC-01-1822 | KC-01-1872 (schematic 1871), as the Hardware Reference table says |
| 8 | Throttle panel inputs | Module UI Reference: four buttons | Five terminals; the fifth (THRTL_ENA) is unused by the firmware |

## 7. Panels

### 7.1 A1 — left display panel

| Item | Board | Connects to | Master sees it on |
|---|---|---|---|
| Information Display 1 (0x12) — **panel hub** | 7" TFT Display Driver (1911) | Its CN1 (34-pin) → Ctrl Module CN4 | INT_12 (its own line) |
| Annunciator (0x10) | 7" TFT Display Driver (1911) | Its P1 (16-pin) → Information Display 1's slot P2 | INT_9 |
| Power button | — | Ctrl Module H1 (power switch header, soft-latch circuit) | Nothing: the latch is analog and has no Teensy pin |

- **Free:** Information Display 1's slots P3 (INT_10) and P4 (INT_11).
- **Safe switch:** Information Display 1's terminal is empty, so INT_SAFE2 is unused.
- **The Annunciator's own terminal** goes nowhere. A driver plugged in through its 16-pin P1 has no safe-switch path, because P1 pin 16 is not connected.
- **No power-off from the master.** The master cannot switch the controller off or see the power button.

### 7.2 A2 — left input panel (Panel Hub on Ctrl Module CN3)

| Slot | Line | Module | Board | Address |
|---|---|---|---|---|
| P1 | INT_1 | Action Control | Button (1801) | 0x22 |
| P2 | INT_2 | Sys Info Display | 2.0" IPS carrier (1901) | 0x14 |
| P3 | INT_3 | Throttle | Throttle (1871) | 0x2C |
| P4 | INT_4 | UI Control | Button (1801) | 0x20 |
| P5 | INT_5 | — empty — | | |
| P6 | INT_6 | GPWS | 7-Segment (1841) | 0x2A |
| P7 | INT_7 | Translation joystick | Joystick (1831) | 0x29 |
| P8 | INT_8 | Function Control | Wide Button (1811) | 0x21 |

| Panel item | Wired to | Master sees it on |
|---|---|---|
| Abort button | Panel Hub safe-switch terminal | INT_SAFE (Teensy 31), active-high |
| Switch Group 1 (8 switches) | Function Control terminals U16 / U17 → B16–B23 | Function Control's packet |
| Translation stick | Joystick terminal U6 | Translation's packet |

**Unused:** Action Control and UI Control terminals (B12–B15); Throttle THRTL_ENA.

**To confirm:** Throttle THRTL_100 / UP / DOWN / 00 (terminals U7, U8, U10, U11).

To be filled in next: B1, B2.
