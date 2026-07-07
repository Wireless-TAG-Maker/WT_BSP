| Supported Targets | ESP32-C61 |
| ----------------- | --------- |

# 通过 ESP32-P4 烧录 ESP32-C61 Hello 示例

本示例用于快速体验 WT9932P4C61-TINY 板载 ESP32-C61。流程是先把 ESP32-P4
临时烧录成 USB-UART 烧录桥，再通过 HUSB TinyUSB CDC 串口把这个 ESP32-C61
应用烧进去。

程序会打印：

```text
Hello Wireless-tag
```

## 硬件

使用 **WT9932P4C61-TINY**。两个 USB 口在不同步骤使用：

- FUSB（全速 USB）：枚举为 ESP32-P4 built-in USB-JTAG/Serial。执行
  `idf.py p4_flash` 时只接这个口，用于把 P4 烧录成 C61 烧录桥。
- HUSB（高速 USB）：P4 bridge 固件运行后枚举为 TinyUSB CDC bridge。烧录
  ESP32-C61 前，请拔掉 FUSB 并接入 HUSB；后续直接使用 HUSB 给开发板供电，
  并给 ESP32-C61 单独编写、烧录和调试代码。

## 1. 将 ESP32-P4 烧录为 C61 烧录桥

```bash
cd examples/get-started/c61-hello-through-p4

# 设置当前工程为 ESP32-C61
idf.py set-target esp32c61

# 执行这条命令前请接入 FUSB
# 通过 FUSB 给 ESP32-P4 烧录 C61 烧录桥固件
idf.py p4_flash
```

`idf.py p4_flash` 使用 FUSB 枚举出的 ESP32-P4 built-in USB-JTAG/Serial
串口。执行过程中有两个交互：

1. 工具会打印 `Available serial ports:`，需要选择 ESP32-P4 的 FUSB 串口。

```bash
Available serial ports:
  [0] /dev/ttyACM0 | USB JTAG/serial debug unit | USB VID:PID=303A:1001 SER=E8:F6:0A:E7:6B:32 LOCATION=1-1:1.0

Please enter the serial port number to use, then press Enter:
```

2. 工具会提示该命令将覆盖当前 ESP32-P4 固件，并要求确认。默认值为 `N`；
   只有输入 `Y` 才会继续烧录。

```bash
WARNING: This command will overwrite the current ESP32-P4 firmware.
Target board: WT9932P4C61-TINY
P4 port: /dev/cu.usbmodem21201
New firmware: ESP32-P4 USB-UART bridge for flashing/debugging onboard ESP32-C61

Type 'y' to continue, or press Enter to cancel. [y/N]:
```

覆盖当前 ESP32-P4 固件是这个流程的预期行为：P4 会临时变成板载 C61 的
USB-UART 烧录桥。

烧录完成后，拔掉 FUSB 并接入 HUSB。从这一步开始，后续直接使用 HUSB 给
开发板供电，并给 ESP32-C61 单独编写、烧录和调试代码。

## 2. 编译并烧录 ESP32-C61

使用上一步 HUSB 枚举出的 TinyUSB CDC 串口：

```bash
# 编译、烧录并监视 ESP32-C61。idf.py flash 会在需要时自动构建
idf.py -p <HUSB_CDC_PORT> flash monitor
```

将 `<HUSB_CDC_PORT>` 替换为 HUSB 枚举出的 TinyUSB CDC 串口。

预期输出：

```text
I (...) c61_hello: Hello Wireless-tag from ESP32-C61!
I (...) c61_hello: Hello Wireless-tag
```

## 恢复 ESP32-P4 固件

体验 C61 后，可通过 FUSB 枚举出的 ESP32-P4 built-in USB-JTAG/Serial 串口
重新烧录任意 WT9932P4C61-TINY 的 ESP32-P4 示例固件。
