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

## BSP 集成

本示例已集成 WT-BSP 接口，使用 `WT9932P4C61-TINY` 板级配置：


- BSP 初始化：`wt_bsp_init()`
- RGB LED 状态指示：通过 `wt_bsp_get_rgb()` 获取 RGB LED 句柄

## RGB LED 状态指示

| 颜色 | 状态 |
|------|------|
| 蓝色 | 正在连接/重连 Wi-Fi |
| 绿色 | Wi-Fi 连接成功 |
| 红色 | Wi-Fi 连接失败 |
| 熄灭 | 初始状态 |

## 使用流程

### 步骤 1: 编译并烧录 ESP32-C61 Slave 固件

首先需要为 ESP32-C61 编译并烧录 ESP-Hosted slave 固件：

```bash
cd ./examples/wifi/esp-hosted/wt9932p4c61-tiny

# 设置目标芯片
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave set-target esp32c61

# 编译固件
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave build

# 烧录到 ESP32-C61（替换 COM1 为实际串口）
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave flash -p COM1

# （可选）监控 ESP32-C61 输出
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave monitor -p COM1
```

**关于 build_slave 目录：**
- `build_slave` 是 slave 固件的独立构建目录，用于与主项目的 `build` 目录分开
- 该目录会在项目根目录下生成，可以添加到 `.gitignore` 中
- 如需清理，可直接删除 `build_slave` 目录

**烧录模式：**
烧录时确保 ESP32-C61 处于下载模式：

1. 将ESP32-C61的RX、TX连接到串口模块
2. ESP32-C61的BOOT（GPIO9）接地（保持）
3. ESP32-C61的EN（RESET）引脚接地，然后松开（完成复位）
4. 松开 ESP32-C61的BOOT（GPIO9）

### 步骤 2: 编译并烧录 ESP32-P4 主控固件

```bash
cd ./examples/wifi/esp-hosted/wt9932p4c61-tiny

# 设置目标芯片
idf.py set-target esp32p4

# 编译
idf.py build

# 烧录到 ESP32-P4（替换 COM2 为实际串口）
idf.py -p COM2 flash monitor
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
