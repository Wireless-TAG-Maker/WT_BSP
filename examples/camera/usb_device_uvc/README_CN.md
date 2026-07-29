| 支持目标 | WT9932P4-TINY | WT9932P4C61-TINY |
| -------- | ------------- | ---------------- |

# USB Device UVC 摄像头示例

本示例将 WT9932P4-TINY 或 WT9932P4C61-TINY 上连接的 SC2336 MIPI CSI 摄像头模拟为 High-Speed USB UVC MJPEG 摄像头。电脑无需专用驱动即可通过系统相机、OBS 等兼容 UVC 的软件获取画面。

摄像头采集、硬件 JPEG 编码和 USB Device UVC 的初始化都由 `wt_bsp_init()` 触发，并由板级 `board_init()` 管理生命周期。应用层不直接使用板级私有头文件或引脚定义。

## 硬件连接

1. 将 SC2336 摄像头连接到开发板 MIPI CSI 接口。
2. 使用 FUSB 接口烧录 ESP32-P4 固件和查看串口日志。
3. 固件启动后，将 HUSB 接口连接到电脑。

两块支持板卡的 IO0 都连接到摄像头 PWDN/LDO/RESET 控制路径，BSP 会在检测摄像头前自动将 IO0 拉高。

## 编译

加载 ESP-IDF v6.0.1 环境后，在仓库根目录运行：

```shell
WT_BSP_BOARD=WT9932P4C61-TINY idf.py \
    -C examples/camera/usb_device_uvc \
    -B build-usb-device-uvc-p4c61 \
    build
```

使用 WT9932P4-TINY 时选择对应板级配置：

```shell
WT_BSP_BOARD=WT9932P4-TINY idf.py \
    -C examples/camera/usb_device_uvc \
    -B build-usb-device-uvc-p4 \
    build
```

也可以进入示例目录，执行 `idf.py set-board` 并选择任一受支持板卡，然后运行 `idf.py build`。

默认配置为：

- High-Speed USB Device
- UVC MJPEG
- 1024x600 @ 30 FPS
- JPEG 压缩质量 80
- 正常运行时 RGB LED 熄灭，摄像头初始化失败时常亮红灯

可在以下 menuconfig 路径关闭板级 UVC 初始化：

```text
WT BSP
└── Enable USB Device UVC
```

## 运行效果

正常启动后，电脑会枚举出名为 `Wireless-Tag CSI Camera` 的摄像头。串口日志类似：

```text
I (...) usb_device_uvc: Initializing Wireless-Tag BSP
I (...) wt_bsp_csi: CSI initialized successfully
I (...) usbd_uvc: UVC Device Start, Version: 1.3.1
I (...) board_usb_uvc: USB Device UVC ready: MJPEG 1024x600@30fps
I (...) usb_device_uvc: USB Device UVC is ready. Connect HUSB to the USB host.
I (...) usbd_uvc: Mount
```

## 说明

- USB UVC 模式独占 CSI 采集设备，应用不要再调用 `wt_bsp_csi_start()`。
- `wt_bsp_get_csi()` 返回 `NULL` 时表示摄像头未启用、未检测到或初始化失败。
- `CONFIG_WT_BSP_ENABLE_USB_DEVICE_UVC` 关闭后，BSP 不会初始化 USB Device UVC。
