# WT BSP

`wt_bsp` 是 Wireless-Tag 板级支持组件。它把板卡差异收敛在 `boards/`，把可复用外设能力收敛在 `features/`，顶层应用只通过 `wt_bsp.h` 获取已初始化的板卡资源。

## 版本要求

`wt_bsp` 仅支持 ESP-IDF `v6.0.0` 及以上版本。组件管理器会根据 `idf_component.yml` 中的 `idf >=6.0.0` 约束检查项目使用的 ESP-IDF 版本。

WT9932S31-TINY 需要 ESP-IDF v6.1 及以上版本，v6.1 命令需加 `--preview`。
ESP-IDF 6.0 下，清单为 P4 显示/触摸选择 `esp_lvgl_port 2.8.*`，并在相关显示/视频功能
引入 H.264 依赖时固定 `esp_h264 1.3.0`，规避后续版本调用该 SDK 尚未提供的 API。
6.1 及以上保留原来的 LVGL `^2` 选择，不施加该 H.264 兼容约束。

## 设计目标

- 应用代码不直接依赖具体板卡引脚。
- 新增板卡时只新增一个 `boards/<BOARD>/` 目录及其 Kconfig/CMake 文件，无需修改公共板卡列表。
- 新增通用外设能力时优先沉淀到 `features/<FEATURE>/`，板级目录只负责填硬件参数和对象生命周期。
- 板卡能力由移植者通过 `WT_BSP_BOARD_HAS_<FEATURE>` 和 `WT_BSP_BOARD_FEATURES` 声明，应用通过 `CONFIG_WT_BSP_ENABLE_<FEATURE>` 在 menuconfig 中裁剪。
- 公共代码只使用 `wt_bsp_config_internal.h` 合成后的 `WT_BSP_<FEATURE>_ENABLED`。

## 目录职责

```text
components/wt_bsp/
├── include/                 # 顶层聚合头文件
├── src/                     # BSP 核心公共实现
├── features/                # 可复用外设能力
│   └── <FEATURE>/
│       ├── CMakeLists.txt
│       ├── Kconfig.projbuild
│       ├── include/
│       │   ├── wt_bsp_<feature>.h
│       │   └── wt_bsp_<feature>_port.h
│       └── wt_bsp_<feature>.c
├── boards/                  # 具体板卡适配
│   └── <BOARD>/
│       ├── board.c          # 板级初始化、对象持有和接口表
│       ├── board.h          # board_get_bsp_interface 声明
│       ├── board_config.h   # 当前板卡硬件能力边界
│       ├── CMakeLists.txt   # 当前板卡源文件接入
│       ├── Kconfig.projbuild # 板卡选项、默认选择和能力
│       └── Kconfig.options  # 可选的额外板级设置
├── tools/                   # set-board 和项目 CMake 辅助脚本
│   ├── wt_bsp_project.cmake
│   └── wt_bsp_set_board.py
├── CMakeLists.txt
├── Kconfig.projbuild
└── wt_bsp_config_internal.h
```

当前公共能力包括：

- 板卡信息：`wt_bsp_board_*`
- 按键：`wt_bsp_button_*`
- RGB LED：`wt_bsp_rgb_*`
- SDMMC：`wt_bsp_sdmmc_*`
- DSI 显示：`wt_bsp_dsi_*`
- CSI 摄像头：`wt_bsp_csi_*`
- USB CDC：`wt_bsp_get_usb_device_cdc()`、`wt_bsp_usb_device_cdc_read()`、`wt_bsp_usb_device_cdc_write()`
- USB UVC：`wt_bsp_get_usb_device_uvc()`、`wt_bsp_usb_device_uvc_start()`、`wt_bsp_usb_device_uvc_stop()`
- 本地 JPEG：`wt_bsp_get_camera()`、`wt_bsp_camera_prepare()`、`wt_bsp_camera_capture()`、`wt_bsp_camera_get_status()`
- 触摸：`wt_bsp_touch_*`

