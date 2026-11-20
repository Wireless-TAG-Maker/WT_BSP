/**
 * @file wt_bsp_dsi_ili9488.h
 * @author Wireless-Tag
 * @brief EQT350XJI718P 模组 ILI9488 面板私有接口.
 * @version 0.1
 * @date 2026-11-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __WT_BSP_DSI_ILI9488_H__
#define __WT_BSP_DSI_ILI9488_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"

#if WT_BSP_DSI_ENABLED

#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 在单通道 DSI 总线上创建 EQT350XJI718P 面板.
 * @param[in] bus 借用的 DSI 总线, 须在删除面板后释放.
 * @param[in] io 借用的 DBI 命令接口, 生命周期与 bus 相同.
 * @param[in] dpi RGB565 DPI 帧缓冲和视频时序, 创建时复制.
 * @param[in] reset_gpio 低有效复位引脚, 负值使用 DCS 软件复位.
 * @param[out] ret_panel 返回的面板由调用方通过 esp_lcd_panel_del() 释放.
 * @return ESP_OK 成功, ESP_ERR_INVALID_ARG 参数无效, ESP_ERR_NO_MEM 分配失败,
 *         其他错误来自 GPIO 或 DPI 驱动.
 */
esp_err_t wt_bsp_dsi_new_panel_ili9488(esp_lcd_dsi_bus_handle_t bus, esp_lcd_panel_io_handle_t io,
                                       const esp_lcd_dpi_panel_config_t *dpi, int reset_gpio,
                                       esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_DSI_ENABLED

#endif // __WT_BSP_DSI_ILI9488_H__
