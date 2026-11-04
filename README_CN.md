# Wireless-Tag 板级支持包 (WT_BSP)

[English](./README.md) | [中文](./README_CN.md)

**WT_BSP** 是专为启明云端 (Wireless-Tag) 基于乐鑫 ESP32 系列 MCU 开发的开发板设计的板级支持包。它提供了一套统一的、与硬件无关的 API，用于操作常用的外设，使开发者能够以极低的代码修改量在不同的开发板之间切换。

该 BSP 要求使用 **ESP-IDF v6.0.0 及以上版本**，覆盖了从高性价比的 ESP32-C2 到高性能 ESP32-P4 的全系列芯片。

## 🚀 核心特性

-   **统一的硬件 API**：通过一组一致的 `wt_bsp_*` 函数访问 RGB LED、按键、SD 卡、显示屏和摄像头。
-   **模块化架构**：每个外设在 BSP 内部都作为一个独立模块实现，易于扩展和维护。
-   **独立相机与 USB**：CSI/JPEG 采集和 USB UVC 分别启用，应用负责摄像头到 USB 的桥接。camera interface 按统一配置路由到自注册驱动，并通过统一操作表调用。
-   **多板支持**：原生支持多种 "TINY" 系列开发板，通过 `idf.py set-board` 选择开发板并生成默认配置。
-   **先进的 ESP32-P4 支持**：
    -   **MIPI-DSI**：支持高分辨率显示屏并集成 LVGL。
    -   **MIPI-CSI**：支持摄像头及硬件加速 (PPA) 图像处理。
    -   **P4C61 烧录桥工具**：`idf.py p4_flash` 可将 WT9932P4C61-TINY 的 ESP32-P4 烧录为 USB-UART 桥，方便快速烧录和体验板载 ESP32-C61。
-   **丰富的示例**：从简单的 "Blink" 到集成了 UI 和摄像头流媒体的复杂 "出厂测试" 固件。

## 📋 支持的开发板

| 开发板名称 | MCU 芯片 | 核心特性 |
| :--- | :--- | :--- |
| **WT9932C2-TINY** | ESP32-C2 | RGB LED, 按键 |
| **WT9932C3-TINY** | ESP32-C3 | RGB LED, 按键 |
| **WT9932C5-TINY** | ESP32-C5 | RGB LED, 按键 |
| **WT9932C61-TINY** | ESP32-C61 | RGB LED, 按键 |
| **WT9932P4-TINY** | ESP32-P4 v1.x | MIPI-DSI, MIPI-CSI, SDMMC, 触摸, RGB, 按键, USB CDC/UVC |
| **WT9932P4X-TINY** | ESP32-P4 v3.x | MIPI-DSI, MIPI-CSI, SDMMC, 触摸, RGB, 按键, USB CDC/UVC |
| **WT9932P4C61-TINY** | ESP32-P4 + ESP32-C61 | 外设与 P4-TINY 一致，额外搭载 ESP32-C61 用于无线/低功耗任务 |
| **WT9932S31-TINY** | ESP32-S31 | RGB、按键、SDMMC、USB CDC/UVC、DVP JPEG 相机 (GC2145/OV3660) |

WT9932P4-TINY 与 WT9932P4X-TINY 的外设资源相同，但 v1.x/v3.x 固件不兼容；请按芯片版本选板并重新构建。详见[P4X 板卡说明](components/wt_bsp/boards/WT9932P4X-TINY/README.md)。

WT9932S31-TINY 使用 **ESP-IDF v6.1 或以上版本**。v6.1 中构建 S31 需使用 `idf.py --preview`；USB 数据连接和摄像头选择见[板卡说明](components/wt_bsp/boards/WT9932S31-TINY/README.md)。

对于 WT9932P4C61-TINY，请先接入 FUSB（全速 USB），在任意 WT_BSP 示例中运行 `idf.py p4_flash`，通过 FUSB 将 ESP32-P4 临时烧录成 USB-UART 桥；烧录 ESP32-C61 固件前，请拔掉 FUSB 并接入 HUSB（高速 USB），后续直接使用 HUSB 给开发板供电并给 ESP32-C61 单独编写、烧录和调试代码。

## 🛠️ 快速上手

### 环境准备
-   **ESP-IDF**：要求使用 `v6.0.0` 及以上版本。
-   **Git**：用于克隆仓库及管理子模块。

### 安装方法
1.  克隆本仓库即可使用仓库内示例。复用 BSP 时，将 `components/wt_bsp` 复制到自己项目的 `components` 目录，或通过 `EXTRA_COMPONENT_DIRS` 引用该组件目录。
2.  组件清单负责解析托管依赖；按[组件指南](components/wt_bsp/README.md)接入项目 CMake 辅助工具以使用 `set-board`，也可通过 Kconfig 选择匹配板卡。

