# WT9932P4X-TINY

WT9932P4X-TINY 使用 ESP32-P4 v3.x 芯片，与使用 v1.x 芯片的 WT9932P4-TINY
共用外设资源、引脚和板级初始化实现。两者仍需分别构建固件，不能混用二进制文件。

## 芯片版本与固件选择

| 板卡 | 芯片版本 | `CONFIG_ESP32P4_SELECTS_REV_LESS_V3` | 最低版本 | 默认 CPU 频率 |
| --- | --- | --- | --- | --- |
| WT9932P4-TINY | ESP32-P4 v1.x | `y` | v1.0 | 360 MHz |
| WT9932P4X-TINY | ESP32-P4 v3.x | `n` | v3.0 | 400 MHz |

使用 ESP-IDF v6.0.0 及以上。选板会应用当前示例的 `sdkconfig.wt9932p4x_tiny`，
在 v1.x/v3.x 之间切换时重新生成芯片版本和 CPU 频率配置。即使两者的 target
都是 `esp32p4`，也必须重新运行 `idf.py build`，然后烧录对应的 bootloader 和应用。
不要通过关闭版本检查或强制烧录旧固件绕过芯片不兼容。

P4X 示例将 bootloader 日志级别设为 WARN，以满足现有 `0x8000` 分区表偏移下的
bootloader 空间限制；应用日志级别保持各示例原有配置。

该区分依据[乐鑫 ESP32-P4 v3.x 使用指南](https://documentation.espressif.com/esp32-p4-chip-revision-v3.x_user_guide_cn.html)
“对客户项目的影响”章节。PCB 外设兼容不表示芯片内存布局、ROM 或寄存器兼容。

## 选板与构建

在下列任一示例目录运行 `idf.py set-board`，选择 `WT9932P4X-TINY` 后构建：

```sh
idf.py set-board
idf.py build
```

也可以从仓库根目录直接构建：

```sh
WT_BSP_BOARD=WT9932P4X-TINY idf.py -C examples/get-started/blink build
WT_BSP_BOARD=WT9932P4X-TINY idf.py -C examples/get-started/button build
WT_BSP_BOARD=WT9932P4X-TINY idf.py -C examples/storage/sdmmc build
WT_BSP_BOARD=WT9932P4X-TINY idf.py -C examples/display/dsi build
WT_BSP_BOARD=WT9932P4X-TINY idf.py -C examples/camera/csi build
WT_BSP_BOARD=WT9932P4X-TINY idf.py -C examples/camera/usb_device_uvc build
WT_BSP_BOARD=WT9932P4X-TINY idf.py -C examples/usb/device_cdc build
WT_BSP_BOARD=WT9932P4X-TINY idf.py -C examples/wt_factory/wt9932p4-tiny build
```

工厂示例复用原 P4 工程目录，通过选板区分固件。FUSB 用于烧录和日志，HUSB
用于 UVC 或 CDC；屏幕、SC2336 CSI 摄像头、触摸、SD、RGB 与按键的接线和
操作方式沿用 WT9932P4-TINY。应用仍只包含 `wt_bsp.h`。

## 共享实现

两款板卡分别提供 `board_get_bsp_interface()` 和板名，共用
`WT9932P4-TINY/board_common.c` 的引脚、对象和资源生命周期；P4X 的能力声明
复用原板 `board_config.h`。构建时只链接所选板卡入口与一份共享实现。
`WT9932P4C61-TINY` 保持其现有独立适配。
