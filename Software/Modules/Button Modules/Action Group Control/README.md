Action Group Control Module – Kerbal Controller Mk1

Version 1.0 (Reference)

This is the finalized firmware for the Action Group Control Module in the Kerbal Controller Mk1 system.
It handles action group button input, RGB LED feedback, and I²C communication with the host using the shared ButtonModuleCore v1.1 library.

📦 Overview

MCU: ATtiny816 (megaTinyCore)

Communication: I²C (slave address 0x29)

Inputs:

16 momentary buttons via shift registers (handled by ButtonModuleCore)

Outputs:

12 RGB NeoPixel LEDs (Action Group indicators)

Host Compatibility: Designed for Kerbal Simpit–based host firmware

Core Library: ButtonModuleCore v1.1 (baseline)

🚀 Features

Reads up to 16 button inputs via shared core logic

Controls 12 RGB NeoPixel LEDs for Action Group feedback

Clean separation of:

Button state tracking

I²C protocol handling

Module-specific LED behavior

Priority-based LED rendering using ButtonModuleCore v1.1 semantics

Automatic bulb test executed during startup

Optimized LED updates (only changed LEDs are refreshed)

🎛 Button Mapping
Index	Label	Function
0	AG1	Action Group 1
1	AG2	Action Group 2
2	AG3	Action Group 3
3	AG4	Action Group 4
4	AG5	Action Group 5
5	AG6	Action Group 6
6	AG7	Action Group 7
7	AG8	Action Group 8
8	AG9	Action Group 9
9	AG10	Action Group 10
10	AG11	Action Group 11
11	AG12	Action Group 12
12–13	—	Unused
14	SPC_MODE	Spacecraft Mode
15	RVR_MODE	Rover Mode
💡 LED Behavior (ButtonModuleCore v1.1)

Each LED follows a strict priority model:

led_bits = 1
→ LED displays its assigned color (GREEN for AG1–AG12)

Else if button_active_bits = 1
→ LED displays DIM_GRAY

Else
→ LED is OFF (BLACK)

LED Color Mapping
LED Index	Label	Assigned Color
0–11	AG1–AG12	GREEN
🔌 I²C Protocol Summary (v1.1)

This module operates as an I²C slave using the shared ButtonModuleCore protocol.

Host → Module (Control Words)

The host may send one or more 16-bit control words to:

Set or clear led_bits (full-brightness LED state)

Set or clear button_active_bits (DIM_GRAY LED state)

Initiate bulb test (core-defined command)

Control words are decoded entirely by ButtonModuleCore v1.1.

Module → Host (Request Event)

On an I²C request from the host, the module returns:

button_state

Current debounced button states

button_pressed

Rising-edge events since last request

Automatically cleared after transmission

button_released

Falling-edge events since last request

Automatically cleared after transmission

This guarantees deterministic edge reporting with no duplicate events.

🔧 Setup & Initialization

The firmware calls:

beginModule(panel_addr);


This performs:

I²C initialization

Button input setup

NeoPixel initialization

State reset

Bulb Test

Stepped LED test sequence

250 ms delay per step

🔁 Main Loop Behavior

The main loop:

Updates LEDs only when updateLED is asserted by the core

Continuously samples button states via readButtonStates()

Leaves all protocol handling to ButtonModuleCore

📂 Files

AG_Control.ino – Action Group Control Module firmware (v1.0 reference)

ButtonModuleCore – Shared core library (v1.1 baseline)

🔖 Versioning & Freeze Policy

Action Group Control Module: v1.0 (FROZEN)

ButtonModuleCore: v1.1 (baseline)

No functional changes should be made without:

A new module version number, or

A new major core version (v2.0+)

© 2025 Jeb’s Controller Works
Licensed under the GNU General Public License v3.0 (GPL-3.0)