### 基础用法
1.  **选择开发板**：在示例目录中运行 `idf.py set-board` 并选择支持的开发板，再执行 `idf.py build`；切换到不同目标芯片的开发板时会自动执行 `idf.py fullclean`。非交互场景同样保留选板再构建的两步流程：

    ```shell
    WT_BSP_BOARD=WT9932C3-TINY idf.py -C examples/get-started/blink set-board
    idf.py -C examples/get-started/blink build
    ```

    项目 CMake 仍兼容显式 `WT_BSP_BOARD=<BOARD> idf.py build`，这些方式均不依赖 `idf_ext.py`。
2.  **初始化 BSP**：
    ```c
    #include "wt_bsp.h"

    void app_main(void) {
        /* 初始化所有板载资源 */
        ESP_ERROR_CHECK(wt_bsp_init());

        /* 访问 RGB LED */
        wt_bsp_rgb_t rgb = wt_bsp_get_rgb();
        if (rgb) {
            wt_bsp_rgb_set_color(rgb, (wt_bsp_rgb_color_t){.r = 255, .g = 0, .b = 0});
        }
    }
    ```

## 📂 项目结构

-   `components/wt_bsp`：BSP 核心组件。
    -   `boards`：板级硬件配置、Kconfig 和资源生命周期。
    -   `features`：可复用外设能力，包括彼此独立的 CSI、JPEG camera、USB UVC 和 USB CDC。
    -   `include`：公共 API 头文件。
    -   `src`：通用 BSP 接口实现。
    -   `tools`：`set-board` 和项目 CMake 辅助脚本。
-   `examples`：展示各种外设能力的示例工程。
    -   `common_components/wt_camera_uvc`：应用层帧桥接、JPEG 转交及 USB profile。

[USB CDC 回显示例](examples/usb/device_cdc/README_CN.md) 支持 WT9932P4-TINY、WT9932P4X-TINY、
WT9932P4C61-TINY 和 WT9932S31-TINY，通过各板的 USB OTG 接口提供串口回显。

[S31 工厂示例](examples/wt_factory/wt9932s31-tiny/README.md) 默认离线检查摄像头
和 SD 卡，J1 连接电脑并枚举后提供 UVC + CDC。SD 录像默认关闭，可在 menuconfig
的 `S31 factory` 中开启，开启后离线 SW2 同时控制暂停/恢复。RGB 基准色为正常
绿色、SD 故障红色、摄像头故障蓝色、两者故障黄色；SD 和摄像头正常、仅按键回调
注册或 CDC 控制台启动失败时为白色。启动时默认点亮，SW2/BOOT 每按一次切换熄灭/点亮，
重新点亮时显示最新状态色；USB 模式和录像关闭时也生效，复位恢复点亮。BOOT 熄灯
不代表硬件正常。暂停不再单独用蓝色表示，取卡前需查看暂停和文件收尾日志。
完整灯色含义见 [RGB 指示灯](examples/wt_factory/wt9932s31-tiny/README.md#rgb-指示灯)。
2026-10-20 用户确认的是分层调整前的 GC2145/录像关闭固件；该历史实板记录不作为本次 camera/UVC 迁移的验收结果。

## ⚙️ 工作原理

WT_BSP 使用 **板级接口模式 (Board Interface Pattern)** 来解耦应用逻辑与硬件细节：
1.  **接口定义**：`wt_bsp_interface_t` 定义了一张包含所有板载资源函数指针的表。
2.  **板级实现**：`components/wt_bsp/boards/` 下的每个板级目录负责实现这些函数并进行注册。
3.  **统一访问**：应用层调用 `wt_bsp_init()`，它根据 Kconfig 选择板级实现，随后通过通用的 `wt_bsp_get_*()` 获取资源。`set-board` 提供板卡预设，普通重配置保留原生 menuconfig 选板和功能裁剪。

新增板卡时，在自己的目录内贡献 Kconfig choice 成员、默认值及 CMake 文件，公共收集器自动发现，
无需修改公共板名列表。详见[新增板卡指南](components/wt_bsp/README.md#新增板卡步骤)。
camera 板级仅提供 `wt_bsp_camera_info_t`，自注册 sensor factory 返回统一 `wt_bsp_camera_ops_t`，
新增 sensor 无需维护中央驱动表。详见[相机指南](components/wt_bsp/features/camera/README.md)和
[应用桥接组件](examples/common_components/wt_camera_uvc/README.md)。

[2026-11-04 验证记录](docs/bsp-validation.md)列出了完整构建矩阵、最低 SDK 回归、模拟器覆盖与实板验证边界。

## 🤝 参与贡献
欢迎提交贡献！请遵循现有的代码风格，并确保所有更改都已在支持的硬件上进行过测试。
