| Supported Targets | WT9932P4C61-TINY |
| ----------------- | ---------------- |

# 综合工厂测试示例 (Factory Firmware)

本工厂固件集成了 Wireless-Tag BSP 提供的 MIPI DSI 显示、MIPI CSI 摄像头、触摸、SD 卡、RGB LED 控制，以及通过板载 ESP32-C61 实现的 Wi-Fi 配网能力。

## 🛠️ 快速上手

1.  **选择开发板**：在示例目录中运行 `idf.py set-board` 并选择支持的开发板，命令会生成下一次配置/构建使用的板级默认配置：

```shell
~/WT_BSP/examples/wt_factory/wt9932p4c61-tiny$ idf.py set-board
...
Supported boards in this example:
0: WT9932P4C61-TINY (esp32p4)

Please select the target board by entering the corresponding number.
Enter board number:
```

根据你的硬件套件型号输入对应数字，然后按下回车（Enter）按键。选择成功后会看到类似输出：

```shell
Enter board number: 0
Generated sdkconfig.board
Generated sdkconfig.board.Kconfig
Updated build/sdkconfig
Selected WT9932P4C61-TINY (esp32p4)
```

然后执行 `idf.py build` 进行编译：

```shell
~/WT_BSP/examples/wt_factory/wt9932p4c61-tiny$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/wt_factory/wt9932p4c61-tiny/build
Executing "ninja all"...
...
```

之后修改了代码，再次执行 `idf.py build` 进行编译。

> 开发过程中可以随时根据需要切换到不同的硬件套件；切换到不同目标芯片时会自动执行 `idf.py fullclean`。

## ESP32-C61 Slave 固件

WT9932P4C61-TINY 使用板载 ESP32-C61 提供无线连接能力。运行 ESP32-P4 固件前，如有需要请先烧录 ESP32-C61 slave 固件。

1. 接入 FUSB，并将 ESP32-P4 烧录为 C61 烧录桥：

```bash
cd ./examples/wt_factory/wt9932p4c61-tiny
idf.py p4_flash
```

2. 拔掉 FUSB，接入 HUSB，然后编译并烧录 ESP32-C61 slave 固件：

```bash
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave set-target esp32c61
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave build
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave flash -p <HUSB_CDC_PORT>
```

3. 重新接入 FUSB，并烧录 ESP32-P4 工厂固件：

```bash
idf.py flash monitor
```

## 硬件状态指示 (RGB LED)

系统启动时会检测外设连接状态，并通过板载 RGB LED 指示硬件状态。Wi-Fi 初始化开始后，Wi-Fi 状态指示会覆盖启动阶段的外设状态指示。

| LED 颜色 | 状态 |
|----------|------|
| 蓝色 | 摄像头未连接，或 Wi-Fi 正在扫描/连接/重连 |
| 绿色 | 全部正常，或 Wi-Fi 已连接 |
| 黄色 | SD 卡未连接 |
| 粉色 | 屏幕未连接 |
| 红色 | 全部未连接，或 Wi-Fi 断开/连接失败 |
| 紫色 | 已进入配网模式 |

## 核心功能

* **摄像头预览**：显示实时摄像头画面。
* **LED 控制**：调节板载 RGB LED 颜色。
* **SD 卡状态**：测试 SD 卡挂载并显示容量信息。
* **Wi-Fi 状态**：显示连接状态、未连接时的配置 AP 名称和系统时间。长按板载按键约 1 秒可进入配网模式，连接配置 AP 后访问 `http://192.168.4.1` 完成配置。

## 工程结构

* `main/main.c`：主入口，负责 BSP 初始化、硬件状态检测、LVGL 启动及外设逻辑。
* `main/lvgl_ui.c`：UI 界面实现。
* `main/wifi_manager_bridge.cpp`：Wi-Fi 连接和配网桥接逻辑。
* `managed_components/`：ESP-IDF managed component 依赖。
