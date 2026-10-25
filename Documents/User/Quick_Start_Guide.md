# KCMk1 Quick Start Guide

**Document type:** User  
**Location:** `Documents/User/Quick_Start_Guide.md`  
**Version:** 0.2 (partial)  
**Status:** Sections 3, 4 and 7 written for the display panels; the rest to be completed when the hardware build is finalized

---

## Contents

1. What's in the box *(planned)*
2. Physical installation *(planned)*
   - Panel mounting
   - Cable connections
   - Power supply connection
3. [KSP mod requirements](#3-ksp-mod-requirements)
4. [First power-on](#4-first-power-on)
5. Connecting to KSP *(planned)*
   - Simpit Settings.cfg configuration
   - COM port assignment
   - Verifying connection
6. Basic operation check *(planned)*
7. [Troubleshooting](#7-troubleshooting)

---

## 3. KSP mod requirements

The controller talks to KSP through the **KerbalSimpit** plugin. Several display readouts
also depend on other mods; without them those readouts read zero or stay dark rather than
showing a false alarm.

| Mod | What needs it |
|---|---|
| **KerbalSimpit** (2.4.0) | Everything: it is the telemetry link for every panel |
| **AGX** (Action Groups Extended) | Every custom action group the controller fires (the controller uses AGX groups 1–238 across its six control groups) |
| **Alternate Resource Panel (ARP)** | Most Resource Display channels; the Annunciator's BUS VOLTAGE tile; the electric-charge readouts on the Info Displays; the "no propellant" handling of the stage Δv readouts and LOW ΔV |
| **TAC Life Support** | The Resource Display's Oxygen, Food, Water, CO2, Waste and Liquid Waste meters; the Annunciator's LIFE SUPPORT tile |
| **Community Resource Pack (CRP)** | The Resource Display's Liquid Hydrogen, Liquid Methane, Lithium, Intake Air, Enriched Uranium, Depleted Fuel, Fertilizer and Stored Charge meters. These also need two blocks added to KerbalSimpit's `Settings.cfg`; see the Resource Display README |
| **Hullcam VDS Continued** | The camera controls |

A tile or meter that needs a missing mod never false-triggers: BUS VOLTAGE simply never
lights, LIFE SUPPORT stays dark, and a Resource Display meter for a resource the game never
reports is treated as not aboard.

## 4. First power-on

### Boot

Each display panel runs its own boot sequence, then waits for the master controller to
release it. The header of every boot screen shows that panel's firmware version, which is
how you tell the separately flashed boards apart.

| Panel | Boot sequence |
|---|---|
| Annunciator | A terminal-style BIOS power-on self test |
| Information Displays | One of three KSP-themed sequences, chosen at random: Jeb's Mission Log, KSP Loading Tips or Gene's Pre-Flight Checklist |
| Resource Display | A Jurassic Park–style terminal |

A panel that stays on its boot screen has not been released by the master; see
[Troubleshooting](#7-troubleshooting).

### Standby

Outside a flight scene, and until a panel's own KerbalSimpit link to KSP is up, every
display shows a full-screen standby splash. It has no live data and needs no action; the
panel moves to its flight screen by itself when a flight starts. A panel that boots into
a flight already in progress picks it up from the first telemetry it receives.

### Touch

All the display panels filter touches the same way, which matters when a tap seems not
to register:

- **One finger only.** A touch that reports two or more contacts is ignored, so a second
  finger resting on the glass makes the panel unresponsive.
- **Press cleanly.** Each tap is read twice, 8 ms apart, and rejected if it moved more
  than 20 px in between, so a sliding touch is ignored.
- **One tap at a time.** Taps are accepted at most every 500 ms, and the finger must lift
  before the next one counts. On the Info Displays a repeat press on the same sidebar key
  is accepted after 150 ms, so cycling through a key's modes stays quick.
- **After a reset** the panel must be seen untouched once before it accepts a tap, so a
  finger already on the glass at power-on does nothing.
- On the Resource Display, a **held** touch on a meter (1 s) sets a reserve bug, where a
  tap opens the Detail screen.

**Most of the glass is not a control.** On the Info Displays the touch targets are the
sidebar keys, the reference chip on the two flight displays, the PRE-LAUNCH board (a tap
dismisses it) and the autopilot consoles. The RCS, SAS, GEAR, BRAKES and AIRBRK tiles on
the flight screens are indicators, not buttons: fly the vessel from the physical panels
and read it from the glass.

## 7. Troubleshooting

| Symptom | Likely cause |
|---|---|
| A display stays on its boot screen | The master has not released it (it sends PROCEED with every command). Check the I2C loom to the master and that the master is running |
| A display shows standby during a flight | That panel's KerbalSimpit link to KSP is not up. Check its USB connection and the Simpit plugin |
| BUS VOLTAGE never lights | Needs the Alternate Resource Panel mod |
| LIFE SUPPORT never lights | Needs TAC Life Support, and a crew aboard |
| A Resource Display meter is missing | The resource is not aboard, or the mod that supplies it is not installed (it collapses rather than reading zero) |
| No sound from the Annunciator | Its AUDIO tile is dark: audio is off |
| Tones work but no GPWS voice | The GPWS panel is OFF, or the DFPlayer card is missing its clips |
| A tap is ignored | Two fingers on the glass, a sliding touch, or a second tap within 500 ms |
| The Annunciator's control-mode tile is red | The control-mode switch does not suit the vessel type, so the sticks are mapped for another vehicle |

Each panel's own reference covers its screens in detail:
[Annunciator](Annunciator_Caution_Warning_Reference.md),
[Information Displays](Info_Display_Screen_Reference.md),
[Resource Display](Resource_Display_Reference.md).
