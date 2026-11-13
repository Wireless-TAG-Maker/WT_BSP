# JPEG 相机能力

`camera` 提供独立的 JPEG 采集能力。应用通过 `wt_bsp.h` 获取
`wt_bsp_get_camera()`，检查句柄后调用 `wt_bsp_camera_prepare()`、
`wt_bsp_camera_capture()` 和 `wt_bsp_camera_get_status()`。
`CONFIG_WT_BSP_ENABLE_CAMERA` 依赖板卡的 `WT_BSP_BOARD_HAS_CAMERA`，与 USB UVC 分别启用。
相机裁剪开关还依赖 `SOC_LCDCAM_CAM_SUPPORTED` 和 `SOC_JPEG_ENCODE_SUPPORTED`
两项 SoC 能力；缺少任一能力时不启用该 feature，获取句柄返回 `NULL`。

## 实现职责

| 位置 | 职责 |
| --- | --- |
| `wt_bsp_camera.c` | 公共入口、参数检查和对象生命周期 |
| `interface/wt_bsp_camera_interface.c` | 按传感器名称查找注册驱动，取得统一相机操作表 |
| `backends/wt_bsp_camera_dvp.c` | DVP/V4L2 采集、JPEG 后端、缓冲、互斥和健康状态 |
| `include/wt_bsp_camera_sensor.h` | 传感器模块与 DVP 后端之间的内部模式操作接口 |
| `sensors/gc2145/` | GC2145 UXGA UYVY 模式扩展及 720p 原生 DVP 采集 |
| `sensors/ov3660/` | OV3660 原生 JPEG 和曝光、色彩、细节参数调节 |
| `boards/<BOARD>/` | 传感器型号、物理接线、工作格式、资源预算和静态对象持有 |

GC2145 UXGA 和 OV3660 使用 `esp_cam_sensor`。GC2145 720p 使用原生 DVP 采集和硬件 JPEG。
camera feature 收集自身的源文件和依赖，
通用实现不读取板级私有头文件、板名或板级 menuconfig 符号。
GC2145 UXGA 基于驱动提供的时序表，选择 UYVY 输出与时钟分频，保留其他寄存器设置。
调用链为板级传入统一 `wt_bsp_camera_info_t`，公共初始化入口交给 interface 按
`sensor_name` 查找注册项，再由驱动 factory 返回统一的 `wt_bsp_camera_ops_t`。
公共相机 API 通过该操作表调用 `deinit`、`prepare`、`capture` 和 `get_status`，
无需了解传感器型号或 DVP 后端。传感器模式 hook 仅用于模块与后端之间。

## 板级接入

板级通过 `wt_bsp_camera_info_t` 向 `wt_bsp_camera_init()` 传入配置值：

- `sensor_name` 字符串、通用整数属性数组、输出尺寸和标称帧率。
- DVP 八位数据线、同步线、SCCB、RESET/PWDN 和 XCLK。
- 采集缓冲数量、JPEG 容量、启动丢帧数量和超时。
- 硬件 JPEG 质量，以及由具体传感器模块解释的图像调节参数。

板级持有零初始化的静态 `wt_bsp_camera_obj_t`，由 feature 管理内部采集和编码资源。
板级初始化和释放分别调用 `wt_bsp_camera_init()`、`wt_bsp_camera_deinit()`，
无需提供 `prepare`、`capture` 或 `get_status` 回调。
板级使用公共配置类型，传入 `"gc2145"` 或 `"ov3660"` 等注册名称。
名称字符串、`properties` 数组及其 `key` 字符串均被 camera 借用，板级应使用静态数据，
保持其内容不变，直到反初始化成功。
S31 仍使用本板的传感器 choice 和现有两份传感器 Kconfig 选择驱动及模式。

下面仅展示 OV3660 的驱动选择和属性部分；完整配置还必须填写本板的
`format`、`dvp`、缓冲预算及超时字段：

```c
static const wt_bsp_camera_property_t s_camera_properties[] = {
    { .key = "ae_level", .value = -4 },
    { .key = "jpeg_quality", .value = 34 },
    { .key = "saturation", .value = 0 },
    { .key = "sharpness", .value = 2 },
    { .key = "denoise", .value = 4 },
};

static const wt_bsp_camera_info_t s_camera_info = {
    .sensor_name = "ov3660",
    .properties = s_camera_properties,
    .property_count = sizeof(s_camera_properties) / sizeof(s_camera_properties[0]),
    /* Fill this board's format, DVP wiring, buffer budget and timeouts. */
};
```

OV3660 当前要求提供上述五项属性，以保留现有板级图像配置；属性名必须唯一，
具体值的范围由驱动检查。GC2145 当前使用 `properties = NULL`、`property_count = 0`。

传感器探测默认支持 `CAMERA_SENSOR_MOTOR_DETECT_METHOD_DYNAMIC_LINK` 模式。
所选传感器的 `AUTO_DETECT_DVP_INTERFACE_SENSOR` 为 `y` 时使用依赖驱动的注册项，
为 `n` 时由 camera feature 注册探测函数。
`CAMERA_SENSOR_MOTOR_DETECT_METHOD_STATIC_STORE` 与所选传感器的该自动探测项为 `n`
的组合不受支持，后端初始化会明确返回 `ESP_ERR_NOT_SUPPORTED`。

## 新增传感器模块

以下步骤适用于当前 DVP/JPEG 采集后端。

1. 在 `sensors/<SENSOR>/` 新建自己的 `.h` 和 `.c`。头文件声明驱动 factory，
   所有 factory 采用相同签名：

   ```c
   esp_err_t camera_sensor_factory(wt_bsp_camera_t camera,
                                   const wt_bsp_camera_info_t *info,
                                   const wt_bsp_camera_ops_t **ops);
   ```

