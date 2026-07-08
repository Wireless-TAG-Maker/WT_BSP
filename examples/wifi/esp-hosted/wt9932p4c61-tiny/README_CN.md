| Supported Targets | WT9932P4C61-TINY |
| ----------------- | ---------------- |

# ESP32-P4 ESP-Hosted Master

本示例让 ESP32-P4 作为 ESP-Hosted 主控，通过 SDIO 使用板载 ESP32-C61 的 Wi-Fi 能力。

## 🛠️ 快速上手

1.  **选择开发板**：在示例目录中运行 `idf.py set-board` 并选择支持的开发板，命令会生成下一次配置/构建使用的板级默认配置：

```shell
~/WT_BSP/examples/wifi/esp-hosted/wt9932p4c61-tiny$ idf.py set-board
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
~/WT_BSP/examples/wifi/esp-hosted/wt9932p4c61-tiny$ idf.py build
Executing action: all (aliases: build)
Running ninja in directory ~/WT_BSP/examples/wifi/esp-hosted/wt9932p4c61-tiny/build
Executing "ninja all"...
...
```

之后修改了代码，再次执行 `idf.py build` 进行编译。

> 开发过程中可以随时根据需要切换到不同的硬件套件；切换到不同目标芯片时会自动执行 `idf.py fullclean`。

## ESP32-C61 Slave 固件

运行 ESP32-P4 ESP-Hosted master 前，请确认 ESP32-C61 slave 固件已经烧录。

1. 接入 FUSB，并将 ESP32-P4 烧录为 C61 烧录桥：

```bash
cd ./examples/wifi/esp-hosted/wt9932p4c61-tiny
idf.py p4_flash
```

2. 拔掉 FUSB，接入 HUSB，然后编译并烧录 ESP32-C61 slave 固件：

```bash
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave set-target esp32c61
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave build
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave flash -p <HUSB_CDC_PORT>
```

3. 重新接入 FUSB，并烧录 ESP32-P4 master 固件：

```bash
idf.py flash monitor
```

## BSP 集成

本示例使用 `WT9932P4C61-TINY` 板级配置：

* BSP 初始化：`wt_bsp_init()`
* RGB LED 状态指示：`wt_bsp_get_rgb()`
* 配网按键：`wt_bsp_get_button()`

## RGB LED 状态指示

| 颜色 | 状态 |
|------|------|
| 蓝色 | 正在连接/重连 Wi-Fi |
| 绿色 | Wi-Fi 连接成功 |
| 红色 | Wi-Fi 连接失败 |
| 紫色 | 已进入 Wi-Fi 配网模式 |
| 熄灭 | 初始状态 |

## 按键进入配网模式

设备运行期间，长按板载按键约 1 秒可进入 Wi-Fi 配网模式。使用手机或电脑连接设备创建的配置热点，然后访问 `http://192.168.4.1` 完成 Wi-Fi 配置。

## 示例效果

### ESP32-C61 Slave 输出

```text
I (xxx) fg_mcu_slave: ESP-Hosted-MCU Slave FW version :: 0.0.6
I (xxx) fg_mcu_slave: Transport used :: SDIO only
I (xxx) fg_mcu_slave: Initial set up done
```

### ESP32-P4 Master 输出

```text
I (xxx) wifi_remote: Initializing BSP...
I (xxx) wifi_remote: wifi_init_sta finished.
I (xxx) wifi_remote: got ip:192.168.1.100
I (xxx) wifi_remote: connected to ap SSID:Your_WiFi password:Your_Password
```

## 参考资料

* [ESP-Hosted 官方文档](https://github.com/espressif/esp-hosted-mcu)
