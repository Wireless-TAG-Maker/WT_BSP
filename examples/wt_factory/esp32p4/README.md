| Supported Targets | WT9932P4-TINY | WT9932P4C61-TINY |
| ----------------- | ------------- | ---------------- |

# 综合工厂测试示例 (Factory Firmware)

本示例是一个综合性的工厂固件，集成了 Wireless-Tag BSP 提供的多种外设功能，包括：
* **MIPI DSI 液晶屏显示**：使用 LVGL 9.4.0 展示图形界面。
* **MIPI CSI 摄像头采集**：实时采集摄像头画面并通过 PPA 硬件加速显示在屏幕上。
* **触摸控制**：支持 DSI 屏配套的触摸功能。
* **SD 卡测试**：支持 SDMMC 接口的 SD 卡挂载及容量查看。
* **RGB LED 控制**：支持板载 WS2812 RGB LED 控制。

## 如何使用示例

### 硬件要求

目前支持的开发板：
* **WT9932P4-TINY** (配套 480x640 MIPI DSI 屏幕和 SC2336 MIPI CSI 摄像头)
* **WT9932P4C61-TINY** (配套 480x640 MIPI DSI 屏幕和 SC2336 MIPI CSI 摄像头)

### 配置工程

在编译之前，您需要设置目标芯片为 `esp32p4`：

```bash
idf.py set-target esp32p4
```

本示例已包含默认的 `sdkconfig.defaults`，会自动配置好 PSRAM、DSI、CSI 等相关参数。

### 编译与烧录

编译工程并烧录到开发板：

```bash
idf.py build flash monitor
```

## 核心功能说明

* **摄像头预览**：顶部区域显示实时摄像头画面。点击画面可以切换全屏预览和普通预览模式。全屏模式下会使用 PPA 硬件进行 90 度旋转。
* **LED 控制**：底部左侧滑块可调节板载 RGB LED 的颜色。
* **SD 卡状态**：底部右侧按钮可测试 SD 卡挂载，并显示其容量信息。

## 工程结构

* `main/mipi_dsi_lcd_example_main.c`：主入口，负责 BSP 初始化、LVGL 启动及外设逻辑。
* `main/lvgl_demo_ui.c`：UI 界面实现。
* `managed_components/`：依赖的 ESP-IDF 组件。
