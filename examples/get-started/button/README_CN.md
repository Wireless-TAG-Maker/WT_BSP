| Supported Targets | WT9932C2-TINY | WT9932C3-TINY | WT9932C5-TINY | WT9932C61-TINY | WT9932P4-TINY | WT9932P4C61-TINY |
| ----------------- | ------------- | ------------- | ------------- | ------------- | ------------- | ------------- |


# Button Example

## 🛠️ 快速上手

1.  **选择开发板**：在示例目录中运行 `idf.py set-board` 并选择支持的开发板，命令会生成下一次配置/构建使用的板级默认配置:

```shell
~/WT_BSP/examples/get-started/button$ idf.py  set-board
...
Supported boards in this example:
0: WT9932C2-TINY (esp32c2)
1: WT9932C3-TINY (esp32c3)
2: WT9932C5-TINY (esp32c5)
3: WT9932C61-TINY (esp32c61)
4: WT9932P4-TINY (esp32p4)
5: WT9932P4C61-TINY (esp32p4)

```

根据你的硬件套件型号，输入对应的数字，然后按下回车（Enter）按键：

```shell
~/WT_BSP/examples/get-started/button$ idf.py  set-board
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


然后执行 `idf.py build`,进行编译：


```shell
~/WT_BSP/examples/get-started/button$ idf.py  set-board
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
~/WT_BSP/examples/get-started/button$ idf.py   build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/get-started/button/build
Executing "ninja all"...
[0/1] Re-running CMake...
...
...
```

之后修改了代码再次执行`idf.py build`进行编译。


> 开发过程中可以随时根据需要切换到不同的硬件套件，切换到不同的硬件套件时会自动执行 `idf.py fullclean`。


## 示例效果

编译烧录固件后，每次操作BOOT按键都会有对应状态的LOG输出。下面是运行LOG：

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


