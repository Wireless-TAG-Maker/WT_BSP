/**
 * @file wt_bsp_rgb_port.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP RGB LED port 接口。
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_RGB_PORT_H__
#define __WT_BSP_RGB_PORT_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_rgb.h"

#if WT_BSP_RGB_ENABLE_IS_ENABLED

#include "led_strip.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief 支持的可寻址 RGB LED 型号。
 */
typedef enum {
    /**
     * @brief 兼容 WS2812 的 RGB LED。
     */
    WT_BSP_RGB_MODEL_WS2812 = 0,

    /**
     * @brief 兼容 SK6812 的 RGB/RGBW LED。
     */
    WT_BSP_RGB_MODEL_SK6812,

    /**
     * @brief 兼容 WS2811 的 RGB LED。
     */
    WT_BSP_RGB_MODEL_WS2811,

    /**
     * @brief 兼容 WS2816 的 RGB LED。
     */
    WT_BSP_RGB_MODEL_WS2816,
} wt_bsp_rgb_model_t;

/**
 * @brief LED 颜色分量顺序。
 */
typedef enum {
    /**
     * @brief 绿色、红色、蓝色分量顺序。
     */
    WT_BSP_RGB_FORMAT_GRB = 0,

    /**
     * @brief 红色、绿色、蓝色分量顺序。
     */
    WT_BSP_RGB_FORMAT_RGB,
#if WT_BSP_RGBW_ENABLE_IS_ENABLED
    /**
     * @brief 绿色、红色、蓝色、白色分量顺序。
     */
    WT_BSP_RGB_FORMAT_GRBW,

    /**
     * @brief 红色、绿色、蓝色、白色分量顺序。
     */
    WT_BSP_RGB_FORMAT_RGBW,
#endif
    /**
     * @brief 内部颜色格式哨兵值。
     */
    _WT_BSP_RGB_FORMAT_MAX,
} wt_bsp_rgb_format_t;

/**
 * @brief RGB LED 硬件配置。
 */
typedef struct {
    /**
     * @brief RGB LED 数据信号使用的 GPIO。
     */
    int gpio_num;

    /**
     * @brief LED 型号。
     */
    wt_bsp_rgb_model_t model;

    /**
     * @brief LED 颜色分量顺序。
     */
    wt_bsp_rgb_format_t format;

    /**
     * @brief 是否反转输出信号。
     */
    bool invert_out;
} wt_bsp_rgb_info_t;

/**
 * @brief RGB LED 对象。
 */
typedef struct wt_bsp_rgb_obj_t {
    /**
     * @brief RGB LED 硬件配置。
     */
    wt_bsp_rgb_info_t info;

    /**
     * @brief 写入操作后是否立即刷新灯带。
     */
    bool auto_refresh;

    /**
     * @brief 底层灯带驱动句柄。
     */
    struct led_strip_t *strip;

    /**
     * @brief 缓存的像素颜色。
     */
    wt_bsp_rgb_color_t pixels[WT_BSP_RGB_NUM];
} wt_bsp_rgb_obj_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 初始化 RGB LED 对象。
 *
 * @param[in,out] rgb 待初始化的 RGB LED 对象。
 * @param[in] info RGB LED 硬件配置。
 *
 * @return 成功时返回 ESP_OK。
 * @return 当 @p rgb、@p info、GPIO、型号或颜色格式无效时返回 ESP_ERR_INVALID_ARG。
 * @return 当前芯片没有可用灯带后端时返回 ESP_ERR_NOT_SUPPORTED。
 * @return 灯带驱动失败时返回对应的 ESP-IDF 错误码。
 */
esp_err_t wt_bsp_rgb_init(wt_bsp_rgb_t rgb, const wt_bsp_rgb_info_t *info);

/**
 * @brief 反初始化 RGB LED 对象。
 *
 * @param[in,out] rgb 待反初始化的 RGB LED 对象。
 *
 * @return 成功时返回 ESP_OK。
 * @return 当 @p rgb 为 NULL 时返回 ESP_ERR_INVALID_ARG。
 */
esp_err_t wt_bsp_rgb_deinit(wt_bsp_rgb_t rgb);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_RGB_ENABLE_IS_ENABLED

#endif // __WT_BSP_RGB_PORT_H__