USB CDC 支持 WT9932P4-TINY、WT9932P4X-TINY、WT9932P4C61-TINY 的 HUSB，以及 WT9932S31-TINY 的 J1。
四个板卡共用 `features/usb_device_cdc` 实现，板级持有对象并管理初始化/释放。
配置、接线和构建命令见 [CDC 回显示例](../../examples/usb/device_cdc/README_CN.md)。

CSI/JPEG 相机和 USB UVC 是独立能力。`wt_bsp_init()` 准备各自启用的对象，不自动把摄像头连接到 USB。
摄像头转 USB 的源选择、JPEG 转交和缓冲由应用组件
[`wt_camera_uvc`](../../examples/common_components/wt_camera_uvc/README.md) 管理。
应用显式调用 `camera_uvc_start()`，并在成功调用 `camera_uvc_stop()` 后再反初始化 BSP。

JPEG 相机的采集、缓冲、互斥、状态和资源释放由 [`features/camera`](features/camera/README.md) 实现，
其 DVP 后端和传感器模式扩展可供其他板卡复用。板级通过 `wt_bsp_camera_info_t` 提供引脚、
SCCB/XCLK、`sensor_name`、通用整数属性、格式及缓冲预算，并持有静态对象，无需包含具体传感器头文件。
interface 按名称查找驱动的自注册项，由其 factory 接收统一配置并返回统一相机操作表，公共 API 经该表调用。
新增传感器时在 `features/camera/sensors/` 添加自行注册 factory 的 `.c/.h`，由使用它的板级填写名称和属性，
设置所需驱动配置。源文件和头文件目录自动收集，无需修改中央型号表或公共 camera 文件；额外传感器菜单可选。

## 顶层应用者视角

顶层应用只需要包含 `wt_bsp.h`，不要包含 `boards/<BOARD>/board.h` 或直接使用板级引脚宏。

### 自有项目接入

将本仓库的 `components/wt_bsp` 复制到项目同名位置后，在项目 `CMakeLists.txt`
的 `project.cmake` 之前接入选板辅助工具：

```cmake
cmake_minimum_required(VERSION 3.16)
include("${CMAKE_CURRENT_LIST_DIR}/components/wt_bsp/tools/wt_bsp_project.cmake")
wt_bsp_apply_sdkconfig_defaults()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_app)
```

为项目实际适配的板卡新增 `sdkconfig.<board_id>` 预设，可从对应仓库示例开始调整，
随后运行 `idf.py set-board`、`idf.py build`。选板工具从项目父目录查找
`components/wt_bsp/boards`；仅通过外部 `EXTRA_COMPONENT_DIRS` 引用 BSP、且项目不在
该目录树中时，使用 ESP-IDF 原生目标/Kconfig 选板流程。原生流程先配置正确的
`IDF_TARGET`，再选择匹配板卡；跨芯片切换仍需切换目标并清理旧构建。

### 基本流程

```c
#include "wt_bsp.h"

void app_main(void)
{
    ESP_ERROR_CHECK(wt_bsp_init());

    wt_bsp_board_t board = wt_bsp_get_board();
    wt_bsp_button_t button = wt_bsp_get_button();
    wt_bsp_rgb_t rgb = wt_bsp_get_rgb();

    (void)board;
    (void)button;
    (void)rgb;
}
```

### 使用建议

- 先调用 `wt_bsp_init()`，再调用 `wt_bsp_get_*()`。
- `wt_bsp_get_*()` 可能返回 `NULL`，应用需要处理功能未启用或初始化失败的情况。
- 生命周期调用由应用串行协调。先成功停止采集消费者和应用桥接，再反初始化 BSP；停止/清理失败时保留仍被借用的对象和依赖，处理错误后重试，不应提前释放回调缓冲。
- RGB 默认支持自动刷新，可通过 `wt_bsp_rgb_set_auto_refresh()` 改为手动刷新。
- 按键事件通过 `wt_bsp_button_register_event_cb()` 注册，回调里只做轻量操作，复杂业务交给任务处理。
- 保留 `idf.py set-board` 再 `idf.py build` 的流程；非交互选板可使用 `WT_BSP_BOARD=<BOARD> idf.py set-board`，无需额外的 `idf_ext.py`。
- 已配置目标芯片后，也可在 `menuconfig` 的 `WT BSP -> Wireless-Tag board` 中选择匹配板卡，或在 `sdkconfig.defaults` 中设置对应的 `CONFIG_WT_BSP_BOARD_*`。外设能力可独立裁剪；跨目标芯片切换仍使用 `set-board`。

