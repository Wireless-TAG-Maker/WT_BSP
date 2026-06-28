# WT BSP

`wt_bsp` 是 Wireless-Tag 板级支持组件。它把板卡差异收敛在 `boards/`，把可复用外设能力收敛在 `features/`，顶层应用只通过 `wt_bsp.h` 获取已初始化的板卡资源。

## 设计目标

- 应用代码不直接依赖具体板卡引脚。
- 新增板卡时只新增一个 `boards/<BOARD>/` 目录，并接入 Kconfig/CMake。
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
│       └── Kconfig.projbuild
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
- 触摸：`wt_bsp_touch_*`

## 顶层应用者视角

顶层应用只需要包含 `wt_bsp.h`，不要包含 `boards/<BOARD>/board.h` 或直接使用板级引脚宏。

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
- RGB 默认支持自动刷新，可通过 `wt_bsp_rgb_set_auto_refresh()` 改为手动刷新。
- 按键事件通过 `wt_bsp_button_register_event_cb()` 注册，回调里只做轻量操作，复杂业务交给任务处理。
- 应用侧通过 `idf.py set-board` 生成板卡选择和目标芯片配置，`menuconfig` 只用于裁剪外设能力，不在业务代码里写板卡条件编译。非交互场景可使用 `WT_BSP_BOARD=<BOARD> idf.py set-board`，也可以直接用 `WT_BSP_BOARD=<BOARD> idf.py build` 构建。

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
config WT_BSP_BOARD_<BOARD_ID>
    bool
    depends on IDF_TARGET_<TARGET>
    select WT_BSP_BOARD_HAS_BOARD
    select WT_BSP_BOARD_HAS_RGB
```

3. 在 `boards/Kconfig.projbuild` 中加入默认选择和 `rsource`：

```kconfig
default WT_BSP_BOARD_<BOARD_ID> if IDF_TARGET_<TARGET>
rsource "<BOARD>/Kconfig.projbuild"
```

4. 添加 `CMakeLists.txt`：

```cmake
if(CONFIG_WT_BSP_BOARD_<BOARD_ID>)
    list(APPEND WT_BSP_BOARD_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}")
    file(GLOB board_sources "${CMAKE_CURRENT_LIST_DIR}/*.c")
    list(APPEND WT_BSP_BOARD_SRCS ${board_sources})
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

### 维护构建系统

- `components/wt_bsp/CMakeLists.txt` 负责收集 board/feature 变量并统一调用一次 `idf_component_register()`。
- `boards/CMakeLists.txt` 自动扫描板级目录，但板级源文件只有在对应 Kconfig 开启时才进入编译。
- SoC 后端差异优先用 ESP-IDF 的能力宏隔离，例如 `CONFIG_SOC_RMT_SUPPORTED`、`CONFIG_SOC_GPSPI_SUPPORTED`。
- 不把某个芯片专用实现强行编进所有目标。

### 维护检查清单

- `git diff --check`
- 至少构建一个 RMT 路径目标，例如 `esp32p4`
- 至少构建一个 SPI 灯带路径目标，例如 `esp32c2` 或 `esp32c61`
- 新增板卡时确认 `menuconfig` 中只在对应 `IDF_TARGET` 下可选
- 新增公共 API 时确认示例或测试覆盖了基本调用链

## 扩展边界

| 需求 | 应放位置 | 原因 |
| --- | --- | --- |
| 新板卡引脚、资源数量、板卡版本 | `boards/<BOARD>/` | 属于硬件差异 |
| 通用按键/RGB/SDMMC/DSI/CSI/touch 对象和 API | `features/<FEATURE>/` | 可复用能力 |
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

当同类能力被两个以上板卡复用，或者应用需要稳定调用入口时，应沉淀为 `include/` 和 `src/` 中的公共能力。只有单板私有、不会复用的初始化细节才留在 `boards/<BOARD>/board.c`。
