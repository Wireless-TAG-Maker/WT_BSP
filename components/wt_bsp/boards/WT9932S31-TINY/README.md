# WT9932S31-TINY

板级适配依据 `SCH_WT9932S31-TINY_1V1(Kirto).pdf`，摄像头设置来自所提供的
`dvp_usb_device_uvc` 参考工程。RGB、按键和 SD 卡复用公共 feature；USB CDC
使用 `features/usb_device_cdc`；USB UVC 配置和启停复用 `features/usb_device_uvc`。
DVP 摄像头型号、引脚、工作参数和缓冲预算由本板配置，采集与 JPEG 输出复用
[`features/camera`](../../features/camera/README.md)。板级持有静态相机对象，并调用公共 feature 初始化和释放。
应用层的 `examples/common_components/wt_camera_uvc` 负责将 JPEG 转交 UVC，BSP 不自动连接两者。
启用相机不需要启用 UVC；`wt_bsp_init()` 准备 UVC 对象，应用显式调用 start 后才启动 USB。

## SDK 与选板

使用 ESP-IDF **v6.1 或以上版本**。v6.1 将 `esp32s31` 列为 preview target，
选板、构建、menuconfig 等命令需加 `--preview`。其他已有板卡仍沿用仓库的
ESP-IDF v6.0.0 最低要求。

