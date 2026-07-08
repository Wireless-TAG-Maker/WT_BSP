| Supported Targets | ESP32-C61 |
| ----------------- | --------- |

# 通过 ESP32-P4 烧录 ESP32-C61 Hello 示例

本示例用于快速体验 WT9932P4C61-TINY 板载 ESP32-C61。流程是先把 ESP32-P4 临时烧录成 USB-UART 烧录桥，再通过 HUSB TinyUSB CDC 串口把这个 ESP32-C61 应用烧进去。

程序会打印：

```text
Hello Wireless-tag
```

## 🛠️ 快速上手

1.  接入 **FUSB（全速 USB）**，并将 ESP32-P4 烧录为 C61 烧录桥：

```shell
~/WT_BSP/examples/get-started/c61-hello-through-p4$ idf.py set-target esp32c61
~/WT_BSP/examples/get-started/c61-hello-through-p4$ idf.py p4_flash
```

2.  烧录桥固件完成后，拔掉 FUSB 并接入 **HUSB（高速 USB）**。

3.  通过 HUSB 枚举出的 TinyUSB CDC 串口编译、烧录并监视 ESP32-C61 应用：

```shell
~/WT_BSP/examples/get-started/c61-hello-through-p4$ idf.py -p <HUSB_CDC_PORT> flash monitor
```

将 `<HUSB_CDC_PORT>` 替换为 HUSB 枚举出的 TinyUSB CDC 串口。

## 硬件

使用 **WT9932P4C61-TINY**。两个 USB 口在不同步骤使用：

- FUSB（全速 USB）：枚举为 ESP32-P4 built-in USB-JTAG/Serial。执行 `idf.py p4_flash` 时只接这个口，用于把 P4 烧录成 C61 烧录桥。
- HUSB（高速 USB）：P4 bridge 固件运行后枚举为 TinyUSB CDC bridge。烧录 ESP32-C61 前，请拔掉 FUSB 并接入 HUSB；后续直接使用 HUSB 给开发板供电，并给 ESP32-C61 单独编写、烧录和调试代码。

## 详细流程

### 1. 将 ESP32-P4 烧录为 C61 烧录桥

```bash
cd examples/get-started/c61-hello-through-p4
idf.py set-target esp32c61
idf.py p4_flash
```

`idf.py p4_flash` 使用 FUSB 枚举出的 ESP32-P4 built-in USB-JTAG/Serial 串口。工具会要求选择 FUSB 串口，并提示该命令将覆盖当前 ESP32-P4 固件。输入 `Y` 后继续烧录。

覆盖当前 ESP32-P4 固件是这个流程的预期行为：P4 会临时变成板载 C61 的 USB-UART 烧录桥。

烧录完成后，拔掉 FUSB 并接入 HUSB。

### 2. 编译并烧录 ESP32-C61

```bash
idf.py -p <HUSB_CDC_PORT> flash monitor
```

预期输出：

```text
I (...) c61_hello: Hello Wireless-tag from ESP32-C61!
I (...) c61_hello: Hello Wireless-tag
```

## 恢复 ESP32-P4 固件

体验 C61 后，可通过 FUSB 枚举出的 ESP32-P4 built-in USB-JTAG/Serial 串口重新烧录任意 WT9932P4C61-TINY 的 ESP32-P4 示例固件。
