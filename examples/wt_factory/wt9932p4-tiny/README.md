| Supported Targets | WT9932P4-TINY |
| ----------------- | ------------- |

# Comprehensive Factory Test Example (Factory Firmware)

This factory firmware integrates Wireless-Tag BSP peripherals including MIPI DSI display, MIPI CSI camera, touch, SD card, and RGB LED control.

## Getting Started

1.  **Select the development board**: Run `idf.py set-board` in the example directory and select a supported development board. The command generates the board-level default configuration used by the next configure/build step:

```shell
~/WT_BSP/examples/wt_factory/wt9932p4-tiny$ idf.py set-board
...
Supported boards in this example:
0: WT9932P4-TINY (esp32p4)

Please select the target board by entering the corresponding number.
Enter board number:
```

Enter the number that matches your hardware kit model, then press Enter. A successful selection prints output similar to this:

```shell
Enter board number: 0
Generated sdkconfig.board
Generated sdkconfig.board.Kconfig
Updated build/sdkconfig
Selected WT9932P4-TINY (esp32p4)
```

Then run `idf.py build` to compile:

```shell
~/WT_BSP/examples/wt_factory/wt9932p4-tiny$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/wt_factory/wt9932p4-tiny/build
Executing "ninja all"...
...
```

After modifying the code, run `idf.py build` again to compile.

> During development, you can switch to another hardware kit whenever needed. When switching to a different target chip, `idf.py fullclean` is automatically executed.

## Hardware Status Indication (RGB LED)

At startup, the system detects peripheral status and uses the onboard RGB LED to indicate hardware state:

| LED Color | Hardware Status | Description |
|-----------|-----------------|-------------|
| Off | All normal | Screen, camera, and SD card are connected and initialized |
| Blue | Camera disconnected | |
| Yellow | SD card disconnected | |
| Pink | Screen disconnected | |
| Red | All disconnected | Screen, camera, and SD card are disconnected |

## Core Features

* **Camera Preview**: Displays the live camera feed. Tapping the preview toggles full-screen and normal preview modes.
* **LED Control**: Adjusts the onboard RGB LED color.
* **SD Card Status**: Tests SD card mounting and displays capacity information.
* **Touch Control**: Uses the touch panel paired with the DSI screen.

## Project Structure

* `main/main.c`: Main entry, BSP initialization, hardware detection, LVGL startup, and peripheral logic.
* `main/lvgl_ui.c`: UI implementation.
* `managed_components/`: ESP-IDF managed component dependencies.
