# WT9932S31-TINY

板级适配依据 `SCH_WT9932S31-TINY_1V1(Kirto).pdf`，摄像头设置来自所提供的
`dvp_usb_device_uvc` 参考工程。RGB、按键和 SD 卡复用公共 feature；USB CDC
使用 `features/usb_device_cdc`；DVP 摄像头型号、引脚及 UVC 数据路径留在本板目录。

## SDK 与选板

使用 ESP-IDF **v6.1 或以上版本**。v6.1 将 `esp32s31` 列为 preview target，
选板、构建、menuconfig 等命令需加 `--preview`。其他已有板卡仍沿用仓库的
ESP-IDF v6.0.0 最低要求。

```sh
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/get-started/blink build
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/get-started/button build
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/storage/sdmmc build
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/usb/device_cdc build
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/camera/usb_device_uvc build
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/wt_factory/wt9932s31-tiny build
```

也可在相应示例目录执行 `idf.py --preview set-board` 交互选择。
工具从板级 Kconfig 发现 `esp32s31`，生成 `sdkconfig.board` 和
`sdkconfig.board.Kconfig`；跨芯片切板时沿用现有 `fullclean` 流程。

## 硬件映射

| 资源 | 配置 |
| --- | --- |
| RGB LED | GPIO19，WS2812 兼容，GRB |
| SW2 用户/BOOT 按键 | GPIO61，低电平有效；SW1 是 EN 复位 |
| SDMMC | slot 0，4-bit；CLK/CMD=24/25，D0..D3=20..23 |
| SD 电源 | 卡由板上 3.3 V 供电；SD pad 使用片上 LDO 1，1.8 V |
| 摄像头 SCCB | I2C0，SCL/SDA=1/0，100 kHz |
| 摄像头数据 | D0..D7=46..53 |
| 摄像头时序 | XCLK=55 (20 MHz)，PCLK=54，VSYNC=56，HREF/DE=57 |
| 摄像头 RESET/PWDN | 板上固定偏置，软件 GPIO 配置为 -1 |
| J1 USB | USB1 High-Speed OTG，供 UVC、CDC 或两者的复合设备使用 |
| J2 USB | USB Serial/JTAG，供烧录和日志使用 |

GPIO20..25 使用 S31 的固定 SDMMC IOMUX，不能按 P4 的 GPIO matrix 方式任意改线。
SD 卡仅在示例调用 `wt_bsp_sdmmc_mount()` 时挂载，BSP 不自动格式化卡。

## 摄像头

| menuconfig 型号 | 采集与编码 | USB 配置 |
| --- | --- | --- |
| GC2145 / CKS-K210-GC3.1 (默认) | UYVY 1600×1200，S31 硬件 JPEG，质量 80 | MJPEG，标称 8 FPS，Bulk，512 字节端点、16 KiB payload |
| OV3660 | 传感器原生 JPEG 640×480 | MJPEG，标称 25 FPS；独立示例用 Isochronous 1023 字节端点，工厂复合设备用 Bulk 512 字节端点、16 KiB payload |

先为 UVC 示例选板，然后运行：

```sh
idf.py --preview -C examples/camera/usb_device_uvc menuconfig
```

在 **WT BSP → Camera sensor** 选择型号。对应配置项为
`CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145` 或
`CONFIG_WT_BSP_S31_CAMERA_SENSOR_OV3660`。切换型号会同时调整 UVC 分辨率、帧率
和实际端点配置，随后重新构建和烧录。

两款传感器都使用 SCCB 地址 `0x3c`，因此仅注册所选型号的探测函数，并核对
传感器 PID。保持组件的两项 `*_AUTO_DETECT_DVP_INTERFACE_SENSOR` 关闭。
这里的兼容方式是编译时选择型号，不支持运行时热插拔或自动切换型号。

GC2145 沿用参考工程的完整 UXGA 时序表、输出二分频 `0xfa=0x10`、200 ms JPEG
编码时限、开流丢弃前两帧和固定缓冲复用；开停流及取帧/归还由递归互斥锁串行化。
UVC 示例启用 PSRAM，缓冲容量按 16 MB PSRAM 预算设置。

S31 的 Bulk 路径通过 `board_tinyusb_video.c` 补充末包结束处理：当最后一个
UVC payload 小于协商的最大长度、但恰好是 USB 端点包长的整数倍时，先发送
零长度包 (ZLP)，再通知当前帧发送完成。否则主机会把下一帧的头部拼入当前
JPEG。这是实机测试发现的间歇性问题；强制触发包边界后可以稳定复现。
CMake 在 S31 GC2145 或工厂复合 USB 配置下用该适配文件编译 TinyUSB 的原有视频驱动，
不修改 `managed_components`；独立 OV3660 和其他板卡使用原驱动。由于适配文件
访问驱动内部状态，S31 UVC 的 TinyUSB 依赖限定为 `0.19.*`，升级时需复核。

独立 UVC 示例关闭本地 JPEG 功能，`wt_bsp_init()` 初始化摄像头并启动 USB 枚举；缺少摄像头或型号不符
会返回错误并清理已初始化的 BSP 资源。S31 使用 DVP，`wt_bsp_get_csi()` 始终返回
`NULL`。应用不要再自行初始化 `esp_video` 或 TinyUSB。

