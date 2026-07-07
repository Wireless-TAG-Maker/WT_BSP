# WT9932P4C61-TINY ESP32-P4 Flash Bridge Firmware

This directory stores the prebuilt ESP32-P4 USB-UART bridge firmware used by
`idf.py p4_flash`.

## Purpose

The firmware turns the ESP32-P4 on WT9932P4C61-TINY into a TinyUSB CDC to UART
bridge connected to the onboard ESP32-C61.

Connect both USB ports on the board:

- FUSB (Full-Speed USB): enumerates as ESP32-P4 built-in USB-JTAG/Serial and is
  used by `idf.py p4_flash` to flash this bridge firmware into P4.
- HUSB (High-Speed USB): enumerates as TinyUSB CDC after the bridge firmware is
  running and is used to flash and monitor the C61.

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

## Usage

Run the target from any ESP-IDF project that includes `components/wt_bsp`:

```bash
idf.py p4_flash
```

The tool scans serial ports, asks you to choose the ESP32-P4 built-in
USB-JTAG/Serial port from FUSB, warns that the current P4 application will be
overwritten, then flashes the merged image.

After the bridge firmware is running, use the HUSB TinyUSB CDC serial port to
flash ESP32-C61 firmware:

```bash
idf.py -p <HUSB_CDC_PORT> flash monitor
```

## Maintenance Notes

The prebuilt image was generated from the validated P4FlashC61 bridge project
using ESP-IDF `v6.0.1`. If the firmware is rebuilt, update both the merged image
and `firmware/SHA256SUMS`.
