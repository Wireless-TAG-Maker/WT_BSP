| Supported Targets | ESP32-C61 |
| ----------------- | --------- |

# ESP32-C61 Hello Through ESP32-P4

This example shows the quickest way to play with the onboard ESP32-C61 on
WT9932P4C61-TINY. It first uses the ESP32-P4 as a USB-UART flash bridge, then
flashes this ESP32-C61 app through the HUSB TinyUSB CDC serial port.

The app prints:

```text
Hello Wireless-tag
```

## Hardware

Use **WT9932P4C61-TINY** with both board USB ports connected to the computer:
**FUSB (Full-Speed USB)** and **HUSB (High-Speed USB)**.

The two USB ports have different jobs:

- FUSB enumerates as ESP32-P4 built-in USB-JTAG/Serial. `idf.py p4_flash`
  uses this port to flash the P4 bridge firmware.
- HUSB enumerates as the TinyUSB CDC bridge after the P4 bridge firmware is
  running. Use this port to flash and monitor ESP32-C61.

## Build and Flash

```bash
cd examples/get-started/c61-hello-through-p4

# Set this project to ESP32-C61
idf.py set-target esp32c61

# Flash the C61 bridge firmware to ESP32-P4 through FUSB
idf.py p4_flash

# Build, flash, and monitor ESP32-C61. idf.py flash builds automatically when needed
idf.py -p <HUSB_CDC_PORT> flash monitor
```

`idf.py p4_flash` overwrites the current ESP32-P4 firmware. This is expected:
the P4 temporarily becomes a USB-UART bridge for the onboard C61. This step uses
FUSB. Replace `<HUSB_CDC_PORT>` with the TinyUSB CDC serial port from HUSB.

Expected output:

```text
I (...) c61_hello: Hello Wireless-tag from ESP32-C61!
I (...) c61_hello: Hello Wireless-tag
```

## Restore ESP32-P4 Firmware

After playing with the C61, flash any WT9932P4C61-TINY ESP32-P4 example again
through the ESP32-P4 built-in USB-JTAG/Serial port from FUSB.
