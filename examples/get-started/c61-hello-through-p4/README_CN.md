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

使用 **WT9932P4C61-TINY**，并将板上的 **FUSB（全速 USB）** 和
**HUSB（高速 USB）** 都接入电脑。

这两个 USB 口的用途不同：

- FUSB：枚举为 ESP32-P4 built-in USB-JTAG/Serial，用于执行 `idf.py p4_flash`，把 P4 烧录成 C61 烧录桥。
- HUSB：P4 bridge 固件运行后枚举为 TinyUSB CDC bridge，用于烧录和监视 ESP32-C61。

## 编译烧录流程

```bash
cd examples/get-started/c61-hello-through-p4

# 设置当前工程为 ESP32-C61
idf.py set-target esp32c61

# 通过 FUSB 给 ESP32-P4 烧录 C61 烧录桥固件
idf.py p4_flash

# 编译、烧录并监视 ESP32-C61。idf.py flash 会在需要时自动构建
idf.py -p <HUSB_CDC_PORT> flash monitor
```

`idf.py p4_flash` 会覆盖 ESP32-P4 当前固件。这是预期行为：P4 会临时变成板载
C61 的 USB-UART 烧录桥。这个步骤走 FUSB；最后一步的 `<HUSB_CDC_PORT>` 要换成
HUSB 枚举出的 TinyUSB CDC 串口。

预期输出：

```text
I (...) c61_hello: Hello Wireless-tag from ESP32-C61!
I (...) c61_hello: Hello Wireless-tag
```

## 恢复 ESP32-P4 固件

体验 C61 后，可通过 FUSB 枚举出的 ESP32-P4 built-in USB-JTAG/Serial 串口
重新烧录任意 WT9932P4C61-TINY 的 ESP32-P4 示例固件。
