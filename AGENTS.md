# AGENTS.md

本文件给在本仓库工作的 AI 编码代理使用。开始修改前先阅读本文件，并以现有代码和 README 为准。

## 项目概览

这是 Wireless-Tag 的 ESP-IDF Board Support Package 仓库。核心组件位于 `components/wt_bsp`，用于给多块 Wireless-Tag TINY 系列 ESP32 开发板提供统一的 `wt_bsp_*` API。示例工程位于 `examples`。

推荐 ESP-IDF 版本为 `v6.0.1`，组件声明兼容 `idf >=5.3`。

## 目录职责

- `components/wt_bsp/include/`：对外公共 API 头文件。
- `components/wt_bsp/src/`：公共 API 的通用实现。
- `components/wt_bsp/boards/<BOARD>/`：单板适配，包括引脚、资源数量、板卡初始化、对象生命周期、Kconfig 和 CMake 接入。
- `components/wt_bsp/wt_bsp_config_internal.h`：把各板 `board_config.h` 中的功能开关转换为公共编译期宏。
- `components/wt_bsp/idf_component.yml`：ESP-IDF 组件依赖声明。
- `examples/get-started/blink/`：基础 RGB 示例。
- `examples/display/dsi/`：ESP32-P4 DSI/LVGL 示例。
- `examples/camera/csi/`：ESP32-P4 CSI 摄像头示例。
- `examples/storage/sdmmc/`：ESP32-P4 SDMMC 示例。
- `examples/wt_factory/*/`：综合工厂测试固件。

## 构建和验证

先确保已经加载 ESP-IDF 环境，例如：

```bash
. /path/to/esp-idf/export.sh
```

常用验证命令：

```bash
idf.py -C examples/get-started/blink -B build-esp32c2 -DIDF_TARGET=esp32c2 build
idf.py -C examples/get-started/blink -B build-esp32c3 -DIDF_TARGET=esp32c3 build
idf.py -C examples/get-started/blink -B build-esp32c5 -DIDF_TARGET=esp32c5 build
idf.py -C examples/get-started/blink -B build-esp32c61 -DIDF_TARGET=esp32c61 build
idf.py -C examples/display/dsi -B build-esp32p4 -DIDF_TARGET=esp32p4 build
idf.py -C examples/camera/csi -B build-esp32p4 -DIDF_TARGET=esp32p4 build
idf.py -C examples/storage/sdmmc -B build-esp32p4 -DIDF_TARGET=esp32p4 build
idf.py -C examples/wt_factory/wt9932p4-tiny -B build-esp32p4 -DIDF_TARGET=esp32p4 build
idf.py -C examples/wt_factory/wt9932p4c61-tiny -B build-esp32p4 -DIDF_TARGET=esp32p4 build
```

提交前至少运行：

```bash
git diff --check
```

如果改动只影响某一类外设，优先构建覆盖该外设的示例；如果改动公共接口、Kconfig、CMake 或 `wt_bsp_config_internal.h`，至少覆盖一个非 P4 目标和一个 P4 目标。

不要提交 `build/`、`examples/**/build*`、`examples/**/sdkconfig`、`managed_components/`、`dependencies.lock`、IDE 配置和系统临时文件。

## 架构约束

- 应用层只能通过 `#include "wt_bsp.h"` 使用 BSP，不直接 include `boards/<BOARD>/board.h` 或 `board_config.h`。
- 板级引脚、硬件资源数量、屏幕/摄像头型号等硬件差异只放在 `components/wt_bsp/boards/<BOARD>/`。
- 可被多块板复用的外设能力应沉淀到 `include/` 和 `src/`，不要复制到多个板级目录。
- `wt_bsp_init()` 获取当前 Kconfig 选中的 `board_get_bsp_interface()` 并调用板级 `init()`。
- `wt_bsp_get_*()` 可能返回 `NULL`。示例和应用必须处理功能未启用、初始化失败或硬件缺失的情况。
- `board_init()` 初始化中途失败时要释放已经初始化的资源；`board_deinit()` 按初始化的反向顺序释放。
- 对外对象优先由板级静态对象持有，避免让应用管理 BSP 资源生命周期。
- SoC 差异优先用 ESP-IDF 配置或能力宏隔离，例如 `IDF_TARGET_*`、`CONFIG_SOC_*`，不要把专用实现强行编入所有目标。

## 新增板卡流程

1. 新建 `components/wt_bsp/boards/<BOARD>/`。
2. 添加 `board.c`、`board.h`、`board_config.h`、`CMakeLists.txt`、`Kconfig.projbuild`。
3. 在 `components/wt_bsp/boards/Kconfig.projbuild` 中添加 `rsource`，并在合适的 `IDF_TARGET` 下设置默认选择。
4. 在板级 `CMakeLists.txt` 中仅当对应 `CONFIG_WT_BSP_BOARD_*` 启用时追加源文件和 include 目录。
5. 在 `board_config.h` 中声明该板启用的功能，例如 RGB、button、SDMMC、DSI、CSI、touch。
6. 在 `board.c` 中实现板卡信息、资源初始化/释放、`get_*` 函数和 `board_get_bsp_interface()`。
7. 编译至少一个该板对应的示例，并确认 `menuconfig` 中该板只在正确目标芯片下可选。

