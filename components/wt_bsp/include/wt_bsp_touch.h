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

#if WT_BSP_TOUCH_ENABLE_IS_ENABLED

#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief 触摸对象句柄。
 */
typedef struct wt_bsp_touch_obj_t *wt_bsp_touch_t;

/**
 * @brief 触摸硬件配置。
 */
typedef struct {
    void *i2c_bus_handle;   /*!< 外部提供的 I2C 总线句柄 (i2c_master_bus_handle_t)。如果非 NULL，则不内部初始化 I2C。 */
    int i2c_port;           /*!< I2C 端口号。 */
    int scl_pin;            /*!< I2C SCL 引脚。 */
    int sda_pin;            /*!< I2C SDA 引脚。 */
    int rst_pin;            /*!< 复位引脚，未使用设为 -1。 */
    int int_pin;            /*!< 中断引脚，未使用设为 -1。 */
    uint16_t width;         /*!< 触摸屏宽度。 */
    uint16_t height;        /*!< 触摸屏高度。 */
} wt_bsp_touch_info_t;

/**
 * @brief 触摸对象。
 */
typedef struct wt_bsp_touch_obj_t {
    wt_bsp_touch_info_t info;
    esp_lcd_touch_handle_t handle;
    bool is_initialized;
} wt_bsp_touch_obj_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 初始化触摸对象。
 * 
 * @param[in,out] touch 待初始化的触摸对象。
 * @param[in] info 硬件配置。
 * @return esp_err_t 
 */
esp_err_t wt_bsp_touch_init(wt_bsp_touch_t touch, const wt_bsp_touch_info_t *info);

/**
 * @brief 反初始化触摸对象。
 * 
 * @param[in,out] touch 待反初始化的触摸对象。
 * @return esp_err_t 
 */
esp_err_t wt_bsp_touch_deinit(wt_bsp_touch_t touch);

/**
 * @brief 读取触摸数据。
 * 
 * @param[in] touch 触摸对象。
 * @return esp_err_t 
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
 * @return bool 是否有按压。
 */
bool wt_bsp_touch_get_coordinates(wt_bsp_touch_t touch, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);

/**
 * @brief 获取底层 esp_lcd_touch 句柄。
 */
esp_lcd_touch_handle_t wt_bsp_touch_get_handle(wt_bsp_touch_t touch);

#ifdef __cplusplus
}
#endif

#endif // WT_BSP_TOUCH_ENABLE_IS_ENABLED

#endif // __WT_BSP_TOUCH_H__
