| Supported Targets | WT9932P4-TINY | WT9932P4C61-TINY |
| ----------------- | ------------- | ---------------- |

# Comprehensive Factory Test Example (Factory Firmware)

This example is a comprehensive factory firmware that integrates various peripheral features provided by the Wireless-Tag BSP, including:
* **MIPI DSI LCD Display**: Uses LVGL 9.4.0 to render the graphical user interface.
* **MIPI CSI Camera Capture**: Captures the real-time camera feed and displays it on the screen via PPA hardware acceleration.
* **Touch Control**: Supports the touch functionality associated with the DSI screen.
* **SD Card Test**: Supports mounting and capacity checking of SD cards using the SDMMC interface.
* **RGB LED Control**: Supports control of the onboard WS2812 RGB LED.

## Hardware Status Indication (RGB LED)

Upon startup, the system automatically detects the connection status of peripherals and uses the onboard RGB LED to display different colors indicating the hardware status:

| LED Color | Hardware Status | Description |
|-----------|-----------------|-------------|
| ⚫ Off | All Normal | Screen, camera, and SD card are correctly connected and successfully initialized |
| 🔵 Blue | Camera Disconnected | |
| 🟡 Yellow | SD Card Disconnected | |
| 🟠 Pink | Screen Disconnected | |
| 🔴 Red | All Disconnected | Screen, camera, and SD card are all disconnected |

**RGB Color Reference Values:**
- Blue: R=0, G=0, B=255
- Yellow: R=255, G=255, B=0
- Orange: R=255, G=165, B=0
- Red: R=255, G=0, B=0

## How to Use the Example

### Hardware Requirements

Currently supported development boards:
* **WT9932P4-TINY** (Equipped with a 480x640 MIPI DSI screen and an SC2336 MIPI CSI camera)
* **WT9932P4C61-TINY** (Equipped with a 480x640 MIPI DSI screen and an SC2336 MIPI CSI camera)

### Configure the Project

Before building, you need to set the target chip to `esp32p4`:

```bash
idf.py set-target esp32p4
```

This example already includes a default `sdkconfig.defaults`, which will automatically configure the relevant parameters for PSRAM, DSI, CSI, etc.

### Build and Flash

Build the project and flash it to the development board:

```bash
idf.py build flash monitor
```

## Core Features Explanation

* **Camera Preview**: The top area displays the real-time camera feed. Tapping the feed toggles between full-screen preview and normal preview modes. In full