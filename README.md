# Wireless-Tag Board Support Package (WT_BSP)

[English](./README.md) | [中文](./README_CN.md)

The **WT_BSP** is a comprehensive Board Support Package (BSP) designed for Wireless-Tag (启明云端) development boards based on Espressif's ESP32 series MCUs. It provides a unified, hardware-agnostic API for common peripherals, enabling developers to switch between different boards with minimal code changes.

This BSP is optimized for **ESP-IDF v6.0.1 or later** and targets a wide range of chips, from the cost-effective ESP32-C2 to the high-performance ESP32-P4.

## 🚀 Key Features

-   **Unified Hardware API**: Access RGB LEDs, buttons, SD cards, displays, and cameras through a consistent set of `wt_bsp_*` functions.
-   **Modular Architecture**: Each peripheral is implemented as a standalone module within the BSP, allowing for easy extension and maintenance.
-   **Multi-Board Support**: Native support for various "TINY" series boards selected with `idf.py set-board`.
-   **Advanced ESP32-P4 Support**:
    -   **MIPI-DSI**: High-resolution display support with LVGL integration.
    -   **MIPI-CSI**: Camera support with hardware acceleration (PPA).
    -   **P4C61 Bridge Tool**: `idf.py p4_flash` flashes the WT9932P4C61-TINY ESP32-P4 into a USB-UART bridge for quickly flashing and playing with the onboard ESP32-C61.
-   **Rich Examples**: From simple "Blink" to complex "Factory Test" firmware integrating UI and camera streaming.

## 📋 Supported Boards

| Board Name | MCU | Key Features |
| :--- | :--- | :--- |
| **WT9932C2-TINY** | ESP32-C2 | RGB LED, Button |
| **WT9932C3-TINY** | ESP32-C3 | RGB LED, Button |
| **WT9932C5-TINY** | ESP32-C5 | RGB LED, Button |
| **WT9932C61-TINY** | ESP32-C6 | RGB LED, Button |
| **WT9932P4-TINY** | ESP32-P4 | MIPI-DSI, MIPI-CSI, SDMMC, Touch, RGB, Button |
| **WT9932P4C61-TINY** | ESP32-P4 + ESP32-C61 | Same as P4-TINY, adds ESP32-C61 for low-power/wireless tasks |

For WT9932P4C61-TINY, connect both FUSB (Full-Speed USB) and HUSB (High-Speed USB). Run `idf.py p4_flash` in any WT_BSP example to flash the ESP32-P4 bridge firmware through FUSB, then flash ESP32-C61 firmware through the HUSB TinyUSB CDC port.

## 🛠️ Getting Started

### Prerequisites
-   **ESP-IDF**: Version `v6.0.1` or later is recommended. (Compatible with `v5.3` and above).([Get esp-idf](https://developer.espressif.com/tags/esp-idf/))
-   **Git**: To clone the repository and submodules.

### Installation
1.  Clone this repository into your project's `components` directory or add it as a submodule.
2.  In your project's `idf_component.yml`, ensure dependencies are met.

### Basic Usage
1.  **Select your board**: Run `idf.py set-board` in an example directory and choose a supported board. The command generates board defaults used by the next configure/build, and automatically runs `idf.py fullclean` when switching to a board with a different target chip. After selecting the development board, run `idf.py build` to compile.

2.  **Initialize the BSP**:
    ```c
    #include "wt_bsp.h"

    void app_main(void) {
        /* Initialize all board resources */
        ESP_ERROR_CHECK(wt_bsp_init());

        /* Access an RGB LED */
        wt_bsp_rgb_t rgb = wt_bsp_get_rgb();
        if (rgb) {
            wt_bsp_rgb_set_color(rgb, (wt_bsp_rgb_color_t){.r = 255, .g = 0, .b = 0});
        }
    }
    ```

## 📂 Project Structure

-   `components/wt_bsp`: The core BSP component.
    -   `boards`: Hardware-specific configurations, Kconfig, and board resource lifecycles.
    -   `features`: Reusable peripheral implementations such as RGB, button, SDMMC, DSI, CSI, and touch.
    -   `include`: Public API headers.
    -   `src`: Implementation of the common BSP interface.
    -   `tools`: `set-board` and project CMake helper scripts.
-   `examples`: Usage demonstrations for various features.

## ⚙️ How it Works

The WT_BSP uses a **Board Interface Pattern** to decouple application logic from hardware specifics:
1.  **Interface Definition**: `wt_bsp_interface_t` defines a table of function pointers for all board resources.
2.  **Board Implementation**: Each board in `components/wt_bsp/boards/` implements these functions and registers them.
3.  **Unified Access**: The application calls `wt_bsp_init()`, which automatically selects the correct board implementation from the Kconfig values written by `set-board`, and subsequently uses generic `wt_bsp_get_*()` functions.

## 🤝 Contributing
Contributions are welcome! Please follow the existing coding style and ensure all changes are tested against supported hardware.
