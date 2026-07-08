| Supported Targets | WT9932P4C61-TINY |
| ----------------- | ---------------- |

# Comprehensive Factory Test Example (Factory Firmware)

This factory firmware integrates Wireless-Tag BSP peripherals including MIPI DSI display, MIPI CSI camera, touch, SD card, RGB LED control, and Wi-Fi provisioning through the onboard ESP32-C61.

## Getting Started

1.  **Select the development board**: Run `idf.py set-board` in the example directory and select a supported development board. The command generates the board-level default configuration used by the next configure/build step:

```shell
~/WT_BSP/examples/wt_factory/wt9932p4c61-tiny$ idf.py set-board
...
Supported boards in this example:
0: WT9932P4C61-TINY (esp32p4)

Please select the target board by entering the corresponding number.
Enter board number:
```

Enter the number that matches your hardware kit model, then press Enter. A successful selection prints output similar to this:

```shell
Enter board number: 0
Generated sdkconfig.board
Generated sdkconfig.board.Kconfig
Updated build/sdkconfig
Selected WT9932P4C61-TINY (esp32p4)
```

Then run `idf.py build` to compile:

```shell
~/WT_BSP/examples/wt_factory/wt9932p4c61-tiny$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/wt_factory/wt9932p4c61-tiny/build
Executing "ninja all"...
...
```

After modifying the code, run `idf.py build` again to compile.

> During development, you can switch to another hardware kit whenever needed. When switching to a different target chip, `idf.py fullclean` is automatically executed.

## ESP32-C61 Slave Firmware

WT9932P4C61-TINY uses the onboard ESP32-C61 for wireless connectivity. Before running this ESP32-P4 firmware, flash the ESP32-C61 slave firmware when needed.

1. Connect FUSB and flash ESP32-P4 as the C61 bridge:

```bash
cd ./examples/wt_factory/wt9932p4c61-tiny
idf.py p4_flash
```

2. Unplug FUSB, connect HUSB, then build and flash the ESP32-C61 slave firmware:

```bash
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave set-target esp32c61
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave build
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave flash -p <HUSB_CDC_PORT>
```

3. Connect FUSB again and flash the ESP32-P4 factory firmware:

```bash
idf.py flash monitor
```

## Hardware Status Indication (RGB LED)

At startup, the system detects peripheral status and uses the onboard RGB LED to indicate hardware state. After Wi-Fi initialization starts, Wi-Fi state indication overrides the startup peripheral indication.

| LED Color | State |
|-----------|-------|
| Blue | Camera disconnected, or Wi-Fi scanning/connecting/reconnecting |
| Green | All normal, or Wi-Fi connected |
| Yellow | SD card disconnected |
| Pink | Screen disconnected |
| Red | All disconnected, or Wi-Fi disconnected/failed |
| Purple | Provisioning mode is active |

## Core Features

* **Camera Preview**: Displays the live camera feed.
* **LED Control**: Adjusts the onboard RGB LED color.
* **SD Card Status**: Tests SD card mounting and displays capacity information.
* **Wi-Fi Status**: Shows connection state, configuration AP name when disconnected, and system time. Long-press the onboard button for about one second to enter provisioning mode at `http://192.168.4.1`.

## Project Structure

* `main/main.c`: Main entry, BSP initialization, hardware detection, LVGL startup, and peripheral logic.
* `main/lvgl_ui.c`: UI implementation.
* `main/wifi_manager_bridge.cpp`: Wi-Fi connection and provisioning bridge.
* `managed_components/`: ESP-IDF managed component dependencies.