## 新增公共外设能力

1. 新增 `include/wt_bsp_<feature>.h` 和 `src/wt_bsp_<feature>.c`。
2. 在 `include/wt_bsp.h` 中包含新头文件。
3. 扩展 `wt_bsp_interface_t`，添加对应 `get_<feature>` 函数指针。
4. 在 `src/wt_bsp.c` 中添加 `wt_bsp_get_<feature>()`，并保持未初始化或不可用时的返回语义稳定。
5. 在 `wt_bsp_config_internal.h` 中加入或扩展对应 `WT_BSP_<FEATURE>_ENABLE_IS_ENABLED`。
6. 在需要支持该能力的 `board_config.h` 和 `board.c` 中接入资源。
7. 同步更新 README、示例或测试路径。

## 代码风格

- 主要代码为 C，沿用现有 ESP-IDF 风格和文件结构注释。
- 公共头文件的新增 API 要写清 Doxygen 注释，说明参数、返回值和生命周期。
- 返回 `esp_err_t` 的公共函数要保持错误语义稳定；获取类函数失败时返回 `NULL`，字符串获取失败时沿用现有空字符串或默认值约定。
- 日志使用 `ESP_LOG*`，tag 使用当前模块名。
- 回调中只做轻量操作，耗时逻辑交给任务处理。
- LVGL API 调用需要遵循示例中的 lock/unlock 约定。
- 除非同一文件已有明确不同风格，否则新代码使用 ASCII 标点和空格。

## C/H 规范整理

以下规范以当前 `components/wt_bsp` 中的 `.c`、`.h` 文件为基准。新增代码优先遵守本节；整理旧代码时只做与当前改动相关的局部修正，不为无关文件做大范围格式化。

### 文件结构

- `.c` 和 `.h` 文件保留现有文件头注释格式，至少包含 `@file`、`@author`、`@brief`、`@version`、`@date` 和版权信息。
- 文件内分区使用现有横幅注释，顺序保持一致：
  1. `Includes`
  2. `Defines`
  3. `Typedefs`
  4. `Static Prototypes`（仅 `.c` 需要）
  5. `Static Variables`（仅 `.c` 需要）
  6. `Macros`
  7. `Global Functions` 或 `Global Prototypes`
  8. `Static Functions`（仅 `.c` 需要）
- 空分区可以保留，便于不同文件保持结构统一。
- `#include` 顺序：本模块头文件优先，例如 `.c` 先 include 对应 `.h` 或 `board.h`；再 include BSP 内部头；最后 include ESP-IDF、驱动、标准库头。不同类别之间可用一个空行分隔。
- 公共头文件使用 include guard，命名沿用 `__WT_BSP_XXX_H__`、`__BOARD_CONFIG_H__` 这类全大写形式。
- 公共 C++ 兼容包装使用：
  ```c
  #ifdef __cplusplus
  extern "C" {
  #endif

  ...

  #ifdef __cplusplus
  } /* extern "C" */
  #endif
  ```

### 命名约定

- 公共 API 使用 `wt_bsp_<feature>_<action>()`，获取默认对象使用 `wt_bsp_get_<feature>()`。
- 公共类型使用 `wt_bsp_<feature>_xxx_t`；对象句柄使用指向不透明结构的 typedef，例如 `typedef struct wt_bsp_rgb_obj_t *wt_bsp_rgb_t;`。
- 对象实体结构使用 `wt_bsp_<feature>_obj_t`；配置结构使用 `wt_bsp_<feature>_info_t`。
- 板级私有函数使用 `board_<action>()`；板级静态对象使用 `s_bsp_<feature>`；文件内全局状态使用 `s_` 前缀。
- 文件内日志 tag 使用 `static const char *TAG = "模块名";`，例如 `"wt_bsp_rgb"` 或 `"board"`。
- 宏使用全大写。板级硬件参数使用 `BOARD_<FEATURE>_<NAME>`；公共编译期能力使用 `WT_BSP_<FEATURE>_...`。
- 内部哨兵枚举值可使用 `_WT_BSP_<FEATURE>_...` 前缀，避免被应用层当作普通枚举使用。

### 格式与排版

