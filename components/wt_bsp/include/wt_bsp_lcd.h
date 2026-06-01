/**
 * @file wt_bsp_lcd.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP 组件的 LCD 接口。
 * @version 0.1
 * @date 2026-05-13
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_LCD_H__
#define __WT_BSP_LCD_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief LCD 对象句柄。
 */
typedef esp_lcd_panel_handle_t wt_bsp_lcd_t;

/**
 * @brief LCD 初始化配置结构体。
 */
typedef struct {
    uint32_t h_res;          /*!< 水平分辨率 */
    uint32_t v_res;          /*!< 垂直分辨率 */
} wt_bsp_lcd_config_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 获取已初始化的 LCD 句柄。
 *
 * @return LCD 句柄；不可用时返回 NULL。
 */
wt_bsp_lcd_t wt_bsp_get_lcd(void);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __WT_BSP_LCD_H__
