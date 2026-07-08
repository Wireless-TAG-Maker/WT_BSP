| Supported Targets | WT9932C2-TINY | WT9932C3-TINY | WT9932C5-TINY | WT9932C61-TINY | WT9932P4-TINY | WT9932P4C61-TINY |
| ----------------- | ------------- | ------------- | ------------- | -------------- | ------------- | ---------------- |

# Button Example

## Getting Started

1.  **Select the development board**: Run `idf.py set-board` in the example directory and select a supported development board. The command generates the board-level default configuration used by the next configure/build step:

```shell
~/WT_BSP/examples/get-started/button$ idf.py set-board
...
Supported boards in this example:
0: WT9932C2-TINY (esp32c2)
1: WT9932C3-TINY (esp32c3)
2: WT9932C5-TINY (esp32c5)
3: WT9932C61-TINY (esp32c61)
4: WT9932P4-TINY (esp32p4)
5: WT9932P4C61-TINY (esp32p4)

Please select the target board by entering the corresponding number.
Enter board number:
```

Enter the number that matches your hardware kit model, then press Enter:

```shell
~/WT_BSP/examples/get-started/button$ idf.py set-board
...
...
Supported boards in this example:
0: WT9932C2-TINY (esp32c2)
1: WT9932C3-TINY (esp32c3)
2: WT9932C5-TINY (esp32c5)
3: WT9932C61-TINY (esp32c61)
4: WT9932P4-TINY (esp32p4)
5: WT9932P4C61-TINY (esp32p4)

Please select the target board by entering the corresponding number.
Enter board number: 
4
Generated sdkconfig.board
Generated sdkconfig.board.Kconfig
Updated build/sdkconfig
Selected WT9932P4-TINY (esp32p4)
```

Then run `idf.py build` to compile:

```shell
~/WT_BSP/examples/get-started/button$ idf.py set-board
...
...
Supported boards in this example:
0: WT9932C2-TINY (esp32c2)
1: WT9932C3-TINY (esp32c3)
2: WT9932C5-TINY (esp32c5)
3: WT9932C61-TINY (esp32c61)
4: WT9932P4-TINY (esp32p4)
5: WT9932P4C61-TINY (esp32p4)

Please select the target board by entering the corresponding number.
Enter board number: 
5
Generated sdkconfig.board
Generated sdkconfig.board.Kconfig
Updated build/sdkconfig
Selected WT9932P4C61-TINY (esp32p4)
~/WT_BSP/examples/get-started/button$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/get-started/button/build
Executing "ninja all"...
[0/1] Re-running CMake...
...
...
```

After modifying the code, run `idf.py build` again to compile.

> During development, you can switch to another hardware kit whenever needed. When switching to a different hardware kit, `idf.py fullclean` is automatically executed.

## Example Output

After compiling and burning the firmware, there will be a corresponding LOG output for each operation of the BOOT button. Here is how to run the log:

```shell
I (1489) main_task: Calling app_main()
I (1489) button: Initializing Wireless-Tag BSP
I (1499) button: IoT Button Version: 4.2.0
W (1499) button: Please operate the button
I (6009) button: Button click
I (7009) button: Button press
I (7509) button: Button press
I (8009) button: Button click
I (9009) button: Button press
I (10009) button: Button long press
I (10509) button: Button long press
I (11009) button: Button long press
I (11509) button: Button long press
I (12009) button: Button long press
I (12509) button: Button long press
I (13009) button: Button release
```
