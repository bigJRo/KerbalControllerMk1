# KerbalControllerMk1

Code base for the **Kerbal Controller Mk1** — a modular hardware flight controller
for *Kerbal Space Program*, built around Teensy microcontrollers that talk to KSP
via the [KerbalSimpit](https://github.com/Simpit-team/KerbalSimpitRevamped) plugin.
The system is a set of I2C-networked panels and input modules coordinated by a
central master controller.

## Repository layout

| Path | Contents |
|------|----------|
| `Software/Controller_Main/` | Master controller firmware — I2C bus master, coordinates all panels and modules |
| `Software/Displays/` | Display-panel firmware and shared display libraries (see below) |
| `Software/Modules/` | Input-module firmware (buttons, switches, joysticks, encoders, 7-segment, etc.) |
| `Software/Libraries/`, `Software/Common/` | Shared cross-project code |
| `Documents/Developer/` | Hardware reference, I2C protocol spec, power budget, module/UI reference, ascent-autopilot interface |
| `Documents/User/` | Quick-start and panel operating guides |

### Display panels (`Software/Displays/`)

The display subsystem targets **hardware rev 2**: Teensy 4.1 driving a 7″
1024×600 RA8876 TFT over a 16-bit 8080 parallel bus, with FT5316 capacitive touch.
(Porting status and hardware details are in
[`Software/Displays/PORTING_7inch_TFT.md`](Software/Displays/PORTING_7inch_TFT.md)
and [`Documents/Developer/Hardware_Reference.md`](Documents/Developer/Hardware_Reference.md).)

| Sketch / library | Role |
|------------------|------|
| `KCMk1_Annunciator/` | Caution & Warning annunciator panel (I2C 0x10) |
| `KCMk1_InfoDisp/` | Flight information display — 13 telemetry screens (I2C 0x12) |
| `KCMk1_ResourceDisp/` | Resource display (I2C 0x11) — *rev-1, port pending* |
| `libraries/KerbalDisplayCommon/` | Shared UI toolkit — drawing, fonts, formatting, threshold colouring |
| `libraries/KCM_Display`, `KCM_Touch`, `KCMk1_SystemConfig` | Display driver wrapper, touch driver, shared pin map + thresholds |
| `libraries/KerbalDisplayAudio/` | Master-alarm buzzer + DFPlayer audio |

Each panel sketch has its own README with per-panel wiring, dependencies, and
screen documentation.

## License

Licensed under the GNU General Public License v3.0 (GPL-3.0).
Written by J. Rostoker for Jeb's Controller Works.
