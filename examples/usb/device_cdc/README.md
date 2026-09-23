# USB Device CDC echo

WT9932P4-TINY uses ESP32-P4 v1.x; WT9932P4X-TINY uses v3.x. They share peripheral wiring but require different firmware. Select the actual board with `idf.py set-board`; see the [revision and build notes](../../../components/wt_bsp/boards/WT9932P4X-TINY/README.md).

This example provides USB CDC ACM serial echo through the BSP on ESP32-P4
and ESP32-S31 boards.

| Board | ESP-IDF | Flash / log connector | CDC echo connector |
| --- | --- | --- | --- |
| WT9932P4-TINY | v6.0.0 or later | FUSB | HUSB (High-Speed USB OTG) |
| WT9932P4X-TINY | v6.0.0 or later | FUSB | HUSB (High-Speed USB OTG) |
| WT9932P4C61-TINY | v6.0.0 or later | FUSB | HUSB (High-Speed USB OTG) |
| WT9932S31-TINY | v6.1 or later | J2 | J1 (High-Speed USB OTG) |

On WT9932P4C61-TINY, this example runs on the ESP32-P4.

Build from the repository root:

```sh
WT_BSP_BOARD=WT9932P4-TINY idf.py -C examples/usb/device_cdc -B build-cdc-p4 build
WT_BSP_BOARD=WT9932P4X-TINY idf.py -C examples/usb/device_cdc -B build-cdc-p4x build
WT_BSP_BOARD=WT9932P4C61-TINY idf.py -C examples/usb/device_cdc -B build-cdc-p4c61 build
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/usb/device_cdc build
```

Choose the command matching your board. S31 requires `--preview` on ESP-IDF v6.1.
You can also select a board with `idf.py set-board` from the example directory;
use `idf.py --preview set-board` for S31.

Flash through the connector listed in the table, then connect the CDC echo
connector to the host and open the new `Wireless-Tag CDC` serial port with DTR
asserted. Sent bytes are echoed unchanged. The flash / log port is separate;
the CDC baud-rate setting does not change USB transfer speed.

The application uses only `wt_bsp.h` for BSP initialization, handle lookup and
CDC I/O. Short writes retry only the unqueued suffix. Blocking work stays in
the application task. Disconnecting or clearing DTR discards the pending echo
suffix for that data block.

This example uses standalone CDC with UVC disabled. Standalone CDC and UVC are
mutually exclusive on P4. S31 can expose both interfaces as a composite device
in the [factory example](../../wt_factory/wt9932s31-tiny/README.md).
The BSP owns TinyUSB and its resources; applications must not install or release
the driver themselves. See the [S31 board notes](../../../components/wt_bsp/boards/WT9932S31-TINY/README.md)
for its connector details.
