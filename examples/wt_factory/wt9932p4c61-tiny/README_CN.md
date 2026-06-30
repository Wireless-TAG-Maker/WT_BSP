| Supported Targets | WT9932P4C61-TINY |
| ----------------- | ---------------- |

# 综合工厂测试示例 (Factory Firmware)

本示例是一个综合性的工厂固件，集成了 Wireless-Tag BSP 提供的多种外设功能，包括：
* **MIPI DSI 液晶屏显示**：使用 LVGL 9.4.0 展示图形界面。
* **MIPI CSI 摄像头采集**：实时采集摄像头画面并通过 PPA 硬件加速显示在屏幕上。
* **触摸控制**：支持 DSI 屏配套的触摸功能。
* **SD 卡测试**：支持 SDMMC 接口的 SD 卡挂载及容量查看。
* **RGB LED 控制**：支持板载 WS2812 RGB LED 控制。
* **Wi-Fi 配网**：显示网络连接状态和系统时间，并支持通过板载按键进入配网模式。

## 硬件状态指示 (RGB LED)

系统启动时会自动检测外设连接状态，并通过板载 RGB LED 显示不同的颜色来指示硬件状态：

| LED 颜色 | 硬件状态 | 说明 |
|----------|----------|------|
| 🟢 绿色 | 全部正常 / Wi-Fi 已连接 | 外设初始化全部正常或 Wi-Fi 连接成功 |
| 🔵 蓝色 | 摄像头未连接 |  |
| 🟡 黄色 | SD 卡未连接 |  |
| 🟠 粉色 | 屏幕未连接 |
| 🔴 红色 | 全部未连接 | 屏幕、摄像头、SD 卡均未连接 |

Wi-Fi 初始化开始后，RGB LED 将切换为指示当前 Wi-Fi 状态：

| LED 颜色 | Wi-Fi 状态 |
|----------|-------------|
| 🔵 蓝色 | 正在扫描、连接或重连热点 |
| 🟢 绿色 | 已连接 Wi-Fi 热点 |
| 🔴 红色 | Wi-Fi 已断开或连接失败 |
| 🟣 紫色 | 已进入配网模式 |

Wi-Fi 状态指示会覆盖设备启动阶段的外设状态指示。

**RGB 颜色参考值：**
- 蓝色: R=0, G=0, B=255
- 绿色: R=0, G=255, B=0
- 黄色: R=255, G=255, B=0
- 橙色: R=255, G=165, B=0
- 红色: R=255, G=0, B=0
- 紫色: R=255, G=0, B=255

## 如何使用示例

### 硬件要求

目前支持的开发板：
* **WT9932P4-TINY** (配套 480x640 MIPI DSI 屏幕和 SC2336 MIPI CSI 摄像头)
* **WT9932P4C61-TINY** (配套 480x640 MIPI DSI 屏幕和 SC2336 MIPI CSI 摄像头)

### 编译并烧录 ESP32-C61 Slave 固件

首先需要为 ESP32-C61 编译并烧录 ESP-Hosted slave 固件：

```bash
cd ./examples/wt_factory/wt9932p4c61-tiny

# 设置目标芯片
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave set-target esp32c61

# 编译固件
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave build

# 烧录到 ESP32-C61（替换 COM1 为实际串口）
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave flash -p COM1

# （可选）监控 ESP32-C61 输出
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave monitor -p COM1
```

**关于 `build_slave` 目录：**

- `build_slave` 是 slave 固件的独立构建目录，用于与主项目的 `build` 目录分开。
- 该目录会在项目根目录下生成，可以添加到 `.gitignore` 中。
- 如需清理，可直接删除 `build_slave` 目录。

**烧录模式：**

烧录时确保 ESP32-C61 处于下载模式：

1. 将 ESP32-C61 的 RX、TX 连接到串口模块。
2. ESP32-C61 的 BOOT（GPIO9）接地并保持。
3. ESP32-C61 的 EN（RESET）引脚接地，然后松开以完成复位。
4. 松开 ESP32-C61 的 BOOT（GPIO9）。

### 配置工程

在编译之前，您需要设置目标开发板：

```bash
idf.py set-board
```

选择 `WT9932P4C61-TINY (esp32p4)`：
```bash
Supported boards in this example:
0: WT9932P4C61-TINY (esp32p4)
```

本示例已包含默认的 `sdkconfig.defaults`、`sdkconfig.wt9932p4c61_tiny`，会自动配置好 PSRAM、DSI、CSI 等相关参数。

### 编译与烧录

编译工程并烧录到开发板：

```bash
idf.py build flash monitor
```

## 核心功能说明

* **摄像头预览**：顶部区域显示实时摄像头画面。点击画面可以切换全屏预览和普通预览模式。全屏模式下会使用 PPA 硬件进行 90 度旋转。
* **LED 控制**：底部左侧滑块可调节板载 RGB LED 的颜色。
* **SD 卡状态**：底部右侧上半区域可测试 SD 卡挂载，并显示其容量信息。
* **Wi-Fi 状态**：底部右侧下半区域显示连接状态、未连接时的配置 AP 名称和系统时间。长按板载按键约 1 秒可进入配网模式，连接配置 AP 后访问 `http://192.168.4.1` 完成配置。

## 工程结构

* `main/main.c`：主入口，负责 BSP 初始化、硬件状态检测、LVGL 启动及外设逻辑。
* `main/lvgl_demo_ui.c`：UI 界面实现。
* `managed_components/`：依赖的 ESP-IDF 组件。
