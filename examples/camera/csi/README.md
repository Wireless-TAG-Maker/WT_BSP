| Supported Targets | WT9932P4-TINY | WT9932P4C61-TINY |
| ----------------- | ------------- | ---------------- |

# CSI Camera Example

This example uses the Wireless-Tag BSP MIPI CSI interface to capture camera frames and displays the live image on the MIPI DSI LCD through the PPA hardware accelerator.

## Getting Started

1.  **Select the development board**: Run `idf.py set-board` in the example directory and select a supported development board. The command generates the board-level default configuration used by the next configure/build step:

```shell
~/WT_BSP/examples/camera/csi$ idf.py set-board
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
~/WT_BSP/examples/camera/csi$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/camera/csi/build
Executing "ninja all"...
...
```

After modifying the code, run `idf.py build` again to compile.

> During development, you can switch to another hardware kit whenever needed. When switching to a different target chip, `idf.py fullclean` is automatically executed.

## Hardware Requirements

Currently supported development boards:

* **WT9932P4-TINY** with a MIPI DSI display and an SC2336 MIPI CSI camera.
* **WT9932P4C61-TINY** with a MIPI DSI display and an SC2336 MIPI CSI camera.

Make sure both the MIPI DSI display and MIPI CSI camera are connected correctly. IO0 is connected to the camera PWDN/LDO/RESET control path on this hardware, and the BSP drives it high before camera detection.

## Example Output

When the example runs correctly, the LCD shows the live camera preview. The serial monitor prints logs similar to this:

```text
I (324) csi_example: Initializing Wireless-Tag BSP
I (324) csi_example: Setting up DSI display
I (324) csi_example: Starting CSI camera stream
I (324) wt_bsp_csi: Video Stream Start
I (324) csi_example: System ready. Camera feed should be visible on the screen.
```

## Core APIs

* `wt_bsp_init()`: Initializes all enabled board peripherals, including DSI and CSI.
* `wt_bsp_get_csi()`: Gets the CSI camera object handle.
* `wt_bsp_csi_start()`: Starts camera video capture and registers the frame callback.
* `wt_bsp_dsi_get_panel_handle()`: Gets the underlying display panel handle for drawing frames.