```sh
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/get-started/blink set-board
idf.py --preview -C examples/get-started/blink build
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/get-started/button set-board
idf.py --preview -C examples/get-started/button build
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/storage/sdmmc set-board
idf.py --preview -C examples/storage/sdmmc build
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/usb/device_cdc set-board
idf.py --preview -C examples/usb/device_cdc build
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/camera/usb_device_uvc set-board
idf.py --preview -C examples/camera/usb_device_uvc build
WT_BSP_BOARD=WT9932S31-TINY idf.py --preview -C examples/wt_factory/wt9932s31-tiny set-board
idf.py --preview -C examples/wt_factory/wt9932s31-tiny build
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
| 摄像头时序 | XCLK=55（GC2145 720p 为 24 MHz，其余为 20 MHz），PCLK=54，VSYNC=56，HREF/DE=57 |
| 摄像头 RESET/PWDN | 板上固定偏置，软件 GPIO 配置为 -1 |
| J1 USB | USB1 High-Speed OTG，供 UVC、CDC 或两者的复合设备使用 |
| J2 USB | USB Serial/JTAG，供烧录和日志使用 |

GPIO20..25 使用 S31 的固定 SDMMC IOMUX，不能按 P4 的 GPIO matrix 方式任意改线。
SD 卡仅在示例调用 `wt_bsp_sdmmc_mount()` 时挂载，BSP 不自动格式化卡。

## 摄像头

| menuconfig 型号 | 采集与编码 | 示例应用的 USB profile |
| --- | --- | --- |
| GC2145 / CKS-K210-GC3.1 UXGA（板级及工厂示例默认） | UYVY 1600×1200，S31 硬件 JPEG，质量 80 | MJPEG，标称 8 FPS，Bulk，512 字节端点、16 KiB payload |
| GC2145 / CKS-K210-GC3.1 720p（独立 UVC 示例默认） | 居中裁切 UYVY 1280×720，S31 硬件 JPEG，质量 60 | MJPEG，30 FPS，独立示例使用 Isochronous 1023 字节端点 |
| OV3660 | 传感器原生 JPEG 640×480 | MJPEG，标称 25 FPS；独立示例用 Isochronous 1023 字节端点，工厂复合设备用 Bulk 512 字节端点、16 KiB payload |

先为 UVC 示例选板，然后运行：

```sh
idf.py --preview -C examples/camera/usb_device_uvc menuconfig
```

在 **WT BSP → Camera sensor** 选择型号和模式。对应配置项为
`CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145`、`CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145_720P`
或 `CONFIG_WT_BSP_S31_CAMERA_SENSOR_OV3660`。板级选项决定相机硬件模式。
板级通过统一 `wt_bsp_camera_info_t` 传入 `sensor_name = "gc2145"`、`"gc2145_720p"` 或 `"ov3660"`，
interface 自动查找已注册驱动，由其 factory 返回统一相机操作表。
GC2145 不需要额外属性；OV3660 通过 `wt_bsp_camera_property_t` 数组提供
`ae_level`、`jpeg_quality`、`saturation`、`sharpness`、`denoise` 五项整数属性。
名称、属性数组和属性名使用静态数据，直到相机反初始化成功前保持有效。
现有两份传感器 Kconfig 设置所需驱动和模式，feature 自动收集传感器实现。
后续新增型号实现模块自己的 `.c/.h` 并自行注册 factory，板级填写名称、属性和硬件参数即可接入，
无需包含传感器私有头文件或扩展 camera 的中央型号表。详见[新增传感器模块](../../features/camera/README.md#新增传感器模块)。
UVC 示例的应用组件再将该模式映射到 USB 分辨率、帧率和端点 profile，随后重新构建和烧录。

两款传感器都使用 SCCB 地址 `0x3c`，因此只探测所选型号并核对传感器 PID。
使用 `esp_cam_sensor` 的模式保持组件的两项 `*_AUTO_DETECT_DVP_INTERFACE_SENSOR` 关闭。
这里的兼容方式是编译时选择型号，不支持运行时热插拔或自动切换型号。

GC2145 UXGA 沿用参考工程的完整时序表、输出二分频 `0xfa=0x10`、200 ms JPEG
编码时限、开流丢弃前两帧和固定缓冲复用；开停流及取帧/归还由递归互斥锁串行化。
采集管线位于 `features/camera/backends/wt_bsp_camera_dvp.c`，GC2145 的 UYVY 模式扩展和
OV3660 的图像调节分别位于 `features/camera/sensors/gc2145/` 与 `sensors/ov3660/`。
GC2145 UXGA 和 OV3660 使用 `esp_cam_sensor`。GC2145 720p 通过原生 DVP 采集，
将 UYVY 直接送入硬件 JPEG 编码器。相机 feature 不依赖本板的私有头文件或配置符号。
UVC 示例启用 PSRAM，缓冲容量按 16 MB PSRAM 预算设置。
GC2145 UXGA 使用一个 DVP 用户缓冲和驱动备用缓冲，为应用层独立 JPEG 快照留出空间。
GC2145 720p 和 OV3660 使用两个 DVP 用户缓冲。

S31 示例的 Bulk 路径通过应用组件中的 `usb/camera_uvc_tinyusb_video.c` 补充末包结束处理：当最后一个
UVC payload 小于协商的最大长度、但恰好是 USB 端点包长的整数倍时，先发送
零长度包 (ZLP)，再通知当前帧发送完成。否则主机会把下一帧的头部拼入当前
JPEG。这是实机测试发现的间歇性问题；强制触发包边界后可以稳定复现。
CMake 在 S31 GC2145 UXGA 或工厂复合 USB 配置下使用该适配文件。
独立 720p、OV3660 和其他板卡使用原 TinyUSB 视频驱动。由于适配文件
访问驱动内部状态，S31 UVC 的 TinyUSB 依赖限定为 `0.19.*`，升级时需复核。

相机功能独立由 `CONFIG_WT_BSP_ENABLE_CAMERA` 启用。`wt_bsp_init()` 将硬件配置传给 camera feature，准备相机对象和同步资源，
硬件检测在 `wt_bsp_camera_prepare()` 时进行；相机缺失不会阻止其他独立外设初始化。
`wt_bsp_camera_capture()` 在调用任务中同步借出 JPEG 帧，回调返回后自动归还。
相机 API 不检查 USB 是否枚举，也不决定录像或推流优先级。
S31 使用 DVP，因而 `wt_bsp_get_csi()` 始终返回 `NULL`。

UVC 示例和工厂应用在 BSP 初始化后显式调用 `camera_uvc_start()`，由应用层连接 JPEG
相机和 UVC，并管理转交缓冲及本地录像/USB 推流的互斥。
应用应先成功停止桥接，再调用 `wt_bsp_deinit()` 释放相机和其他板级资源。
相机释放失败时保留尚被驱动借用的资源和对象，应用应保留相关上下文并重试释放。
不要在帧回调中调用相机或 BSP 生命周期 API。

历史固件取帧中仍可见 GC2145 右侧细彩点竖线；OV3660 的传输曾通过
Windows DirectShow 验证，但当时场景画面明显模糊。传输和 JPEG 解码通过不代表画质问题已经解决。
本轮架构迁移未重新验证这些画质和传输结果。

## CDC

`CONFIG_WT_BSP_ENABLE_USB_DEVICE_CDC` 启用独立 CDC ACM 串口，使用 J1。
默认与 UVC 互斥；示例应用提供复合 USB profile 后，启用 `CONFIG_WT_BSP_USB_DEVICE_COMPOSITE` 可让两者共用
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
的 `S31 factory` 中开启录像后，离线录制 AVI，SW2 同时暂停/恢复。
工厂示例基准灯色为正常绿色、SD 故障红色、摄像头故障蓝色、两者故障黄色；
SD 与摄像头正常、仅按键回调注册或 CDC 控制台启动失败时为白色。启动时默认点亮，SW2/BOOT 每按一次
切换熄灭/点亮，重新点亮时显示最新状态色；USB 模式和关闭录像时也生效，复位恢复点亮。
BOOT 熄灯不代表硬件正常。完整灯色含义见
[RGB 指示灯](../../../../examples/wt_factory/wt9932s31-tiny/README.md#rgb-指示灯)。
健康暂停仍使用绿色基准色，取卡前查看暂停和文件收尾日志，不能只凭灯色判断。
2026-10-20 用户确认的是分层调整前的 GC2145/录像关闭固件，该历史记录不作为本次迁移的实板验收，细节见
[示例实机记录](../../../../examples/wt_factory/wt9932s31-tiny/README.md#实机验证记录)。
从 USB 模式回到离线模式时，拔掉 J1 后按 SW1/EN 复位。

## 实机验证

以下为分层调整前的历史固件记录，不代表本次分层修改已经通过实板验证。

以下为 2026-09 的历史记录，不代表已完成本次所有故障灯色或 OV3660 的重新验收。

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