### 构建示例

```bash
WT_BSP_BOARD=WT9932P4-TINY idf.py -C examples/get-started/blink set-board
idf.py -C examples/get-started/blink build
```

## 底层移植者视角

底层移植者负责把一块新板卡接入 `wt_bsp`，核心工作是提供板级目录和 `wt_bsp_interface_t`。

### 新增板卡步骤

1. 新建目录：

```text
components/wt_bsp/boards/<BOARD>/
```

2. 添加 `Kconfig.projbuild`：

```kconfig
config WT_BSP_SELECTED_BOARD_<BOARD_ID>  # ignore: multiple-definition
    bool

choice WT_BSP_BOARD  # ignore: multiple-definition
    default WT_BSP_BOARD_<BOARD_ID> if WT_BSP_SELECTED_BOARD_<BOARD_ID>
    default WT_BSP_BOARD_<BOARD_ID> if !WT_BSP_BOARD_SELECTED_BY_TOOL && IDF_TARGET_<TARGET>

    config WT_BSP_BOARD_<BOARD_ID>
        bool "<BOARD>"
        depends on IDF_TARGET_<TARGET>
        select WT_BSP_BOARD_HAS_BOARD
        select WT_BSP_BOARD_HAS_RGB
endchoice
```

3. 公共 `boards/Kconfig.projbuild` 通过通配符自动载入各板的 `Kconfig.projbuild` 和可选 `Kconfig.options`，无需新增 `rsource`、板名或目标芯片条件。每块板在自己的命名 `choice WT_BSP_BOARD` 中贡献成员和默认值；同一目标有多块板时，只为默认板保留目标芯片回退的 `default`。额外传感器等设置放入该板的 `Kconfig.options`，并依赖对应 `WT_BSP_BOARD_*`，保持在选板 choice 之外。

4. 添加 `CMakeLists.txt`：

```cmake
if(CONFIG_WT_BSP_BOARD_<BOARD_ID>)
    list(APPEND WT_BSP_BOARD_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}")
    file(GLOB board_sources "${CMAKE_CURRENT_LIST_DIR}/*.c")
    list(APPEND WT_BSP_BOARD_SRCS ${board_sources})
    list(APPEND WT_BSP_BOARD_FEATURES board rgb)
endif()
```

5. 添加 `board_config.h`，描述当前板卡硬件具备哪些 BSP 能力：

```c
#define WT_BSP_BOARD_HAS_RGB 1
#define WT_BSP_RGB_NUM 1
#define WT_BSP_BOARD_HAS_BUTTON 1
#define WT_BSP_BOARD_HAS_SDMMC 0
```

6. 实现 `board.c`，至少提供：

- 板卡名称和版本号。
- 按键、RGB 等资源的引脚和型号。
- `board_init()` 和 `board_deinit()`。
- `board_get_board()`、`board_get_button()`、`board_get_rgb()`。
- `board_get_bsp_interface()` 返回静态 `wt_bsp_interface_t`。

7. 在需要支持该板的示例目录下添加 `sdkconfig.<board_id>`，例如：

```ini
CONFIG_WT_BSP_BOARD_<BOARD_ID>=y
```

`idf.py set-board` 会读取这个文件生成 `sdkconfig.board` 和 `sdkconfig.board.Kconfig`。CMake 配置阶段会读取 `sdkconfig.board` 设置 `IDF_TARGET`；active `SDKCONFIG` 默认放在 build 目录下，不同 build 目录可以分别保存 menuconfig 裁剪结果。如果 active `SDKCONFIG` 已存在且目标芯片不同，旧文件会在同目录备份为 `sdkconfig.old` 后重置目标行。如果选择的新板卡目标芯片与当前 build 目录的 `CMakeCache.txt` 不一致，`set-board` 会自动执行 `idf.py fullclean`，避免下次构建出现 target mismatch。