- 缩进使用 4 个空格，不使用 tab。
- 函数左花括号单独一行；`if`、`for`、`while`、`switch` 的左花括号与语句同行。
- 关键字后保留一个空格，例如 `if (ret != ESP_OK)`；函数调用名与左括号之间不加空格。
- 指针星号靠近变量名或参数名，沿用当前写法，例如 `const wt_bsp_rgb_info_t *info`、`static const char *TAG`。
- 结构体、数组和对象初始化优先使用 designated initializer，并在多字段初始化时每个字段单独一行，末尾保留逗号。
- 复合字面量初始化沿用当前 ESP-IDF 风格，例如：
  ```c
  ret = wt_bsp_rgb_init(&s_bsp_rgb, &(wt_bsp_rgb_info_t) {
      .gpio_num = BOARD_RGB_GPIO_NUM,
      .model = BOARD_RGB_MODEL,
  });
  ```
- 多行函数调用参数按现有代码对齐到参数列，尤其是底层驱动 API 调用。
- `switch` 的 `case` 与 `switch` 同级缩进，`case` 内语句缩进一级。
- 不新增行尾空格；提交前运行 `git diff --check`。

### 头文件与 Doxygen

- 公共头文件中的类型、枚举、结构体字段和函数都要写 Doxygen 注释。
- 函数注释至少包含 `@brief`、必要的 `@param[in]`/`@param[out]`/`@param[in,out]` 和所有稳定返回语义。
- 对象生命周期必须写清楚：由谁初始化、谁反初始化、返回的句柄是否可被应用释放。
- 获取类 API 的注释要明确不可用时返回 `NULL`；字符串获取类 API 要明确失败时是否返回空字符串。
- `board_config.h` 中的功能开关宏使用单行 Doxygen 注释，说明该板是否启用某能力或数量含义。
- 应用层和示例只 include `wt_bsp.h`；公共头文件不要暴露 `boards/<BOARD>/board.h` 或板级私有配置。

### 错误处理与资源生命周期

- 返回 `esp_err_t` 的函数先检查参数，参数无效返回 `ESP_ERR_INVALID_ARG` 并记录 `ESP_LOGE`。
- 初始化函数使用 `esp_err_t ret = ESP_OK;` 承接每一步结果；失败时释放已经成功初始化的资源，再返回原始错误码。
- `board_init()` 可重复调用；已经初始化时记录 warning 并返回 `ESP_OK`。
- `board_deinit()` 按初始化的反向顺序释放资源；即使某一步释放失败，也应继续尝试释放后续资源，并最终让板级状态回到未初始化。
- 清理函数应允许部分初始化状态，释放前检查句柄是否为 `NULL`，释放后将句柄清为 `NULL`。
- `wt_bsp_get_*()`、`board_get_*()` 不转移对象所有权；返回板级静态对象地址或 `NULL`。
- 外设缺失、功能未启用或初始化失败时，示例和应用必须处理 `NULL` 句柄，不假设硬件一定存在。

### 条件编译与 SoC 差异

- 功能开关统一通过 `wt_bsp_config_internal.h` 中的 `WT_BSP_<FEATURE>_ENABLE_IS_ENABLED` 使用。
- 公共头文件中仅在能力启用时暴露特定外设类型和 API；公共入口 `wt_bsp.h` 可以提供稳定的获取函数声明。
- SoC 后端差异优先使用 ESP-IDF 能力宏，例如 `SOC_RMT_SUPPORTED`、`SOC_GPSPI_SUPPORTED`，不要用板名硬编码公共实现。
- 板级目录只编译当前 Kconfig 选中的板卡文件；不要让某块板的私有实现进入所有目标。

### 注释和日志

- 注释用于解释硬件约束、资源顺序、非显然行为和失败后继续执行的原因；不要为直观赋值写重复注释。
- 新增注释优先使用中文或英文中的一种，并与所在文件现有注释保持一致。
- 日志消息使用英文，沿用当前风格；错误日志包含失败步骤和 `esp_err_to_name(ret)`。
- 非致命失败使用 `ESP_LOGW`，成功路径的重要硬件状态可用 `ESP_LOGI`，高频路径避免刷日志。

### 示例代码

- 示例入口先调用 `wt_bsp_init()` 并检查返回值；失败时打印错误并退出当前示例逻辑。
- 示例获取外设后必须判断句柄是否为 `NULL`，再调用对应 API。
- LVGL 示例中所有 LVGL 对象操作遵守已有 lock/unlock 约定。
- 示例中不要 include 板级私有头，不要复制板级引脚定义；硬件差异应由 BSP 层封装。

## 修改注意事项

- 不要引入与当前 BSP 目标无关的应用业务逻辑。
- 不要把示例里的临时配置、生成文件或本地构建产物提交。
- 不要为了单个板卡的特殊需求污染公共 API；先判断是否可以留在板级目录。
- 修改 CMake/Kconfig 时要考虑所有支持目标，尤其是 ESP32-P4 与 C2/C3/C5/C61 的差异。
- README 中可能存在历史路径或示例名，执行命令前以当前仓库实际目录为准。
