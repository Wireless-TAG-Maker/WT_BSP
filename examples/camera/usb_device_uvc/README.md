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

After loading an ESP-IDF v6.0.0 or later environment, run this command from the repository root:

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
- RGB LED off during normal operation and solid red if camera initialization fails

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

### Expected Warnings

The following warnings can appear during normal operation and do not indicate a camera or UVC failure:

```text
W (...) board: Display device not found at address: 0x28
W (...) board: Touch device not found at address: 0x55
```

The BSP scans the shared I2C bus for the optional display, touch controller, and camera. These two warnings are expected when only the SC2336 camera is connected. Camera initialization can continue as long as the camera is detected at address `0x30`.

```text
W (...) usb_phy: Using UTMI PHY instead of requested internal PHY
```

ESP-IDF selects the ESP32-P4 UTMI PHY for High-Speed USB Device operation. This compatibility fallback is expected and does not reduce the configured UVC speed.

On ESP32-P4 revisions that support ISP AWB subwindows, a 1024x600 stream can also produce the following warning repeatedly while streaming. This example suppresses the `ISP_AWB` tag at runtime by default; the warning may still appear if that filter is removed or the BSP is used by another application:

```text
W (...) ISP_AWB: subwindow size (1024 x 600) is not divisible by AWB subwindow blocks grid (5 x 5).
             Resolution will be floored to the nearest divisible value.
```

This warning applies only to the ISP automatic white balance statistics window. ESP-IDF aligns that internal window from 1024x600 to 1020x600 because its width must be divisible by the 5x5 AWB grid. Camera capture, JPEG encoding, and USB UVC output remain at 1024x600. With `esp_video` 2.3.x, AWB parameters can be reconfigured for each frame, so the same non-fatal warning may be printed at approximately the stream frame rate.

Do not change the UVC resolution to 1020x600 only to suppress this warning. If the host receives a stable image and the log reports that CSI, UVC, and streaming started successfully, no action is required.

### Windows Preview

On Windows 11, open **Settings > Bluetooth & devices > Cameras**, select `Wireless-Tag CSI Camera`, and view the live preview as shown below:

![Windows 11 UVC camera preview](docs/images/windows-uvc-camera-preview.png)

> The screenshot was captured before the custom UVC interface name was applied. With the current firmware, `UVC CAM1` in the screenshot is displayed as `Wireless-Tag CSI Camera`.
>
> The screenshot above was captured with WT9932P4C61-TINY. This board uses an ESP32-P4 v3.x chip, and the camera image is processed by the ISP. WT9932P4-TINY uses an ESP32-P4 v1.x chip without ISP processing in this example, so its displayed image quality may be lower.

## Notes

- USB UVC mode owns the CSI capture device. Do not call `wt_bsp_csi_start()` from the application while it is enabled.
- A `NULL` result from `wt_bsp_get_csi()` means that the camera is disabled, missing, or failed to initialize.
- Disabling `CONFIG_WT_BSP_ENABLE_USB_DEVICE_UVC` prevents the BSP from initializing USB Device UVC.
