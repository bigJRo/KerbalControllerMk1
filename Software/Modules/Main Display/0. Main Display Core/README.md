# KerbalDisplayCore – Kerbal Controller Mk1 Display Library

This is the core firmware library for **Display Modules** used in the **Kerbal Controller Mk1** system.  
It provides foundational support for 5" RA8875-based TFT displays with capacitive touch, USB control, and I2C communication.

---

## 📦 Overview

- **Library Name**: KerbalDisplayCore
- **MCU Target**: Teensy 4.0
- **Display Driver**: RA8875_t4 (800x480 resolution)
- **Touch Controller**: GSLX680 (I2C)
- **Communication**: I2C (slave device)
- **Special Functions**:
  - Touch firmware loading on startup
  - I2C USB connect/disconnect
  - Soft reboot capability
  - Optional debug mode

---

## 🚀 Features

- 🟢 **Modular Initialization**: Call `beginModule(panel_addr)` to configure display, touch, and I2C functions.
- 🖼️ **RA8875 Display Support**: Automatically initializes resolution, rotation, and clears screen.
- ✍️ **Touchscreen Input**: Reads up to 5 points of touch data using GSLX680 and firmware blob.
- ⚡ **Interrupt-Driven**: Supports host notification over digital interrupt line.
- 🔌 **USB Reconnect/Disconnect**: Controlled via I2C commands for dynamic USB behavior.
- 🔁 **Soft Reboot**: Allows full MCU restart on demand.

---

## 🧰 Hardware Pin Mapping

| Signal           | Teensy 4.0 Pin | Macro        | Description                          |
|------------------|----------------|--------------|--------------------------------------|
| TFT Chip Select  | 10             | `TFT_CS`     | SPI chip select for RA8875           |
| TFT Reset        | 15             | `TFT_RST`    | Display reset pin                    |
| TFT WAIT         | 14             | `TFT_WAIT`   | Optional wait/status pin             |
| Touch Wake       | 3              | `CTP_WAKE`   | Controls touch chip power state      |
| Touch Interrupt  | 22             | `CTP_INT`    | Touch interrupt line (optional use)  |
| Main I2C SDA     | 18             | `MAIN_SDA`   | Host communication                   |
| Main I2C SCL     | 19             | `MAIN_SCL`   | Host communication                   |
| Touch I2C SDA    | 17             | `CTP_SDA`    | Touchscreen I2C SDA                  |
| Touch I2C SCL    | 16             | `CTP_SCL`    | Touchscreen I2C SCL                  |
| Host Interrupt   | 2              | `INT_OUT`    | Signals host of activity             |

---

## 📡 I²C Protocol Description

The display module communicates over I²C as a **slave** device. It supports a 2-byte protocol to exchange control messages.

### 📥 Master Write (to display module)

| Byte | Description                     |
|------|---------------------------------|
| [0]  | Command ID                      |
| [1]  | Command Parameter / Signature   |

Supported Commands:
- `0x00` → Reset display (`0xAA`)
- `0x01` → USB Disconnect (`0xAB`)
- `0x02` → USB Connect (`0xAC`)
- `0x03` → Simpit Reset (`0xAD`)

---

## 🔧 API Summary

### `bool beginModule(panel_addr)`
Initializes display, touchscreen, I2C, and interrupt output. Returns `true` if successful.

### `setInterrupt()` / `clearInterrupt()`
Controls the INT line used to notify host of activity.

### `handleRequestEvent()` / `handleReceiveEvent(howMany)`
I2C handlers for status request and command reception.

### `GSLX680_read_data()`
Reads and decodes current touch data into `ts_event`.

---

## 📚 Example Usage

```cpp
#include <MainDisplayCore.h>

void setup() {
  if (!beginModule(0x23)) {
    Serial.println("Display failed to initialize");
    while (1);
  }
}

void loop() {
  // Touch processing and UI updates here
}
```

---

## 🧩 Dependencies

- [`RA8875_t4`](https://github.com/PaulStoffregen/RA8875)
- [`Wire`](https://www.arduino.cc/reference/en/language/functions/communication/wire/)
- [`SPI`](https://www.arduino.cc/reference/en/language/functions/communication/spi/)
- `touchScreen_fw.h` (GSLX680 firmware blob, included in `src/`)

---

## 📂 File Structure

- `MainDisplayCore.h` – Header file with pin mappings, globals, and function declarations
- `MainDisplayCore.cpp` – Implementation of display, touch, and communication logic
- `library.properties` – Arduino library metadata
- `examples/BasicTest/BasicTest.ino` – Simple usage demo

---

## 📄 License

This project is licensed under the GNU General Public License v3.0  
Final code written by J. Rostoker for **Jeb's Controller Works**
