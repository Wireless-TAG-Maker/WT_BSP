| Supported Targets | WT9932P4C61-TINY |
| ----------------- | ---------------- |

# Comprehensive Factory Test Example (Factory Firmware)

This example is a comprehensive factory firmware that integrates various peripheral features provided by the Wireless-Tag BSP, including:
* **MIPI DSI LCD Display**: Uses LVGL 9.4.0 to render the graphical user interface.
* **MIPI CSI Camera Capture**: Captures the real-time camera feed and displays it on the screen via PPA hardware acceleration.
* **Touch Control**: Supports the touch functionality associated with the DSI screen.
* **SD Card Test**: Supports mounting and capacity checking of SD cards using the SDMMC interface.
* **RGB LED Control**: Supports control of the onboard WS2812 RGB LED.
* **Wi-Fi Provisioning**: Displays the connection state and system time, and supports entering provisioning mode with the onboard button.

## Hardware Status Indication (RGB LED)

Upon startup, the system automatically detects the connection status of peripherals and uses the onboard RGB LED to display different colors indicating the hardware status:

| LED Color | Hardware Status | Description |
|-----------|-----------------|-------------|
| 🟢 Green | All Normal / Wi-Fi Connected | Peripherals initialized successfully or Wi-Fi connected |
| 🔵 Blue | Camera Disconnected | |
| 🟡 Yellow | SD Card Disconnected | |
| 🟠 Pink | Screen Disconnected | |
| 🔴 Red | All Disconnected | Screen, camera, and SD card are all disconnected |

After Wi-Fi initialization starts, the RGB LED switches to indicating the current Wi-Fi state:

| LED Color | Wi-Fi State |
|-----------|------------|
| 🔵 Blue | Scanning, connecting, or reconnecting |
| 🟢 Green | Connected to a Wi-Fi access point |
| 🔴 Red | Disconnected or connection failed |
| 🟣 Purple | Provisioning mode is active |

The Wi-Fi state indication overrides the startup peripheral indication.

**RGB Color Reference Values:**
- Blue: R=0, G=0, B=255
- Green: R=0, G=255, B=0
- Yellow: R=255, G=255, B=0
- Orange: R=255, G=165, B=0
- Red: R=255, G=0, B=0
- Purple: R=255, G=0, B=255

## How to Use the Example

### Hardware Requirements

Currently supported development boards:
* **WT9932P4-TINY** (Equipped with a 480x640 MIPI DSI screen and an SC2336 MIPI CSI camera)
* **WT9932P4C61-TINY** (Equipped with a 480x640 MIPI DSI screen and an SC2336 MIPI CSI camera)

### Build and Flash the ESP32-C61 Slave Firmware

First, build and flash the ESP-Hosted slave firmware for the ESP32-C61:

```bash
cd ./examples/wt_factory/wt9932p4c61-tiny

# Set the target chip
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave set-target esp32c61

# Build the firmware
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave build

# Flash the ESP32-C61 (replace COM1 with the actual serial port)
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave flash -p COM1

# Optional: monitor the ESP32-C61 output
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave monitor -p COM1
```

**About the `build_slave` directory:**

- `build_slave` is the independent build directory for the slave firmware, separate from the main project's `build` directory.
- The directory is generated in the project root and can be added to `.gitignore`.
- Delete the `build_slave` directory when a clean build is required.

**Entering download mode:**

Make sure the ESP32-C61 is in download mode before flashing:

1. Connect the ESP32-C61 RX and TX pins to a USB-to-UART adapter.
2. Hold ESP32-C61 BOOT (GPIO9) low.
3. Pull ESP32-C61 EN (RESET) low, then release it to reset the chip.
4. Release ESP32-C61 BOOT (GPIO9).

### Configure the Project

Before building, you need to set the target board:

```bash
idf.py set-board
```

choice `WT9932P4C61-TINY (esp32p4)`:

```bash
Supported boards in this example:
0: WT9932P4C61-TINY (esp32p4)
```

This example already includes a default `sdkconfig.defaults`、`sdkconfig.wt9932p4c61_tiny`, which will automatically configure the relevant parameters for PSRAM, DSI, CSI, etc.

### Build and Flash

Build the project and flash it to the development board:

```bash
idf.py build flash monitor
```

## Core Features Explanation

* **Camera Preview**: The top area displays the real-time camera feed. Tapping the feed toggles between full-screen preview and normal preview modes.
* **LED Control**: The bottom-left sliders adjust the color of the onboard RGB LED.
* **SD Card Status**: The upper half of the bottom-right area tests SD card mounting and displays capacity information.
* **Wi-Fi Status**: The lower half of the bottom-right area shows the connection state, configuration AP name when disconnected, and system time. Long-press the onboard button for about one second to enter provisioning mode at `http://192.168.4.1`.
