| Supported Targets | WT9932P4-TINY | WT9932P4C61-TINY |
| ----------------- | ------------- | ---------------- |

# SDMMC Example

This example uses the Wireless-Tag BSP SDMMC interface to mount a MicroSD card and performs basic file write, rename, and read operations with standard C/POSIX APIs.

## Getting Started

1.  **Select the development board**: Run `idf.py set-board` in the example directory and select a supported development board. The command generates the board-level default configuration used by the next configure/build step:

```shell
~/WT_BSP/examples/storage/sdmmc$ idf.py set-board
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
~/WT_BSP/examples/storage/sdmmc$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/storage/sdmmc/build
Executing "ninja all"...
...
```

After modifying the code, run `idf.py build` again to compile.

> During development, you can switch to another hardware kit whenever needed. When switching to a different target chip, `idf.py fullclean` is automatically executed.

## Hardware Requirements

Currently supported development boards:

* **WT9932P4-TINY**.
* **WT9932P4C61-TINY**.

Insert a FAT-formatted MicroSD card before running the example. On WT9932P4 hardware, make sure the SD card power configuration required by the board is connected correctly.

## Example Output

When the example runs correctly, the serial monitor prints logs similar to this:

```text
I (314) sdmmc_example: Initializing BSP
I (314) wt_bsp_sdmmc: Mounting filesystem
I (354) wt_bsp_sdmmc: Filesystem mounted
I (364) sdmmc_example: SD card mounted at: /sdcard
I (374) sdmmc_example: Creating file: /sdcard/hello.txt
I (404) sdmmc_example: File written successfully
I (414) sdmmc_example: Renaming /sdcard/hello.txt to /sdcard/foo.txt
I (424) sdmmc_example: Reading file: /sdcard/foo.txt
I (424) sdmmc_example: Read from file: 'Hello SDMMC from WT_BSP!'
I (424) sdmmc_example: Example finished successfully
```

## Troubleshooting

* If mounting fails, check that the SD card is inserted correctly and formatted as FAT/FAT32.
* Check the board-specific SD card power and pin connections.
