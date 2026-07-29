| Supported Targets | WT9932P4-TINY | WT9932P4C61-TINY |
| ----------------- | ------------- | ---------------- |

# USB Device UVC Camera Example

This example exposes the SC2336 MIPI CSI camera connected to WT9932P4-TINY or WT9932P4C61-TINY as a High-Speed USB UVC MJPEG camera. A USB host can open the stream with a standard camera application or OBS without a board-specific camera driver.

Camera capture, hardware JPEG encoding, and USB Device UVC initialization are triggered by `wt_bsp_init()` and owned by the board-level `board_init()` lifecycle. The application does not include private board headers or duplicate board pin definitions.

## Hardware

1. Connect the SC2336 camera to the board MIPI CSI connector.
2. Use FUSB to flash and monitor the ESP32-P4 firmware.
3. After the firmware starts, connect HUSB to the computer.

IO0 is connected to the camera PWDN/LDO/RESET control path on both supported boards. The BSP drives IO0 high before detecting the camera.

## Build

After loading the ESP-IDF v6.0.1 environment, run this command from the repository root:

```shell
WT_BSP_BOARD=WT9932P4C61-TINY idf.py \
    -C examples/camera/usb_device_uvc \
    -B build-usb-device-uvc-p4c61 \
    build
```

For WT9932P4-TINY, select the corresponding board configuration:

```shell
WT_BSP_BOARD=WT9932P4-TINY idf.py \
    -C examples/camera/usb_device_uvc \
    -B build-usb-device-uvc-p4 \
    build
```

Alternatively, enter the example directory, run `idf.py set-board`, select either supported board, and then run `idf.py build`.

The default configuration uses:

- High-Speed USB Device
- UVC MJPEG
- 1024x600 at 30 FPS
- JPEG quality 80

Board-level UVC initialization can be disabled at:

```text
WT BSP
└── Enable USB Device UVC
```

## Expected Result

The USB host enumerates a camera named `Wireless-Tag CSI Camera`. Typical logs are:

```text
I (...) usb_device_uvc: Initializing Wireless-Tag BSP
I (...) wt_bsp_csi: CSI initialized successfully
I (...) usbd_uvc: UVC Device Start, Version: 1.3.1
I (...) board_usb_uvc: USB Device UVC ready: MJPEG 1024x600@30fps
I (...) usb_device_uvc: USB Device UVC is ready. Connect HUSB to the USB host.
I (...) usbd_uvc: Mount
```

## Notes

- USB UVC mode owns the CSI capture device. Do not call `wt_bsp_csi_start()` from the application while it is enabled.
- A `NULL` result from `wt_bsp_get_csi()` means that the camera is disabled, missing, or failed to initialize.
- Disabling `CONFIG_WT_BSP_ENABLE_USB_DEVICE_UVC` prevents the BSP from initializing USB Device UVC.
