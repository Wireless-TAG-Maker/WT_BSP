| Supported Targets | WT9932P4-TINY | WT9932P4C61-TINY |
| ----------------- | ------------- | ---------------- |

# SDMMC 示例

本示例演示如何使用 Wireless-Tag BSP 提供的 SDMMC 接口挂载 MicroSD 卡，并使用标准 C/POSIX API 完成基础文件写入、重命名和读取操作。

## 🛠️ 快速上手

1.  **选择开发板**：在示例目录中运行 `idf.py set-board` 并选择支持的开发板，命令会生成下一次配置/构建使用的板级默认配置：

```shell
~/WT_BSP/examples/storage/sdmmc$ idf.py set-board
...
Supported boards in this example:
0: WT9932P4-TINY (esp32p4)
1: WT9932P4C61-TINY (esp32p4)

Please select the target board by entering the corresponding number.
Enter board number:
```

根据你的硬件套件型号输入对应数字，然后按下回车（Enter）按键。选择成功后会看到类似输出：

```shell
Enter board number: 0
Generated sdkconfig.board
Generated sdkconfig.board.Kconfig
Updated build/sdkconfig
Selected WT9932P4-TINY (esp32p4)
```

然后执行 `idf.py build` 进行编译：

```shell
~/WT_BSP/examples/storage/sdmmc$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/storage/sdmmc/build
Executing "ninja all"...
...
```

之后修改了代码，再次执行 `idf.py build` 进行编译。

> 开发过程中可以随时根据需要切换到不同的硬件套件；切换到不同目标芯片时会自动执行 `idf.py fullclean`。

## 硬件要求

目前支持的开发板：

* **WT9932P4-TINY**。
* **WT9932P4C61-TINY**。

运行示例前请插入 FAT 格式的 MicroSD 卡。使用 WT9932P4 硬件时，请确认板卡要求的 SD 卡供电配置和引脚连接正确。

## 示例效果

示例正常运行后，串口监视器中应看到类似日志：

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

## 故障排除

* 如果挂载失败，请检查 SD 卡是否插入正确，并确认已格式化为 FAT/FAT32。
* 检查板卡对应的 SD 卡供电配置和引脚连接。
