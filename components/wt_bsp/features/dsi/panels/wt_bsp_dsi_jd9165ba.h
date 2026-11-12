/**
 * @file wt_bsp_dsi_jd9165ba.h
 * @author Wireless-Tag
 * @brief JD9165BA 面板私有驱动接口.
 * @version 0.1
 * @date 2026-11-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __WT_BSP_DSI_JD9165BA_H__
#define __WT_BSP_DSI_JD9165BA_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"

#if WT_BSP_DSI_ENABLED

#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_vendor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief JD9165BA 面板使用的 DSI 传输配置.
 */
typedef struct {
    esp_lcd_dsi_bus_handle_t dsi_bus;             /*!< 借用的 DSI0 总线, 须配置为两条数据通道. */
    const esp_lcd_dpi_panel_config_t *dpi_config; /*!< 创建时复制的 DPI 配置, 要求 RGB565 输入和输出. */
} wt_bsp_dsi_jd9165ba_vendor_config_t;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 创建使用沃乐康 W700BH018I-30Z 初始化序列的 JD9165BA 面板.
 *
 * panel_config->vendor_config 指向 wt_bsp_dsi_jd9165ba_vendor_config_t,
 * bits_per_pixel 为 16. 工厂内部仅为此面板关闭 DSI0 command/frame ACK.
 * 初始化会退出睡眠并启动 DPI; 应用通过 esp_lcd_panel_disp_on_off() 控制显示开关.
 * IO 和 DSI 总线由调用方持有, 删除面板成功后方可释放; 配置结构在本调用返回后即可释放.
 * 调用方须串行调用面板操作.
 *
 * @param[in] io 已创建的 DBI 命令 IO, 不转移所有权.
 * @param[in] panel_config 面板配置, 包括复位 GPIO 和有效极性.
 * @param[out] ret_panel 创建的面板, 使用 esp_lcd_panel_del() 释放; 失败时为 NULL.
 * @return ESP_OK 创建成功.
 * @return ESP_ERR_INVALID_ARG 配置缺失或复位 GPIO 无效.
 * @return ESP_ERR_NOT_SUPPORTED 像素格式或颜色顺序不受支持.
 * @return ESP_ERR_NO_MEM 内存分配失败.
 * @return 其他错误来自底层 GPIO 或 DPI 创建.
 */
esp_err_t wt_bsp_dsi_new_panel_jd9165ba(esp_lcd_panel_io_handle_t io,
                                      const esp_lcd_panel_dev_config_t *panel_config,
                                      esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_DSI_ENABLED

#endif // __WT_BSP_DSI_JD9165BA_H__
