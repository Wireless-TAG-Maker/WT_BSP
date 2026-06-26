/**
 * @file wt_bsp_touch_port.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP touch port 接口。
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_TOUCH_PORT_H__
#define __WT_BSP_TOUCH_PORT_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_touch.h"

#if WT_BSP_TOUCH_ENABLE_IS_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

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

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
}
#endif

#endif // WT_BSP_TOUCH_ENABLE_IS_ENABLED

#endif // __WT_BSP_TOUCH_PORT_H__
