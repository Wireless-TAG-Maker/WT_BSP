# WT BSP

`wt_bsp` 是 Wireless-Tag 板级支持组件。它把板卡差异收敛在 `boards/`，把可复用外设能力收敛在 `include/` 和 `src/`，顶层应用只通过 `wt_bsp.h` 获取已初始化的板卡资源。

## 设计目标

- 应用代码不直接依赖具体板卡引脚。
- 新增板卡时只新增一个 `boards/<BOARD>/` 目录，并接入 Kconfig/CMake。
- 新增通用外设能力时优先沉淀到 `include/` 和 `src/`，板级目录只负责填硬件参数和对象生命周期。
- 功能开关由 `board_config.h` 提供，公共代码通过 `wt_bsp_config_internal.h` 统一转换为 `WT_BSP_*_ENABLE_IS_ENABLED`。

## 目录职责

```text
components/wt_bsp/
├── include/                 # 对外公共接口
├── src/                     # 公共接口实现
├── boards/                  # 具体板卡适配
│   └── <BOARD>/
│       ├── board.c          # 板级初始化、对象持有和接口表
│       ├── board.h          # board_get_bsp_interface 声明
│       ├── board_config.h   # 当前板卡功能开关
│       ├── CMakeLists.txt   # 当前板卡源文件接入
│       └── Kconfig.projbuild
├── third_party/             # 组件内使用的三方库
├── CMakeLists.txt
├── Kconfig.projbuild
└── wt_bsp_config_internal.h
```

当前公共能力包括：

- 板卡信息：`wt_bsp_board_*`
- 按键：`wt_bsp_button_*`
- RGB LED：`wt_bsp_rgb_*`

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
- 应用侧通过 `IDF_TARGET` 或 `menuconfig` 选择目标芯片和板卡，不在业务代码里写板卡条件编译。

### 构建示例

```bash
idf.py -C examples/get-started/bsp_self_test -B build-p4 -DIDF_TARGET=esp32p4 build
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
    bool "<BOARD>"
    depends on IDF_TARGET_<TARGET>
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

5. 添加 `board_config.h`，描述当前板卡启用哪些 BSP 功能：

```c
#define WT_BSP_RGB_ENABLE 1
#define WT_BSP_RGB_NUM 1
#define WT_BSP_BUTTON_ENABLE 1
#define WT_BSP_LCD_ENABLE 0
#define WT_BSP_SDCARD_ENABLE 0
```

6. 实现 `board.c`，至少提供：

- 板卡名称和版本号。
- 按键、RGB 等资源的引脚和型号。
- `board_init()` 和 `board_deinit()`。
- `board_get_board()`、`board_get_button()`、`board_get_rgb()`。
- `board_get_bsp_interface()` 返回静态 `wt_bsp_interface_t`。

### 移植约束

- 板级目录可以知道硬件引脚，公共 `src/` 和应用层不应该知道。
- `board_init()` 负责初始化当前板卡声明启用的资源。
- 初始化中途失败时，需要释放已初始化资源。
- `board_deinit()` 按初始化的反向顺序释放资源。
- 对外对象建议使用静态对象持有，避免应用层管理生命周期。
- 新板卡移植完成后至少编译 `bsp_self_test` 对应目标。

## BSP 维护者视角

BSP 维护者负责维护公共接口、通用驱动和组件结构，目标是让新板卡和新外设能力能稳定扩展。

### 维护公共接口

- 公共接口放在 `include/`，实现放在 `src/`。
- 头文件需要提供 Doxygen 注释，说明参数、返回值和生命周期。
- 公共函数返回 `esp_err_t` 时，错误语义要稳定；获取类函数失败时返回 `NULL`。
- 不把板级引脚、板级型号判断暴露到顶层应用。
- 新增公共能力时，要同步更新 `wt_bsp.h`、`wt_bsp_interface_t` 和 README。

### 新增通用外设能力

以新增 `lcd` 为例，推荐步骤：

1. 添加 `include/wt_bsp_lcd.h` 和 `src/wt_bsp_lcd.c`。
2. 在 `wt_bsp.h` 中包含新头文件。
3. 在 `wt_bsp_interface_t` 中增加 `get_lcd`，并在 `wt_bsp.c` 增加 `wt_bsp_get_lcd()`。
4. 在 `wt_bsp_config_internal.h` 中保留或扩展 `WT_BSP_LCD_ENABLE_IS_ENABLED`。
5. 在需要支持 LCD 的 `board_config.h` 中启用 `WT_BSP_LCD_ENABLE`。
6. 在对应 `board.c` 中持有 LCD 对象、初始化 LCD、实现 `get_lcd`。
7. 给 `examples/get-started/bsp_self_test` 或新增示例补验证路径。

### 维护构建系统

- `components/wt_bsp/CMakeLists.txt` 负责公共源码和三方库接入。
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
| 通用按键/RGB/LCD/SD 卡对象和 API | `include/`、`src/` | 可复用能力 |
| ESP-IDF 后端选择 | `src/` 或三方适配层 | 与 SoC 能力相关，应用不应感知 |
| 顶层业务逻辑 | 应用工程 | 不属于 BSP |
| 三方库源码 | `third_party/` | 与本组件绑定的外部实现 |

## 常见问题

### 应用应该如何判断某个功能是否可用？

调用 `wt_bsp_get_*()` 后检查返回值是否为 `NULL`。需要编译期判断时，可以使用 `WT_BSP_*_ENABLE_IS_ENABLED`，但应用优先使用运行时返回值处理功能缺失。

### 新板卡是否一定要支持所有公共能力？

不需要。`board_config.h` 描述该板卡启用哪些能力；未启用的能力不应该被应用强依赖。

### 可以在应用里直接 include `board_config.h` 吗？

不建议。应用应通过 `wt_bsp.h` 使用公共接口，避免绑定到某块板卡。

### 公共驱动何时需要抽象？

当同类能力被两个以上板卡复用，或者应用需要稳定调用入口时，应沉淀为 `include/` 和 `src/` 中的公共能力。只有单板私有、不会复用的初始化细节才留在 `boards/<BOARD>/board.c`。
