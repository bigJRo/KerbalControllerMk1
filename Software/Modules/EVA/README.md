# EVA Module – Kerbal Controller Mk1 Device

This is the firmware for the **EVA Module** used in the **Kerbal Controller Mk1** system.  
It runs on an **Adafruit ItsyBitsy M4**, handles 6 RGB LED buttons, and displays telemetry on a **0.96" ST7789 round LCD** using **LVGL** and **LovyanGFX**.

---

## 📦 Overview

- **Module Name**: EVA Display Module
- **MCU Target**: Adafruit ItsyBitsy M4
- **Display**: 240x240 ST7789 Round IPS LCD
- **Communication**: I²C (slave device)
- **Inputs**:
  - 6 active-HIGH pushbuttons (debounced in software)
- **Outputs**:
  - 6 RGB LEDs (NeoPixels)
  - Dynamic telemetry gauge (fuel percentage)
  - Interrupt pin to host (active LOW)

---

## 🚀 Features

- 🎛 **Fuel Gauge Visualization**:
  - LVGL-rendered arc gauge from 0–100%
  - Red/yellow/white ticks and labels for clear segmentation
  - Live percentage label and title ("EVA Fuel")

- 💡 **NeoPixel LED Support**:
  - Each LED has 2-bit mode: OFF, DIM GRAY, GREEN
  - States controlled via bitfield sent from host

- 🔄 **I²C Communication**:
  - Host sends LED modes, power mode, and fuel percentage
  - Interrupt pin alerts host on button activity

- 💤 **Power Save Mode**:
  - Host can disable display and LEDs to conserve power

---

## 🎮 Button Mapping

| Button | Arduino Pin | Functionality    |
|--------|-------------|------------------|
| 1      | `7`         | EVA Lights       |
| 2      | `9`         | Board            |
| 3      | `10`        | Jump / Let Go    |
| 4      | `11`        | Jet Pack         |
| 5      | `12`        | Construction     |
| 6      | `13`        | Grab             |

- All buttons are **active HIGH**
- Events are latched and cleared once sent over I²C

---

## 💡 LED Color Configuration

| Mode Bits | Color     | RGB Value     |
|-----------|-----------|---------------|
| `00`      | OFF       | (0, 0, 0)     |
| `01`      | DIM GRAY  | (32, 32, 32)  |
| `10`      | GREEN     | (0, 128, 0)   |

- Each LED uses 2 bits in a 16-bit host-defined bitfield.
- The MSB (`0x8000`) is a **power save flag**; if set, LEDs and display are disabled.

---

## 📡 I²C Protocol

### 📥 Master Write → Device

Host sends **2 bytes**:

| Byte Index | Field             | Description                                |
|------------|------------------|--------------------------------------------|
| `[0-1]`    | `ledStates`       | 16-bit bitfield: 2 bits per LED (6 LEDs), MSB = power save |
| `[2]`      | `eva_fuel`        | 1 byte (0–100) fuel percentage             |

- On update, `updateDisplay` is flagged to refresh visuals.

### 📤 Master Read ← Device

Host receives **1 byte**:

| Byte | Description                   |
|------|-------------------------------|
| `[0]`| Bitmask of pressed buttons    |

- Each bit `0–5` corresponds to a button.
- Interrupt line is pulled LOW on press, then cleared after read.

---

## 📐 Display Rendering

- Arc gauge from **135° to -135°** sweep
- Tick marks every 2.5%, major ticks every 10%
- Colored segments:
  - Red: 0–10%
  - Yellow: 10–25%
  - White: 25–100%
- Label in **Futura Bold 28pt**
- Title ("EVA Fuel") centered bottom in **Futura Bold 20pt**

---

## 📚 Required Libraries

To compile and run the `eva.ino` firmware, ensure the following libraries are installed via the Arduino Library Manager:

| Library              | Purpose                                    | Source / Notes                    |
|----------------------|--------------------------------------------|-----------------------------------|
| `Wire`               | I²C communication                          | Built-in                          |
| `Adafruit NeoPixel`  | RGB LED control                            | Adafruit                          |
| `LovyanGFX`          | ST7789 display driver                      | LovyanGFX GitHub                  |
| `lvgl`               | GUI framework for embedded displays        | LittlevGL (via Library Manager)   |

Additionally, the following custom fonts must be included in the project directory:

| Font File            | Usage                          |
|----------------------|--------------------------------|
| `futura_12.c`        | Tick mark labels               |
| `futura_bold_20.c`   | EVA Fuel title label           |
| `futura_bold_28.c`   | Central fuel percentage label  |

---

## 🧰 Setup Instructions

1. Install the latest **Arduino IDE**
2. Add the **Adafruit SAMD board package**
3. Install required libraries (see 📚 above)
4. Include the custom font `.c` files in your project folder
5. Upload `eva.ino` to the ItsyBitsy M4 using USB

---

## 📂 File Structure

| File Name           | Description                             |
|---------------------|-----------------------------------------|
| `eva.ino`           | Main firmware sketch                    |
| `futura_12.c`       | Font for tick labels                    |
| `futura_bold_20.c`  | Font for title label                    |
| `futura_bold_28.c`  | Font for percentage label               |
| `README.md`         | This documentation                      |

---

## 📄 License

This project is licensed under the **GNU General Public License v3.0**  
Firmware written by **J. Rostoker** for **Jeb's Controller Works**
