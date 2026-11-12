/**
 * @file wt_bsp_touch.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP 组件的触摸接口。
 * @version 0.1
 * @date 2026-06-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_TOUCH_H__
#define __WT_BSP_TOUCH_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"

/**
 * @brief 触摸对象句柄。
 */
typedef struct wt_bsp_touch_obj_t *wt_bsp_touch_t;

#if WT_BSP_TOUCH_ENABLED

#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_touch.h"

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 读取触摸数据。
 *
 * @param[in] touch 触摸对象。
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE 或底层读取错误。
 */
esp_err_t wt_bsp_touch_read(wt_bsp_touch_t touch);

/**
 * @brief 获取触摸坐标。
 *
 * @param[in] touch 触摸对象。
 * @param[out] x X 坐标数组。
 * @param[out] y Y 坐标数组。
 * @param[out] strength 压力数组（可选，可设为 NULL）。
 * @param[out] point_num 点数量指针。
 * @param[in] max_point_num X/Y 数组最大容量。
 * @return bool 是否有按压; 参数无效、对象不可用或读取失败时返回 false。
 */
bool wt_bsp_touch_get_coordinates(wt_bsp_touch_t touch, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);

/**
 * @brief 获取底层 esp_lcd_touch 句柄。
 * @param[in] touch 触摸对象。
 * @return 已初始化对象的借用句柄, 不可用时为 NULL; 调用方不能自行删除。
 */
esp_lcd_touch_handle_t wt_bsp_touch_get_handle(wt_bsp_touch_t touch);

/**
 * @brief 将触摸注册到 LVGL。
 * @param[in] touch 已初始化的触摸对象。
 * @param[in] disp 已启动的 LVGL 显示。
 * @return 对象持有的输入设备, 失败时为 NULL; 重复调用返回已注册设备。
 * 输入设备由 wt_bsp_touch_deinit() 释放, 调用方不能自行删除。
 */
lv_indev_t *wt_bsp_touch_lvgl_start(wt_bsp_touch_t touch, lv_display_t *disp);

#ifdef __cplusplus
}
#endif

#endif // WT_BSP_TOUCH_ENABLED

#endif // __WT_BSP_TOUCH_H__
