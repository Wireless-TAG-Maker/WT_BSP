| Supported Targets | WT9932C2-TINY | WT9932C3-TINY | WT9932C5-TINY | WT9932C61-TINY | WT9932P4-TINY | WT9932P4C61-TINY |
| ----------------- | ------------- | ------------- | ------------- | -------------- | ------------- | ---------------- |

# Blink Example

## Getting Started

1.  **Select the development board**: Run `idf.py set-board` in the example directory and select a supported development board. The command generates the board-level default configuration used by the next configure/build step:

```shell
~/WT_BSP/examples/get-started/blink$ idf.py set-board
...
Supported boards in this example:
0: WT9932C2-TINY (esp32c2)
1: WT9932C3-TINY (esp32c3)
2: WT9932C5-TINY (esp32c5)
3: WT9932C61-TINY (esp32c61)
4: WT9932P4-TINY (esp32p4)
5: WT9932P4C61-TINY (esp32p4)

```

Enter the number that matches your hardware kit model, then press Enter:

```shell
~/WT_BSP/examples/get-started/blink$ idf.py set-board
...
...
Supported boards in this example:
0: WT9932C2-TINY (esp32c2)
1: WT9932C3-TINY (esp32c3)
2: WT9932C5-TINY (esp32c5)
3: WT9932C61-TINY (esp32c61)
4: WT9932P4-TINY (esp32p4)
5: WT9932P4C61-TINY (esp32p4)
4
Select board by number or name: Generated sdkconfig.board
Generated sdkconfig.board.Kconfig
Updated build/sdkconfig
Selected WT9932P4-TINY (esp32p4)
```

Then run `idf.py build` to compile:

```shell
~/WT_BSP/examples/get-started/blink$ idf.py set-board
...
...
Supported boards in this example:
0: WT9932C2-TINY (esp32c2)
1: WT9932C3-TINY (esp32c3)
2: WT9932C5-TINY (esp32c5)
3: WT9932C61-TINY (esp32c61)
4: WT9932P4-TINY (esp32p4)
5: WT9932P4C61-TINY (esp32p4)
5
Select board by number or name: Generated sdkconfig.board
Generated sdkconfig.board.Kconfig
Updated build/sdkconfig
Selected WT9932P4-TINY (esp32p4)
~/WT_BSP/examples/get-started/blink$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/get-started/blink/build
Executing "ninja all"...
[0/1] Re-running CMake...
...
...
```

After modifying the code, run `idf.py build` again to compile.

> During development, you can switch to another hardware kit whenever needed. When switching to a different hardware kit, `idf.py fullclean` is automatically executed.

## Example Output

After building and flashing the firmware, the RGB LED on the hardware stays on and randomly changes color once per second. The corresponding logs are also printed in the console. Example runtime logs:

```shell
I (1402) main_task: Calling app_main()
I (1402) blink: Initializing Wireless-Tag BSP
I (2422) blink: Hello Wireless-Tag BSP
I (3422) blink: Hello Wireless-Tag BSP
I (4422) blink: Hello Wireless-Tag BSP
I (5422) blink: Hello Wireless-Tag BSP
```