2. 在 `.c` 中解析统一配置和属性，验证模式及探测支持，初始化私有资源并返回统一操作表。
   可以复用 DVP 后端和模块内部的传感器模式 hook；初始化失败时释放本次分配的资源。
   在同一 `.c` 内使用 `WT_BSP_CAMERA_DRIVER_REGISTER(id, "model", factory)` 自行注册。
3. 需要 feature 提供探测注册时，在该 `.c` 内使用 `ESP_CAM_SENSOR_DETECT_FN`，
   通过 `wt_bsp_camera_sensor_is_selected()` 判断当前选择，避免同 SCCB 地址的其他型号被误选。
   按模块自己的驱动或构建配置条件编译实现与注册项。
4. 在使用该模块的板级目录中填写 `sensor_name`、通用属性和硬件参数，设置所需驱动的 Kconfig。
   板级只使用公共配置类型，无需包含具体传感器头文件。底层驱动必须已由依赖提供，
   或由该模块自行实现；两个空文件不会形成可用驱动。

### Factory 与操作表契约

注册的 `id` 必须是唯一的 C 标识符，`"model"` 名称必须在所有注册项中唯一，
并与板级 `info.sensor_name` 完全一致。factory 只验证统一配置、准备私有上下文和同步资源，
不探测传感器、不开始采集；硬件准备延迟到 `prepare()` 或首次 `capture()`。
私有状态由驱动或复用的后端通过 `camera->backend` 管理，板级和应用不解释其结构，
也不直接调用具体传感器的 factory。

factory 成功时通过 `*ops` 返回具有静态生命周期的完整 `wt_bsp_camera_ops_t`。
以下四个成员均不能为 `NULL`，也不能返回栈上的临时操作表：

```c
static const wt_bsp_camera_ops_t s_camera_ops = {
    .deinit = sensor_deinit,
    .prepare = sensor_prepare,
    .capture = sensor_capture,
    .get_status = sensor_get_status,
};
```

这里的函数名仅示意模块需要实现的操作。`prepare` 成功后返回选定格式；`capture`
同步借出 JPEG 并传递消费者返回值；`get_status` 返回状态快照，未准备硬件时 `ready` 为 `false`。
factory 也可以返回复用后端提供的静态操作表，无需为每款传感器重复实现采集管线。
例如 [GC2145](sensors/gc2145/wt_bsp_camera_gc2145.c) 和
[OV3660](sensors/ov3660/wt_bsp_camera_ov3660.c) 验证各自配置后调用
`wt_bsp_camera_dvp_init()`，由 DVP 后端返回统一相机操作表。
它们提供的 `wt_bsp_camera_sensor_ops_t` 是模块内部模式 hook，与上层的
`wt_bsp_camera_ops_t` 职责不同，不传给板级或应用。

factory 返回失败前必须释放本次分配的资源，不能把半初始化上下文留给公共入口；
只有成功时才写入输出操作表。反初始化前先停止消费者；`deinit` 在释放私有资源并清除
`camera->backend` 后才返回 `ESP_OK`。如果底层仍借用缓冲或清理失败，应返回错误并保留尚在使用的资源、
上下文及可重试状态，不能提前释放或清空所有权。静态操作表必须在重试期间仍然有效。

camera 的 CMake 通过 `GLOB_RECURSE CONFIGURE_DEPENDS` 自动收集 `sensors/` 下的 `.c`
及传感器头文件所在目录。构建时自动读取每个源文件行首的注册宏调用（参数允许换行），提取注册 ID，
通过对应的 `-u wt_bsp_camera_driver_<id>` 锚点保留该 sensor 对象，
配合链接段的 `KEEP` 保留注册项。无需整体拉入 BSP 静态库，新增模块也无需维护锚点列表。
完成上述实现和板级接入后，无需修改 camera 公共头文件、interface、后端、中央型号表或公共 CMake。
只有需要额外菜单时才添加模块自己的 `Kconfig.projbuild`，公共 Kconfig 使用可选通配符加载；
模块无需额外的 CMake 文件。

## 帧与资源生命周期

初始化只准备对象和同步资源，传感器检测延迟到 `prepare()` 或首次 `capture()`。
`capture()` 在调用任务中同步调用消费者，JPEG 数据只在回调内有效，回调返回后归还缓冲。
应用需要异步使用帧时应自行复制，且不得从帧回调重入相机或 BSP 生命周期函数。

释放时先停止应用消费者，再释放采集设备、传感器和编码资源。
GC2145 模式描述和寄存器表被驱动借用，必须等视频设备和传感器成功销毁后才可释放。
清理失败时保留尚在使用的资源和相机对象，以便应用重试。

S31 的板级配置按 16 MB PSRAM 预算设置：GC2145 UXGA 使用一个 DVP 用户缓冲，
另保留驱动备用缓冲，为应用 JPEG 快照留出空间。GC2145 720p 和 OV3660 使用两个用户缓冲。
缓冲调整后的吞吐仍需实机确认。模拟器可覆盖初始化和接口路由，采集、编码及 USB
视频传输仍需连接实际硬件验证。

## 与 UVC 的关系

相机采集不检查 USB 状态。USB 描述符、JPEG 转交和录像/推流消费者优先级由应用决定。
仓库示例通过 [`wt_camera_uvc`](../../../../examples/common_components/wt_camera_uvc/README.md)
连接相机与独立 UVC feature，并在成功停止桥接后再释放 BSP。
