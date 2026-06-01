/**
 * @file wt_bsp_button.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP 组件的按键接口。
 * @version 0.1
 * @date 2026-05-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_BUTTON_H__
#define __WT_BSP_BUTTON_H__

/* ==================== [Includes] ========================================== */

#include <stdbool.h>
#include <stdint.h>
#include "wt_bsp_config_internal.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "lwbtn/lwbtn.h"
#include "esp_timer.h"

#if WT_BSP_BUTTON_ENABLE_IS_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */


/* ==================== [Typedefs] ========================================== */

/**
 * @brief 按键对象句柄。
 */
typedef struct wt_bsp_button_obj_t *wt_bsp_button_t;

/**
 * @brief 按键有效电平。
 */
typedef enum {
    /**
     * @brief GPIO 为低电平时认为按键被按下。
     */
    WT_BSP_BUTTON_ACTIVE_LOW = 0,

    /**
     * @brief GPIO 为高电平时认为按键被按下。
     */
    WT_BSP_BUTTON_ACTIVE_HIGH = 1,
} wt_bsp_button_active_level_t;

/**
 * @brief 按键事件类型。
 */
typedef enum {
    /**
     * @brief 按键按下事件。
     */
    WT_BSP_BUTTON_EVENT_PRESS = 0,

    /**
     * @brief 按键释放事件。
     */
    WT_BSP_BUTTON_EVENT_RELEASE,

    /**
     * @brief 完整点击事件。
     */
    WT_BSP_BUTTON_EVENT_CLICK,

    /**
     * @brief 长按保持事件。
     */
    WT_BSP_BUTTON_EVENT_KEEPALIVE,
} wt_bsp_button_event_t;

/**
 * @brief 按键事件回调函数。
 *
 * @param[in] button 产生事件的按键对象。
 * @param[in] event 按键事件类型。
 * @param[in] user_data 通过 @ref wt_bsp_button_register_event_cb 传入的用户上下文。
 */
typedef void (*wt_bsp_button_event_cb_t)(wt_bsp_button_t button,
        wt_bsp_button_event_t event,
        void *user_data);

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
typedef struct wt_bsp_button_obj_t{
    /**
     * @brief 按键硬件配置。
     */
    wt_bsp_button_info_t info;

    /**
     * @brief 内部 lwbtn 核心对象。
     */
    lwbtn_t lwbtn;

    /**
     * @brief 内部 lwbtn 按键对象。
     */
    lwbtn_btn_t lwbtn_btn;

    /**
     * @brief 用于轮询按键状态的周期定时器。
     */
    esp_timer_handle_t timer;

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
 * 该函数会配置按键 GPIO、初始化消抖/点击状态机，并启动轮询定时器。
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

/**
 * @brief 注册按键事件回调函数。
 *
 * 将 @p cb 设为 NULL 可清除已注册的回调。
 *
 * @param[in,out] button 按键对象句柄。
 * @param[in] cb 按键事件发生时调用的回调函数。
 * @param[in] user_data 回传给 @p cb 的用户上下文。
 *
 * @return 成功时返回 ESP_OK。
 * @return 当 @p button 为 NULL 时返回 ESP_ERR_INVALID_ARG。
 */
esp_err_t wt_bsp_button_register_event_cb(wt_bsp_button_t button, wt_bsp_button_event_cb_t cb, void *user_data);

/**
 * @brief 重置按键状态机。
 *
 * @param[in,out] button 按键对象句柄。
 *
 * @return 成功时返回 ESP_OK。
 * @return 当 @p button 为 NULL 时返回 ESP_ERR_INVALID_ARG。
 * @return 当底层状态机重置失败时返回 ESP_FAIL。
 */
esp_err_t wt_bsp_button_reset(wt_bsp_button_t button);

/**
 * @brief 获取当前连续点击次数。
 *
 * @param[in] button 按键对象句柄。
 *
 * @return 当前点击次数；当 @p button 为 NULL 时返回 0。
 */
uint8_t wt_bsp_button_get_click_count(wt_bsp_button_t button);

/**
 * @brief 获取当前长按保持计数。
 *
 * @param[in] button 按键对象句柄。
 *
 * @return 当前长按保持计数；当 @p button 为 NULL 时返回 0。
 */
uint16_t wt_bsp_button_get_keepalive_count(wt_bsp_button_t button);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_BUTTON_ENABLE_IS_ENABLED

#endif // __WT_BSP_BUTTON_H__
