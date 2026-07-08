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

Use **WT9932P4C61-TINY**. The two USB ports are used in different steps:

- FUSB (Full-Speed USB) enumerates as ESP32-P4 built-in USB-JTAG/Serial. Use
  only this port when running `idf.py p4_flash`.
- HUSB (High-Speed USB) enumerates as the TinyUSB CDC bridge after the P4 bridge
  firmware is running. Before flashing ESP32-C61, unplug FUSB and connect HUSB.
  Use HUSB to power the board and flash/debug ESP32-C61.

## 1. Flash ESP32-P4 as the C61 Bridge

```bash
cd examples/get-started/c61-hello-through-p4

# Set this project to ESP32-C61
idf.py set-target esp32c61

# Connect FUSB before this command
# Flash the C61 bridge firmware to ESP32-P4 through FUSB
idf.py p4_flash
```

`idf.py p4_flash` uses the FUSB ESP32-P4 built-in USB-JTAG/Serial port. During
this step, the tool has two interactive prompts:

1. It prints `Available serial ports:` and asks you to select the FUSB serial
   port for ESP32-P4.

```bash
examples/get-started/c61-hello-through-p4$ idf.py p4_flash
Executing action: p4_flash
Running ninja in directory /home/biubiu/work/WT_BSP/examples/get-started/c61-hello-through-p4/build
Executing "ninja p4_flash"...
[0/1] cd /home/biubiu/work/WT_BSP/examples/get-started/c61-hello-through-p4 && /home/biubiu/.espressif/tools/python/v6.0.1/venv/bin/python /home/biubiu/work/WT_BSP/components/wt_bsp/tools/wt_bsp_p4_flash.py
Firmware SHA256 verified: e16f5afa26a27ec9c513cefb68082737bf24fbff809b2ae4b557a4ad867dd179
Available serial ports:
  [0] /dev/ttyACM0 | USB JTAG/serial debug unit | USB VID:PID=303A:1001 SER=E8:F6:0A:E7:6B:32 LOCATION=1-1:1.0
```

2. It prints a warning that the command will overwrite the current ESP32-P4
   firmware and asks for confirmation. The default answer is `N`; enter `Y` to
   continue.

```bash
Select the ESP32-P4 port number:
WARNING: This operation will overwrite the current firmware on the ESP32-P4.
Type 'y' to continue, or press Enter to cancel. [y/N]:
Target board: WT9932P4C61-TINY
P4 port: /dev/ttyACM0
New firmware: ESP32-P4 USB-UART bridge for flashing/debugging onboard ESP32-C61
```

Overwriting the current ESP32-P4 firmware is expected in this flow: the P4
temporarily becomes a USB-UART bridge for the onboard C61.

After flashing completes, unplug FUSB and connect HUSB. From this point on, use
HUSB to power the board and write, flash, and debug standalone ESP32-C61 code.

## 2. Build and Flash ESP32-C61

Use the HUSB TinyUSB CDC serial port from the previous step:

```bash
# Build, flash, and monitor ESP32-C61. idf.py flash builds automatically when needed
idf.py -p <HUSB_CDC_PORT> flash monitor
```

Replace `<HUSB_CDC_PORT>` with the TinyUSB CDC serial port from HUSB.

Expected output:

```text
I (...) c61_hello: Hello Wireless-tag from ESP32-C61!
I (...) c61_hello: Hello Wireless-tag
```

## Restore ESP32-P4 Firmware

After playing with the C61, flash any WT9932P4C61-TINY ESP32-P4 example again
through the ESP32-P4 built-in USB-JTAG/Serial port from FUSB.