### 移植约束

- 板级目录可以知道硬件引脚，公共 feature 实现和应用层不应该知道。
- `board_init()` 负责初始化当前工程有效启用的资源。
- 初始化中途失败时，需要释放已初始化资源。
- `board_deinit()` 按初始化的反向顺序释放资源。
- 对外对象建议使用静态对象持有，避免应用层管理生命周期。
- 新板卡移植完成后至少编译一个该板对应示例。

## BSP 维护者视角

BSP 维护者负责维护公共接口、通用驱动和组件结构，目标是让新板卡和新外设能力能稳定扩展。

### 维护公共接口

- 顶层公共入口放在 `include/`，可复用外设能力放在 `features/<FEATURE>/`。
- 头文件需要提供 Doxygen 注释，说明参数、返回值和生命周期。
- 公共函数返回 `esp_err_t` 时，错误语义要稳定；获取类函数失败时返回 `NULL`。
- 不把板级引脚、板级型号判断暴露到顶层应用。
- 新增公共能力时，要同步更新 `wt_bsp.h`、`wt_bsp_interface_t` 和 README。

### 新增通用外设能力

以新增 `audio` 为例，推荐步骤：

1. 新建 `features/audio/`。
2. 添加 `CMakeLists.txt`、`Kconfig.projbuild`、`include/wt_bsp_audio.h`、`include/wt_bsp_audio_port.h` 和 `wt_bsp_audio.c`。
3. 在 `wt_bsp.h` 中包含公共头，在 `wt_bsp_port.h` 中包含 port 头。
4. 在 `wt_bsp_interface_t` 中增加 `get_audio`，并在 `wt_bsp.c` 增加 `wt_bsp_get_audio()`。
5. 在 `wt_bsp_config_internal.h` 中增加 `WT_BSP_AUDIO_ENABLED`，语义为板卡具备且 menuconfig 启用。
6. 在对应 `board_config.h`、板级 `Kconfig.projbuild`、板级 `CMakeLists.txt` 和 `board.c` 中接入 audio 能力。
7. 给现有示例或新增示例补验证路径。

若新能力需要按 Kconfig 控制托管依赖，在自己的 Kconfig 中定义始终导出的隐藏字符串，
例如 `WT_BSP_DEP_AUDIO`，按有效启用状态默认取 `"y"` 或 `"n"`。不可见的 bool
可能不进入 ESP-IDF 的 `sdkconfig.json`，不能假定其始终以 `false` 存在。
清单沿用 `matches` 和下面的引号形式，并对多个布尔项显式嵌套括号：

```yaml
matches:
  - if: '$CONFIG{WT_BSP_DEP_AUDIO} == "y"'
```

用真实 Kconfig JSON 序列化结果和 Component Manager 解析器验证启用、关闭、
板卡不具备能力及目标/SDK 条件。现有回归入口是 `tools/tests/test_dependency_kconfig_exports.py`。
没有托管依赖条件的纯源码能力无需额外增加该字符串。

### 维护构建系统

- `components/wt_bsp/CMakeLists.txt` 负责收集 board/feature 变量并统一调用一次 `idf_component_register()`。
- `boards/CMakeLists.txt` 自动扫描板级目录，但板级源文件只有在对应 Kconfig 开启时才进入编译。
- 各板的 Kconfig 默认值、目标依赖和额外设置均由本板目录提供，公共收集器不枚举板名或目标芯片。
- SoC 后端差异优先用 ESP-IDF 的能力宏隔离，例如 `CONFIG_SOC_RMT_SUPPORTED`、`CONFIG_SOC_GPSPI_SUPPORTED`。
- 不把某个芯片专用实现强行编进所有目标。
- 应用桥接自己的 CSI/V4L2 头文件依赖由 `examples/common_components/wt_camera_uvc/idf_component.yml` 以私有 `esp_video` 依赖声明；不要在 ESP-IDF 早期依赖扫描尚无 Kconfig 值时，仅用条件 `PRIV_REQUIRES` 声明它。

