# Wireless-Tag 板级支持包 (WT_BSP)

[English](./README.md) | [中文](./README_CN.md)

**WT_BSP** 是专为启明云端 (Wireless-Tag) 基于乐鑫 ESP32 系列 MCU 开发的开发板设计的板级支持包。它提供了一套统一的、与硬件无关的 API，用于操作常用的外设，使开发者能够以极低的代码修改量在不同的开发板之间切换。

该 BSP 针对 **ESP-IDF v6.0.1 及以上版本** 进行了优化，覆盖了从高性价比的 ESP32-C2 到高性能 ESP32-P4 的全系列芯片。

## 🚀 核心特性

-   **统一的硬件 API**：通过一组一致的 `wt_bsp_*` 函数访问 RGB LED、按键、SD 卡、显示屏和摄像头。
-   **模块化架构**：每个外设在 BSP 内部都作为一个独立模块实现，易于扩展和维护。
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
| **WT9932C61-TINY** | ESP32-C6 | RGB LED, 按键 |
| **WT9932P4-TINY** | ESP32-P4 | MIPI-DSI, MIPI-CSI, SDMMC, 触摸, RGB, 按键 |
| **WT9932P4C61-TINY** | ESP32-P4 + ESP32-C61 | 外设与 P4-TINY 一致，额外搭载 ESP32-C61 用于无线/低功耗任务 |

对于 WT9932P4C61-TINY，请将 FUSB（全速 USB）和 HUSB（高速 USB）都接入电脑；在任意 WT_BSP 示例中运行 `idf.py p4_flash`，通过 FUSB 将 ESP32-P4 临时烧录成 USB-UART 桥，然后通过 HUSB TinyUSB CDC 串口烧录 ESP32-C61 固件。

## 🛠️ 快速上手

### 环境准备
-   **ESP-IDF**：推荐使用 `v6.0.1` 及以上版本（兼容 `v5.3` 及以上版本）。
-   **Git**：用于克隆仓库及管理子模块。

### 安装方法
1.  将本仓库克隆到项目的 `components` 目录下，或将其添加为 Git 子模块。
2.  在项目的 `idf_component.yml` 中确保依赖项已配置。

### 基础用法
1.  **选择开发板**：在示例目录中运行 `idf.py set-board` 并选择支持的开发板，命令会生成下一次配置/构建使用的板级默认配置；切换到不同目标芯片的开发板时会自动执行 `idf.py fullclean`。非交互场景可使用 `WT_BSP_BOARD=WT9932P4-TINY idf.py set-board`，也可以直接用 `WT_BSP_BOARD=WT9932P4-TINY idf.py build` 构建。
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
    -   `features`：RGB、按键、SDMMC、DSI、CSI、触摸等可复用外设能力。
    -   `include`：公共 API 头文件。
    -   `src`：通用 BSP 接口实现。
    -   `tools`：`set-board` 和项目 CMake 辅助脚本。
-   `examples`：展示各种外设能力的示例工程。

## ⚙️ 工作原理

WT_BSP 使用 **板级接口模式 (Board Interface Pattern)** 来解耦应用逻辑与硬件细节：
1.  **接口定义**：`wt_bsp_interface_t` 定义了一张包含所有板载资源函数指针的表。
2.  **板级实现**：`components/wt_bsp/boards/` 下的每个板级目录负责实现这些函数并进行注册。
3.  **统一访问**：应用层调用 `wt_bsp_init()`，它会根据 `set-board` 写入的 Kconfig 配置自动选择正确的板级实现，随后应用即可通过通用的 `wt_bsp_get_*()` 函数获取资源。

## 🤝 参与贡献
欢迎提交贡献！请遵循现有的代码风格，并确保所有更改都已在支持的硬件上进行过测试。
