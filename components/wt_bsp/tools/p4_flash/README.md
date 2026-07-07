# WT9932P4C61-TINY ESP32-P4 Flash Bridge Firmware

This directory stores the prebuilt ESP32-P4 USB-UART bridge firmware used by
`idf.py p4_flash`.

## Purpose

The firmware turns the ESP32-P4 on WT9932P4C61-TINY into a TinyUSB CDC to UART
bridge connected to the onboard ESP32-C61.

Use the two USB ports in different steps:

- FUSB (Full-Speed USB): enumerates as ESP32-P4 built-in USB-JTAG/Serial and is
  used by `idf.py p4_flash` to flash this bridge firmware into P4. Connect FUSB
  for this step.
- HUSB (High-Speed USB): enumerates as TinyUSB CDC after the bridge firmware is
  running and is used to flash and monitor the C61. Before flashing ESP32-C61,
  unplug FUSB and connect HUSB. Use HUSB to power the board and develop
  standalone ESP32-C61 applications.

## Firmware

- Image: `firmware/P4FlashC61_usb2uart_esp32p4_merged.bin`
- Target: `esp32p4`
- Flash offset: `0x0`
- ESP-IDF used for the validated build: `v6.0.1`
- SHA256: recorded in `firmware/SHA256SUMS`

## WT9932P4C61-TINY Wiring

| ESP32-P4 | ESP32-C61 | Function |
|----------|-----------|----------|
| GPIO52 | EN | Reset control |
| GPIO2 | IO9 | Download strap |
| GPIO54 | UART0 RX | C61 TX to P4 RX |
| GPIO53 | UART0 TX | P4 TX to C61 RX |

## 1. Flash ESP32-P4 as the C61 Bridge

Run the target from any ESP-IDF project that includes `components/wt_bsp`:

```bash
# Connect FUSB before this command
idf.py p4_flash
```

`idf.py p4_flash` uses the ESP32-P4 built-in USB-JTAG/Serial port from FUSB.
The command has two interactive prompts:

1. It prints `Available serial ports:` and asks you to select the FUSB serial
   port for ESP32-P4.
2. It prints a warning that the command will overwrite the current ESP32-P4
   firmware and asks for confirmation. The default answer is `N`; enter `Y` to
   continue.

After the bridge firmware is running, unplug FUSB and connect HUSB. From this
point on, use HUSB to power the board and write, flash, and debug standalone
ESP32-C61 code.

## 2. Flash ESP32-C61 Firmware

Use the HUSB TinyUSB CDC serial port to flash ESP32-C61 firmware:

```bash
idf.py -p <HUSB_CDC_PORT> flash monitor
```

## Maintenance Notes

The prebuilt image was generated from the validated P4FlashC61 bridge project
using ESP-IDF `v6.0.1`. If the firmware is rebuilt, update both the merged image
and `firmware/SHA256SUMS`.
