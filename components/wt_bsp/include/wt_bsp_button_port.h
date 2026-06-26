/**
 * @file wt_bsp_button_port.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP button port 接口。
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_BUTTON_PORT_H__
#define __WT_BSP_BUTTON_PORT_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_button.h"

#if WT_BSP_BUTTON_ENABLE_IS_ENABLED

#include "iot_button.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief 按键硬件配置。
 */
typedef struct {
    /**
     * @brief 连接按键的 GPIO。
     */
    gpio_num_t gpio_num;

    /**
     * @brief 表示按下状态的 GPIO 电平。
     */
    wt_bsp_button_active_level_t active_level;
} wt_bsp_button_info_t;

/**
 * @brief 按键对象。
 */
typedef struct wt_bsp_button_obj_t {
    /**
     * @brief 按键硬件配置。
     */
    wt_bsp_button_info_t info;

    /**
     * @brief 内部 iot_button 句柄。
     */
    button_handle_t handle;

    /**
     * @brief 用户事件回调函数。
     */
    wt_bsp_button_event_cb_t event_cb;

    /**
     * @brief 传递给 @ref event_cb 的用户上下文。
     */
    void *user_data;
} wt_bsp_button_obj_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 初始化按键对象。
 *
 * @param[in,out] button 待初始化的按键对象。
 * @param[in] info 按键硬件配置。
 *
 * @return 成功时返回 ESP_OK。
 * @return 当 @p button、@p info 或 GPIO 无效时返回 ESP_ERR_INVALID_ARG。
 * @return 当按键状态机初始化失败时返回 ESP_FAIL。
 * @return GPIO 或定时器设置失败时返回对应的 ESP-IDF 错误码。
 */
esp_err_t wt_bsp_button_init(wt_bsp_button_t button, const wt_bsp_button_info_t *info);

/**
 * @brief 反初始化按键对象。
 *
 * @param[in,out] button 待反初始化的按键对象。
 *
 * @return 成功时返回 ESP_OK。
 * @return 当 @p button 为 NULL 时返回 ESP_ERR_INVALID_ARG。
 */
esp_err_t wt_bsp_button_deinit(wt_bsp_button_t button);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_BUTTON_ENABLE_IS_ENABLED

#endif // __WT_BSP_BUTTON_PORT_H__
