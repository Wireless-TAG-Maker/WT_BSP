# ESP-Hosted 示例项目

ESP32-P4 + ESP32-C61 ESP-Hosted Wi-Fi 联网示例。

## 项目概述

本项目演示如何使用 ESP32-C61 作为 Wi-Fi 协处理器，通过 SDIO 接口为 ESP32-P4 提供 Wi-Fi 联网功能。

```
┌─────────────┐         SDIO          ┌─────────────┐
│  ESP32-P4   │◄─────────────────────►│  ESP32-C61  │
│   (Master)  │                       │   (Slave)   │
│             │                       │             │
│  主控芯片    │                       │  Wi-Fi 芯片  │
└─────────────┘                       └─────────────┘
```

## 项目结构

```
examples/wifi/esp-hosted/
├── README.md         # 项目说明
├── esp32p4/          # ESP32-P4 Master（主控）
│   ├── main/         # 主程序
│   ├── CMakeLists.txt
│   ├── sdkconfig
│   └── README.md
└── esp32c61/         # ESP32-C61 Slave（协处理器）
    ├── main/         # 主程序
    ├── CMakeLists.txt
    ├── sdkconfig
    ├── sdkconfig.defaults
    ├── partitions.esp32c61.csv
    └── README.md
```

## 快速开始

### 1. 烧录 ESP32-C61 Slave 固件

**必须先烧录 C61，等待其进入 "Waiting for host connection" 状态后再启动 P4**

```bash
cd ~/work/WT_BSP/examples/wifi/esp-hosted/esp32c61
idf.py set-target esp32c61
idf.py build
idf.py -p <C61串口> flash monitor
```

预期输出：
```
I (xxx) esp_hosted_slave_c61: ========================================
I (xxx) esp_hosted_slave_c61:  ESP32-C61 ESP-Hosted Slave Example
I (xxx) esp_hosted_slave_c61: ========================================
I (xxx) esp_hosted_slave_c61: ESP-Hosted Slave initialized
I (xxx) esp_hosted_slave_c61: Waiting for host connection via SDIO...
```

### 2. 配置并烧录 ESP32-P4 Master 固件

```bash
cd ~/work/WT_BSP/examples/wifi/esp-hosted/esp32p4

# 配置 Wi-Fi SSID 和密码
idf.py menuconfig
# 进入 `Component config` -> `Example Configuration` -> `Example Wi-Fi Configuration`
# 设置 WiFi SSID 和 Password

# 编译并烧录
idf.py build
idf.py -p <P4串口> flash monitor
```

预期输出：
```
I (xxx) esp_hosted_master_p4: ========================================
I (xxx) esp_hosted_master_p4:  ESP32-P4 ESP-Hosted Master Example
I (xxx) esp_hosted_master_p4: ========================================
I (xxx) esp_hosted_master_p4: SDIO GPIO: CLK=18, CMD=19, D0-3=14-17, RESET=13
I (xxx) transport: Identified slave [esp32c61]
I (xxx) esp_hosted_master_p4: Successfully connected to Wi-Fi!
I (xxx) esp_hosted_master_p4: Got IP:192.168.1.100
```

## 硬件连接

### SDIO 接口连接（ESP32-P4 ↔ ESP32-C61）

**注意：SDIO 引脚在 P4 和 C61 之间是交叉连接的**

| ESP32-P4 | ESP32-C61 | 功能 |
|----------|-----------|------|
| GPIO 18  | GPIO 19   | CLK (SDIO 时钟) |
| GPIO 19  | GPIO 18   | CMD (SDIO 命令) |
| GPIO 14  | GPIO 20   | D0 (SDIO 数据线 0) |
| GPIO 15  | GPIO 21   | D1 (SDIO 数据线 1) |
| GPIO 16  | GPIO 22   | D2 (SDIO 数据线 2) |
| GPIO 17  | GPIO 23   | D3 (SDIO 数据线 3) |
| GPIO 13  | EN        | 复位控制（低电平有效） |

### 天线连接

- ESP32-C61 需要连接 Wi-Fi 天线才能正常工作
- 确保天线连接牢固，否则 Wi-Fi 连接会失败

## 软件依赖

| 组件 | 版本 |
|------|------|
| ESP-IDF | 5.5.4 |
| esp_hosted | 2.12.9 |
| esp_wifi_remote | 1.6.1 |

## 配置说明

### ESP32-P4 (Master) 配置

通过 `idf.py menuconfig` 配置：

```
Component config -> Example Configuration -> Example Wi-Fi Configuration
├── WiFi SSID: 您的Wi-Fi名称
├── WiFi Password: 您的Wi-Fi密码
└── Maximum retry: 5
```

### ESP32-C61 (Slave) 配置

Slave 端使用默认配置，主要参数在 `sdkconfig.defaults` 中：

- SDIO 接口模式
- 4-bit 数据总线
- 40MHz 时钟频率
- 2MB Flash 大小

## 故障排查

### 1. SDIO 连接失败

**症状**：P4 端报错 `Card init failed` 或 `send_op_cond returned 0x107`

**解决方案**：
- 检查 SDIO 线缆连接是否正确
- 确认 C61 已烧录 ESP-Hosted slave 固件并正在运行
- 检查 GPIO 配置是否正确
- 确保 C61 先启动并进入 "Waiting for host connection" 状态

### 2. Wi-Fi 连接失败

**症状**：P4 端报错 `Failed to connect to AP`

**解决方案**：
- 确认 Wi-Fi SSID 和密码正确（通过 `idf.py menuconfig` 配置）
- 检查 Wi-Fi AP 信号强度
- **确认 C61 已连接 Wi-Fi 天线**
- 检查 Wi-Fi AP 是否支持 WPA2-PSK 认证
- 查看 C61 端的 log 获取详细错误信息

### 3. 目标芯片识别错误

**症状**：P4 端报错 `Identified slave [esp32c61] != Expected [esp32c6]`

**解决方案**：
- 修改 P4 的 `sdkconfig` 中的 `CONFIG_ESP_HOSTED_CP_TARGET_ESP32C61=y`

## 运行流程

1. **启动 C61**：烧录并启动 ESP32-C61 slave 固件
2. **等待就绪**：C61 显示 "Waiting for host connection via SDIO..."
3. **启动 P4**：烧录并启动 ESP32-P4 master 固件
4. **建立连接**：P4 通过 SDIO 连接到 C61
5. **Wi-Fi 连接**：P4 通过 C61 连接到 Wi-Fi AP
6. **数据传输**：P4 通过 C61 进行网络通信

## 参考资料

- [ESP-Hosted MCU 文档](https://github.com/espressif/esp-hosted-mcu)
- [esp_wifi_remote 组件](https://components.espressif.com/components/espressif/esp_wifi_remote)
- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/en/latest/)

## 许可证

本项目基于 ESP-Hosted 组件，遵循 Apache-2.0 许可证。
