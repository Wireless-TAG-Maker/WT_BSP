/**
 * @file wt_bsp_rgb.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP 组件的 RGB LED 接口。
 * @version 0.1
 * @date 2026-05-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_RGB_H__
#define __WT_BSP_RGB_H__

/* ==================== [Includes] ========================================== */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "wt_bsp_config_internal.h"
#include "esp_err.h"

/**
 * @brief RGB LED 对象句柄。
 */
typedef struct wt_bsp_rgb_obj_t *wt_bsp_rgb_t;

#if WT_BSP_RGB_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief RGB 或 RGBW 颜色值。
 */
typedef struct {
    /**
     * @brief 红色分量。
     */
    uint8_t r;

    /**
     * @brief 绿色分量。
     */
    uint8_t g;

    /**
     * @brief 蓝色分量。
     */
    uint8_t b;
#if WT_BSP_RGBW_ENABLED
    /**
     * @brief 白色分量。
     */
    uint8_t w;
#endif
} wt_bsp_rgb_color_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 开启或关闭写入操作后的自动刷新。
 *
 * @param[in,out] rgb RGB LED 对象句柄。
 * @param[in] enable 为 true 时在设置颜色后自动刷新；为 false 时需要显式调用
 * @ref wt_bsp_rgb_refresh。
 *
 * @return 成功时返回 ESP_OK。
 * @return 当 @p rgb 为 NULL 时返回 ESP_ERR_INVALID_ARG。
 */
esp_err_t wt_bsp_rgb_set_auto_refresh(wt_bsp_rgb_t rgb, bool enable);

/**
 * @brief 设置单个 RGB LED 像素颜色。
 *
 * @param[in,out] rgb RGB LED 对象句柄。
 * @param[in] index 像素索引，范围为 0 到 WT_BSP_RGB_NUM - 1。
 * @param[in] color 待写入的颜色。
 *
 * @return 成功时返回 ESP_OK。
 * @return 当 @p rgb 为 NULL 或 @p index 超出范围时返回 ESP_ERR_INVALID_ARG。
 * @return 灯带驱动失败时返回对应的 ESP-IDF 错误码。
 */
esp_err_t wt_bsp_rgb_set_pixel(wt_bsp_rgb_t rgb, uint32_t index, wt_bsp_rgb_color_t color);

/**
 * @brief 将所有 RGB LED 像素设置为同一颜色。
 *
 * @param[in,out] rgb RGB LED 对象句柄。
 * @param[in] color 写入到每个像素的颜色。
 *
 * @return 成功时返回 ESP_OK。
 * @return 当 @p rgb 为 NULL 时返回 ESP_ERR_INVALID_ARG。
 * @return 灯带驱动失败时返回对应的 ESP-IDF 错误码。
 */
esp_err_t wt_bsp_rgb_set_color(wt_bsp_rgb_t rgb, wt_bsp_rgb_color_t color);

/**
 * @brief 使用缓存的像素数据刷新 RGB LED 灯带。
 *
 * @param[in,out] rgb RGB LED 对象句柄。
 *
 * @return 成功时返回 ESP_OK。
 * @return 当 @p rgb 为 NULL 时返回 ESP_ERR_INVALID_ARG。
 * @return 灯带驱动失败时返回对应的 ESP-IDF 错误码。
 */
esp_err_t wt_bsp_rgb_refresh(wt_bsp_rgb_t rgb);

/**
 * @brief 清空所有 RGB LED 像素。
 *
 * @param[in,out] rgb RGB LED 对象句柄。
 *
 * @return 成功时返回 ESP_OK。
 * @return 当 @p rgb 为 NULL 时返回 ESP_ERR_INVALID_ARG。
 * @return 灯带驱动失败时返回对应的 ESP-IDF 错误码。
 */
esp_err_t wt_bsp_rgb_clear(wt_bsp_rgb_t rgb);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_RGB_ENABLED

#endif // __WT_BSP_RGB_H__
