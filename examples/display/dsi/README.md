| Supported Targets | WT9932P4-TINY | WT9932P4C61-TINY |
| ----------------- | ------------- | ---------------- |

# DSI Display Example

This example uses the Wireless-Tag BSP MIPI DSI interface to drive the LCD panel and starts LVGL for UI rendering.

## Getting Started

1.  **Select the development board**: Run `idf.py set-board` in the example directory and select a supported development board. The command generates the board-level default configuration used by the next configure/build step:

```shell
~/WT_BSP/examples/display/dsi$ idf.py set-board
...
Supported boards in this example:
0: WT9932P4-TINY (esp32p4)
1: WT9932P4C61-TINY (esp32p4)

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
~/WT_BSP/examples/display/dsi$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/display/dsi/build
Executing "ninja all"...
...
```

After modifying the code, run `idf.py build` again to compile.

> During development, you can switch to another hardware kit whenever needed. When switching to a different target chip, `idf.py fullclean` is automatically executed.

## Hardware Requirements

Currently supported development boards:

* **WT9932P4-TINY** with a MIPI DSI display.
* **WT9932P4C61-TINY** with a MIPI DSI display.

Make sure the MIPI DSI display is connected correctly and that the board has a stable power supply.

## Example Output

When the example runs correctly, the LCD lights up and shows the LVGL widgets demo. The serial monitor prints logs similar to this:

```text
I (324) dsi_example: Initializing BSP
I (324) dsi_example: Starting LVGL
I (324) dsi_example: Turning on display and setting brightness
I (324) dsi_example: DSI example ready
```

## Core APIs

* `wt_bsp_init()`: Initializes all enabled board peripherals, including DSI.
* `wt_bsp_get_dsi()`: Gets the DSI display object handle.
* `wt_bsp_dsi_lvgl_start()`: Starts the LVGL port and registers the display driver.
* `wt_bsp_dsi_lvgl_lock()` / `wt_bsp_dsi_lvgl_unlock()`: Protect LVGL API calls.
