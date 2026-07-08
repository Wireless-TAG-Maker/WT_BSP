# ESP32-P4 ESP-Hosted Master

ESP32-P4 作为 ESP-Hosted 主控，通过 SDIO 使用 ESP32-C61 的 Wi-Fi 功能。

```
┌─────────────┐         SDIO          ┌─────────────┐
│  ESP32-P4   │◄─────────────────────►│  ESP32-C61  │
│   (Master)  │                       │   (Slave)   │
│             │                       │             │
│  主控芯片    │                       │  Wi-Fi 芯片  │
└─────────────┘                       └─────────────┘
```

## 硬件连接



| ESP32-P4 | ESP32-C61 | 功能 |
|----------|-----------|------|
| GPIO 14  | D0        | SDIO 数据线 0 |
| GPIO 15  | D1        | SDIO 数据线 1 |
| GPIO 16  | D2        | SDIO 数据线 2 |
| GPIO 17  | D3        | SDIO 数据线 3 |
| GPIO 18  | CLK       | SDIO 时钟 |
| GPIO 19  | CMD       | SDIO 命令 |
| GPIO 13  | EN        | 复位控制 |
| GPIO 51  | -         | RGB LED (WS2812) |
| GPIO 35  | -         | 配网按键（低电平有效） |

## BSP 集成

本示例已集成 WT-BSP 接口，使用 `WT9932P4C61-TINY` 板级配置：


- BSP 初始化：`wt_bsp_init()`
- RGB LED 状态指示：通过 `wt_bsp_get_rgb()` 获取 RGB LED 句柄
- 配网按键：通过 `wt_bsp_get_button()` 获取板载按键句柄

## RGB LED 状态指示

| 颜色 | 状态 |
|------|------|
| 蓝色 | 正在连接/重连 Wi-Fi |
| 绿色 | Wi-Fi 连接成功 |
| 红色 | Wi-Fi 连接失败 |
| 紫色 | 已进入 Wi-Fi 配网模式 |
| 熄灭 | 初始状态 |

## 按键进入配网模式

设备运行期间，长按板载按键约 1 秒可进入 Wi-Fi 配网模式。长按期间只会触发一次，松开按键后才能再次触发。

触发后可观察到以下现象：

1. 如果设备正在连接或已经连接 Wi-Fi，会停止 Station 模式并启动配置 AP。
2. RGB LED 变为紫色。
3. 串口输出 `Button long press detected, entering configuration mode`，随后显示配置热点名称。
4. 使用手机或电脑连接设备创建的配置热点，然后访问 `http://192.168.4.1` 完成 Wi-Fi 配置。

如果设备启动时没有保存的 Wi-Fi 凭据，也会自动进入相同的配网模式，无需按键触发。

## 使用流程

### 步骤 1: 将 ESP32-P4 烧录为 C61 烧录桥

这一步请先将 WT9932P4C61-TINY 板上的 **FUSB（全速 USB）** 接入电脑。`idf.py p4_flash` 通过 FUSB 枚举出的 ESP32-P4 built-in USB-JTAG/Serial 端口给 P4 烧录 bridge 固件；烧录 ESP32-C61 固件前，请拔掉 FUSB 并接入 **HUSB（高速 USB）**。

WT9932P4C61-TINY 板载 ESP32-P4 可以临时作为 ESP32-C61 的 USB-UART 烧录桥使用。执行以下命令时有两个交互：

1. 工具会打印 `Available serial ports:`，需要选择 ESP32-P4 的 FUSB 串口。
2. 工具会提示该命令将覆盖当前 ESP32-P4 固件，并要求确认。默认值为 `N`；
   只有输入 `Y` 才会继续烧录。

> 注意：`idf.py p4_flash` 会覆盖 ESP32-P4 当前固件。完成 ESP32-C61 烧录后，如需运行本示例，还需要在步骤 3 重新烧录 ESP32-P4 主控固件。

```bash
cd ./examples/wifi/esp-hosted/wt9932p4c61-tiny

# 如当前工程尚未配置过开发板，先选择 WT9932P4C61-TINY
idf.py set-board

# 执行这条命令前请接入 FUSB
# 将 ESP32-P4 烧录为 C61 烧录桥，选择 FUSB 枚举出的 P4 USB-JTAG/Serial 端口
idf.py p4_flash
```

烧录完成后，拔掉 FUSB 并接入 HUSB。后续直接使用 HUSB 给开发板供电并烧录 ESP32-C61 slave 固件。

### 步骤 2: 编译并烧录 ESP32-C61 Slave 固件

首先需要为 ESP32-C61 编译并烧录 ESP-Hosted slave 固件：

```bash
cd ./examples/wifi/esp-hosted/wt9932p4c61-tiny

# 设置目标芯片
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave set-target esp32c61

# 编译固件
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave build

# 烧录到 ESP32-C61（替换 <HUSB_CDC_PORT> 为 P4 bridge 枚举出的 HUSB TinyUSB CDC 串口）
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave flash -p <HUSB_CDC_PORT>

# （可选）监控 ESP32-C61 输出
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave monitor -p <HUSB_CDC_PORT>
```

**关于 build_slave 目录：**
- `build_slave` 是 slave 固件的独立构建目录，用于与主项目的 `build` 目录分开
- 该目录会在项目根目录下生成，可以添加到 `.gitignore` 中
- 如需清理，可直接删除 `build_slave` 目录

ESP32-P4 bridge 固件会通过板载连接控制 ESP32-C61 的 EN 和 IO9，正常情况下不需要外接 USB-to-UART，也不需要手动进入下载模式。

### 步骤 3: 编译并烧录 ESP32-P4 主控固件

```bash
cd ./examples/wifi/esp-hosted/wt9932p4c61-tiny

# 设置目标开发板
idf.py set-board

# 编译
idf.py build

# 烧录到 ESP32-P4（选择 FUSB 枚举出的 ESP32-P4 built-in USB-JTAG/Serial 串口）
idf.py flash monitor
```

**Wi-Fi 配置：**
- SSID 和密码，执行 `idf.py menuconfig` 进入 **Example Wi-Fi Configuration  --->** 进行编辑修改

## 预期输出

### ESP32-C61 Slave 输出

```
I (xxx) fg_mcu_slave: *********************************************************************
I (xxx) fg_mcu_slave:                 ESP-Hosted-MCU Slave FW version :: 0.0.6
I (xxx) fg_mcu_slave:                 Transport used :: SDIO only
I (xxx) fg_mcu_slave: *********************************************************************
I (xxx) SDIO_SLAVE: sdio_init: ESP32-C61 SDIO RxQ[20] timing[0]
I (xxx) fg_mcu_slave: Initial set up done
```

### ESP32-P4 Master 输出

```
I (xxx) wifi_remote: Initializing BSP...
I (xxx) board: RGB LED initialized for status indication
I (xxx) wifi_remote: wifi_init_sta finished.
I (xxx) wifi_remote: got ip:192.168.1.100
I (xxx) wifi_remote: connected to ap SSID:Your_WiFi password:Your_Password
```

## 故障排查

### RGB LED 始终为红色（连接失败）
1. 检查 Wi-Fi SSID 和密码是否正确
2. 确认 ESP32-C61 slave 固件已正常运行
3. 检查 SDIO 硬件连接是否正确

### RGB LED 始终为蓝色（连接中）
1. 检查 ESP32-C61 是否能连接到目标 Wi-Fi 热点
2. 查看 ESP32-C61 的串口输出确认 Wi-Fi 状态

## 参考资料

- [ESP-Hosted 官方文档](https://github.com/espressif/esp-hosted-mcu)
