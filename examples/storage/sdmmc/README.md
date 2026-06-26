| Supported Targets | WT9932P4-TINY |
| ----------------- | ------------- |

# SDMMC 示例

(参见 [storage/sdmmc](../../../README.md) 了解更多信息。)

本示例演示了如何使用 Wireless-Tag BSP 提供的 SDMMC 接口显式挂载 SD 卡，并使用标准 C 库和 POSIX 函数进行基本的文件读写操作。

## 如何使用示例

### 硬件要求

目前支持的开发板：

*   **WT9932P4-TINY**

#### 硬件连接说明

使用 **WT9932P4** 开发板时，由于硬件电源配置要求，**需要将 VO4 引脚短接到 3.3V 引脚**。

此外，确保您的开发板上已插入一张格式化为 FAT 格式的 MicroSD 卡。

### 配置工程

在编译之前，您需要设置目标芯片为 `esp32p4`：

```bash
idf.py set-target esp32p4
```

### 编译与烧录

编译工程并烧录到开发板，然后查看串口输出：

```bash
idf.py build flash monitor
```

(若要退出串口监视器，请按 `Ctrl-]`。)

有关编译步骤的更多详细信息，请参阅 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)。

## 预期输出

当示例正常运行时，您应该在串口监视器中看到类似以下的日志：

```text
I (304) main_task: Started on CPU0
I (314) main_task: Calling app_main()
I (314) sdmmc_example: Initializing BSP
I (314) wt_bsp_sdmmc: Using SDMMC peripheral
I (314) wt_bsp_sdmmc: Mounting filesystem
I (354) wt_bsp_sdmmc: Filesystem mounted
Name: 00000
Type: SDHC
Speed: 20.00 MHz (limit: 20.00 MHz)
Size: 3763MB
CSD: ver=2, sector_size=512, capacity=7706624 read_bl_len=9
SSR: bus_width=4
I (364) sdmmc_example: SD card mounted at: /sdcard
I (374) sdmmc_example: Creating file: /sdcard/hello.txt
I (404) sdmmc_example: File written successfully
I (414) sdmmc_example: Renaming /sdcard/hello.txt to /sdcard/foo.txt
I (424) sdmmc_example: Reading file: /sdcard/foo.txt
I (424) sdmmc_example: Read from file: 'Hello SDMMC from WT_BSP!'
I (424) sdmmc_example: Example finished successfully
I (424) main_task: Returned from app_main()
```

## 故障排除

*   如果挂载失败，请检查 SD 卡是否插入正确，以及引脚（包括引用的引脚配置和 VO4 短接）是否连接稳固。
*   确保 SD 卡使用的是 FAT32 格式。