### 维护检查清单

- `git diff --check`
- 至少构建一个 RMT 路径目标，例如 `esp32p4`
- 至少构建一个 SPI 灯带路径目标，例如 `esp32c2` 或 `esp32c61`
- 新增板卡时确认 `menuconfig` 中只在对应 `IDF_TARGET` 下可选
- 新增公共 API 时确认示例或测试覆盖了基本调用链
- 配置回归及完整矩阵方法、2026-11-04 结果和模拟器边界见[验证记录](../../docs/bsp-validation.md)。

## 扩展边界

| 需求 | 应放位置 | 原因 |
| --- | --- | --- |
| 新板卡引脚、资源数量、板卡版本 | `boards/<BOARD>/` | 属于硬件差异 |
| 通用按键/RGB/SDMMC/DSI/CSI/UVC/touch 对象和 API | `features/<FEATURE>/` | 可复用能力 |
| CSI/JPEG 到 UVC 的桥接、USB 描述符组合和消费优先级 | 应用工程 | 连接独立能力的应用策略 |
| ESP-IDF 后端选择 | `features/<FEATURE>/` 或 BSP 内部适配层 | 与 SoC 能力相关，应用不应感知 |
| 顶层业务逻辑 | 应用工程 | 不属于 BSP |

## 常见问题

### 应用应该如何判断某个功能是否可用？

调用 `wt_bsp_get_*()` 后检查返回值是否为 `NULL`。需要编译期判断时，可以使用 `WT_BSP_*_ENABLED`，但应用优先使用运行时返回值处理功能缺失。

### 新板卡是否一定要支持所有公共能力？

不需要。`board_config.h` 描述该板卡具备哪些能力；应用只能在这些能力边界内通过 menuconfig 启用或关闭。

### 可以在应用里直接 include `board_config.h` 吗？

不建议。应用应通过 `wt_bsp.h` 使用公共接口，避免绑定到某块板卡。

### 公共驱动何时需要抽象？

当同类能力被两个以上板卡复用，或者应用需要稳定调用入口时，应沉淀到 `features/<FEATURE>/`，由该 feature 提供公共头和实现。只有单板私有、不会复用的初始化细节才留在 `boards/<BOARD>/board.c`。

### WT9932S31-TINY

S31 使用 ESP-IDF v6.1 及以上版本，v6.1 的选板和构建命令需加 `--preview`。板级配置 DVP 引脚、传感器和资源预算，`features/camera` 负责采集与 JPEG 输出，`wt_bsp_get_csi()` 返回 `NULL`。独立相机 API 在调用任务中借出 JPEG 帧，回调返回后归还，不检查 USB 状态；录像和推流优先级由应用管理。UVC 通过公共 feature 提供，示例应用负责 JPEG 桥接和 USB profile。CDC 可独立使用，也可在应用提供复合描述符并启用 `CONFIG_WT_BSP_USB_DEVICE_COMPOSITE` 时与 UVC 共用 USB 栈。详细引脚、生命周期和示例见[板卡说明](boards/WT9932S31-TINY/README.md)。

### WT9932P4X-TINY

P4X 对应 ESP32-P4 v3.x，原 WT9932P4-TINY 对应 v1.x。两者使用相同外设和引脚，
P4X 作为选板工具中的配置变体，直接复用原 P4 的板级实现和 Kconfig 选择。
运行时 BSP 板名仍为 `WT9932P4-TINY`，芯片版本由独立 sdkconfig 区分。
所有支持原 P4 的示例均提供 `sdkconfig.wt9932p4x_tiny`，包括原 P4 工厂示例。
`idf.py set-board` 会切换芯片版本及默认 CPU 频率；重新构建、烧录后才生效。
详见[P4X 板卡说明](boards/WT9932P4X-TINY/README.md)。
