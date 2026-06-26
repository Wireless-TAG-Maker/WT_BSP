/**
 * @file wt_bsp_button.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-05-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_button_port.h"

#if WT_BSP_BUTTON_ENABLE_IS_ENABLED

#include "esp_log.h"
#include "driver/gpio.h"
#include "button_gpio.h"

/* ==================== [Defines] =========================================== */

#define WT_BSP_BUTTON_DEFAULT_CLICK_MIN_MS         20U
#define WT_BSP_BUTTON_DEFAULT_KEEPALIVE_PERIOD_MS  1000U
#define WT_BSP_BUTTON_DEFAULT_MAX_CLICK_COUNT      3U

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static esp_err_t wt_bsp_button_create_device(wt_bsp_button_t button);
static esp_err_t wt_bsp_button_register_driver_cbs(wt_bsp_button_t button);
static void wt_bsp_button_driver_event_cb(void *button_handle, void *usr_data);
static bool wt_bsp_button_map_event(button_event_t driver_event, wt_bsp_button_event_t *bsp_event);
static void wt_bsp_button_cleanup(wt_bsp_button_t button);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_button";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_button_init(wt_bsp_button_t button, const wt_bsp_button_info_t *info)
{
    esp_err_t ret = ESP_OK;

    if (button == NULL || info == NULL || !GPIO_IS_VALID_GPIO(info->gpio_num)) {
        ESP_LOGE(TAG, "Invalid argument: button=%p, info=%p", button, info);
        return ESP_ERR_INVALID_ARG;
    }

    button->info = *info;
    if (button->info.active_level != WT_BSP_BUTTON_ACTIVE_HIGH) {
        button->info.active_level = WT_BSP_BUTTON_ACTIVE_LOW;
    }

    button->event_cb = NULL;
    button->user_data = NULL;
    ret = wt_bsp_button_create_device(button);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

esp_err_t wt_bsp_button_deinit(wt_bsp_button_t button)
{
    if (button == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    wt_bsp_button_cleanup(button);
    return ESP_OK;
}

esp_err_t wt_bsp_button_register_event_cb(wt_bsp_button_t button,
        wt_bsp_button_event_cb_t cb,
        void *user_data)
{
    if (button == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    button->event_cb = cb;
    button->user_data = user_data;
    return ESP_OK;
}

esp_err_t wt_bsp_button_reset(wt_bsp_button_t button)
{
    if (button == NULL) {
        ESP_LOGE(TAG, "Invalid argument: button=%p", button);
        return ESP_ERR_INVALID_ARG;
    }

    if (button->handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    wt_bsp_button_cleanup(button);
    return wt_bsp_button_create_device(button);
}

uint8_t wt_bsp_button_get_click_count(wt_bsp_button_t button)
{
    if (button == NULL) {
        ESP_LOGE(TAG, "Invalid argument: button=%p", button);
        return 0;
    }

    return button->handle != NULL ? iot_button_get_repeat(button->handle) : 0;
}

uint16_t wt_bsp_button_get_keepalive_count(wt_bsp_button_t button)
{
    if (button == NULL) {
        ESP_LOGE(TAG, "Invalid argument: button=%p", button);
        return 0;
    }

    return button->handle != NULL ? iot_button_get_long_press_hold_cnt(button->handle) : 0;
}

/* ==================== [Static Functions] ================================== */

static esp_err_t wt_bsp_button_create_device(wt_bsp_button_t button)
{
    button_config_t button_config = {
        .long_press_time = WT_BSP_BUTTON_DEFAULT_KEEPALIVE_PERIOD_MS,
        .short_press_time = WT_BSP_BUTTON_DEFAULT_CLICK_MIN_MS,
    };
    button_gpio_config_t gpio_config = {
        .gpio_num = button->info.gpio_num,
        .active_level = button->info.active_level,
        .enable_power_save = false,
        .disable_pull = false,
    };

    esp_err_t ret = iot_button_new_gpio_device(&button_config, &gpio_config, &button->handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "create gpio button failed: %s", esp_err_to_name(ret));
        button->handle = NULL;
        return ret;
    }

    ret = wt_bsp_button_register_driver_cbs(button);
    if (ret != ESP_OK) {
        wt_bsp_button_cleanup(button);
    }

    return ret;
}

static esp_err_t wt_bsp_button_register_driver_cbs(wt_bsp_button_t button)
{
    static button_event_args_t multiple_click_args = {
        .multiple_clicks.clicks = WT_BSP_BUTTON_DEFAULT_MAX_CLICK_COUNT,
    };
    static const button_event_t events[] = {
        BUTTON_PRESS_DOWN,
        BUTTON_PRESS_UP,
        BUTTON_SINGLE_CLICK,
        BUTTON_DOUBLE_CLICK,
        BUTTON_LONG_PRESS_HOLD,
    };

    esp_err_t ret = ESP_OK;
    for (size_t i = 0; i < sizeof(events) / sizeof(events[0]); i++) {
        ret = iot_button_register_cb(button->handle, events[i], NULL, wt_bsp_button_driver_event_cb, button);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "register button callback failed: event=%d, err=%s",
                     events[i], esp_err_to_name(ret));
            return ret;
        }
    }

    ret = iot_button_register_cb(button->handle, BUTTON_MULTIPLE_CLICK, &multiple_click_args,
                                 wt_bsp_button_driver_event_cb, button);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register multiple click callback failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

static void wt_bsp_button_driver_event_cb(void *button_handle, void *usr_data)
{
    wt_bsp_button_t button = (wt_bsp_button_t)usr_data;
    wt_bsp_button_event_t bsp_event = WT_BSP_BUTTON_EVENT_PRESS;

    if (button == NULL || button->event_cb == NULL) {
        return;
    }

    if (!wt_bsp_button_map_event(iot_button_get_event((button_handle_t)button_handle), &bsp_event)) {
        return;
    }

    button->event_cb(button, bsp_event, button->user_data);
}

static bool wt_bsp_button_map_event(button_event_t driver_event, wt_bsp_button_event_t *bsp_event)
{
    if (bsp_event == NULL) {
        return false;
    }

    switch (driver_event) {
    case BUTTON_PRESS_DOWN:
        *bsp_event = WT_BSP_BUTTON_EVENT_PRESS;
        return true;
    case BUTTON_PRESS_UP:
        *bsp_event = WT_BSP_BUTTON_EVENT_RELEASE;
        return true;
    case BUTTON_SINGLE_CLICK:
    case BUTTON_DOUBLE_CLICK:
    case BUTTON_MULTIPLE_CLICK:
        *bsp_event = WT_BSP_BUTTON_EVENT_CLICK;
        return true;
    case BUTTON_LONG_PRESS_HOLD:
        *bsp_event = WT_BSP_BUTTON_EVENT_KEEPALIVE;
        return true;
    default:
        return false;
    }
}

static void wt_bsp_button_cleanup(wt_bsp_button_t button)
{
    if (button == NULL) {
        ESP_LOGE(TAG, "Invalid argument: button=%p", button);
        return;
    }

    if (button->handle == NULL) {
        return;
    }

    esp_err_t ret = iot_button_delete(button->handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "delete button failed: %s", esp_err_to_name(ret));
    }
    button->handle = NULL;

}

#endif // WT_BSP_BUTTON_ENABLE_IS_ENABLED