工厂示例启用 `CONFIG_WT_BSP_ENABLE_CAMERA`，通过 `wt_bsp_get_camera()` 获取
板级持有的相机接口。该模式将硬件检测推迟到 `wt_bsp_camera_prepare()`，摄像头
缺失时仍保留 RGB、CDC 等资源用于诊断。`wt_bsp_camera_capture()` 在应用任务
中同步提供一个借用的 JPEG 帧，回调返回后自动归还；UVC 和本地采集共享互斥锁
及缓冲池。USB 枚举后拒绝本地采集，应用不应在回调中调用相机或 BSP 生命周期 API。

`wt_bsp_deinit()` 先退出 USB 任务，再关闭视频设备、释放传感器寄存器表和 JPEG
编码器，最后释放其他板级资源。应用使用句柄期间不能并发反初始化 BSP。

GC2145 右侧细彩点竖线在本次实机取帧中仍可见。OV3660 的传输已经通过
Windows DirectShow 验证，当前场景画面明显模糊；本轮按用户要求跳过清晰度问题。
传输和 JPEG 解码通过不代表画质问题已经解决。

## CDC

`CONFIG_WT_BSP_ENABLE_USB_DEVICE_CDC` 启用独立 CDC ACM 串口，使用 J1。
默认与 UVC 互斥；启用 `CONFIG_WT_BSP_USB_DEVICE_COMPOSITE` 后，两者共用
TinyUSB、PHY 和复合描述符，不安装第二个 USB 栈。J2 的 USB Serial/JTAG 不受此
选择影响。独立 CDC 示例见 [examples/usb/device_cdc](../../../../examples/usb/device_cdc/README_CN.md)。

应用只需包含 `wt_bsp.h`，成功初始化 BSP 后获取 `wt_bsp_get_usb_device_cdc()`。
句柄由板级静态对象持有，关闭功能或初始化失败时返回 `NULL`。接收采用非阻塞
读取；发送报告实际排队字节数，允许短写，并单独返回 flush 的超时状态。
示例在应用任务中处理回显，主机需打开串口并置 DTR。

`wt_bsp_usb_device_cdc_is_mounted()` 表示 J1 已完成 USB 枚举，不要求 CDC DTR；
`wt_bsp_usb_device_cdc_is_connected()` 表示 CDC 串口已打开并置 DTR。
当前硬件没有 J1 VBUS 检测线，这两个状态不能用作物理拔线检测；J2 继续供电时，
仅拔掉 J1 不会可靠清除枚举状态。
[工厂示例](../../../../examples/wt_factory/wt9932s31-tiny/README.md) 同时使用摄像头、
SD、RGB、按键及复合 USB。SD 录像默认关闭，离线只检查摄像头和 SD；在 menuconfig
的 `S31 factory` 中开启录像后，离线录制 AVI，SW2 暂停/恢复。
从 USB 模式回到离线模式时，拔掉 J1 后按 SW1/EN 复位。

## 实机验证

2026-09-20 使用 ESP-IDF v6.1 分别烧录以下示例。样机识别为 ESP32-S31 rev 0.0，
16 MB Flash、32 MB PSRAM，PSRAM 启动自检通过；这不改变上面的 16 MB 缓冲预算。

| 示例 | 验证结果 |
| --- | --- |
| RGB | 用户确认 RGB 持续变色 |
| 按键 | SW2 的 press、release、click、long press 均有日志 |
| SDMMC | 4-bit 挂载成功，写入、重命名及精确回读通过；未格式化 |
| CDC | WSL 下 97 次事务、169192 字节精确回显，5 次重新打开串口通过 |
| GC2145 UVC | WSL V4L2 下 300 帧连续采集、20 次重开各 5 帧，共 400 帧完整解码，实测约 8.04 FPS |
| OV3660 UVC | Windows DirectShow 下 300 帧连续采集、20 次重开各 5 帧，共 400 帧完整解码，采集时间戳为 25 FPS |

UVC 测试检查每帧的 JPEG 头尾、尺寸及 Pillow 完整解码，并检查板端异常日志。
GC2145 的 Bulk 修复另通过 100 帧强制末包边界测试；正式镜像不含该测试插桩。

本机 WSL USB/IP 链路下，OV3660 的 ISO 传输在 4 次尝试中均收到截断帧；
同一固件改由 Windows 原生 DirectShow 采集后通过上述测试。因此测试 OV3660
时优先使用原生 USB 主机，不能把当前 WSL 链路的失败等同于传感器采集失败。
Windows 系统自带“相机”应用尚未测试。GC2145 独立 UVC 示例仍仅验证 WSL V4L2；
两款摄像头的工厂复合设备已验证 Windows DirectShow，结果见
[工厂示例实机记录](../../../../examples/wt_factory/wt9932s31-tiny/README.md#实机验证记录)。

## 接入关系

`wt_bsp_project.cmake` 调用选板工具生成默认配置；板级 `CMakeLists.txt` 声明
资源能力，feature 的 CMake 按有效开关收集源文件和依赖。最终只有组件顶层
调用 `idf_component_register()`。

公共 `wt_bsp_interface_t` 的字段固定，不支持相应功能的板卡为 CDC 或本地 JPEG getter
显式填 `NULL`；P4/P4C61 和 S31 提供 CDC getter。S31 只初始化有效启用的资源，
失败时按反向顺序清理，getter 不转移所有权。
