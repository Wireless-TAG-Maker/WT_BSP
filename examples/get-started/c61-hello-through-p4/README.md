| Supported Targets | ESP32-C61 |
| ----------------- | --------- |

# ESP32-C61 Hello Through ESP32-P4

This example shows the quickest way to play with the onboard ESP32-C61 on WT9932P4C61-TINY. It first uses the ESP32-P4 as a USB-UART flash bridge, then flashes this ESP32-C61 app through the HUSB TinyUSB CDC serial port.

The app prints:

```text
Hello Wireless-tag
```

## Getting Started

1.  Connect **FUSB (Full-Speed USB)** and flash the ESP32-P4 bridge firmware:

```shell
~/WT_BSP/examples/get-started/c61-hello-through-p4$ idf.py set-target esp32c61
~/WT_BSP/examples/get-started/c61-hello-through-p4$ idf.py p4_flash
```

2.  After the bridge firmware is flashed, unplug FUSB and connect **HUSB (High-Speed USB)**.

3.  Build, flash, and monitor the ESP32-C61 app through the HUSB TinyUSB CDC serial port:

```shell
~/WT_BSP/examples/get-started/c61-hello-through-p4$ idf.py -p <HUSB_CDC_PORT> flash monitor
```

Replace `<HUSB_CDC_PORT>` with the TinyUSB CDC serial port enumerated from HUSB.

## Hardware

Use **WT9932P4C61-TINY**. The two USB ports are used in different steps:

- FUSB (Full-Speed USB) enumerates as ESP32-P4 built-in USB-JTAG/Serial. Use only this port when running `idf.py p4_flash`.
- HUSB (High-Speed USB) enumerates as the TinyUSB CDC bridge after the P4 bridge firmware is running. Before flashing ESP32-C61, unplug FUSB and connect HUSB. Use HUSB to power the board and flash/debug ESP32-C61.

## Detailed Flow

### 1. Flash ESP32-P4 as the C61 Bridge

```bash
cd examples/get-started/c61-hello-through-p4
idf.py set-target esp32c61
idf.py p4_flash
```

`idf.py p4_flash` uses the FUSB ESP32-P4 built-in USB-JTAG/Serial port. The tool asks you to select the FUSB serial port and then confirms that the current ESP32-P4 firmware will be overwritten. Enter `Y` to continue.

Overwriting the current ESP32-P4 firmware is expected in this flow: the P4 temporarily becomes a USB-UART bridge for the onboard C61.

After flashing completes, unplug FUSB and connect HUSB.

### 2. Build and Flash ESP32-C61

```bash
idf.py -p <HUSB_CDC_PORT> flash monitor
```

Expected output:

```text
I (...) c61_hello: Hello Wireless-tag from ESP32-C61!
I (...) c61_hello: Hello Wireless-tag
```

## Restore ESP32-P4 Firmware

After playing with the C61, flash any WT9932P4C61-TINY ESP32-P4 example again through the ESP32-P4 built-in USB-JTAG/Serial port from FUSB.
