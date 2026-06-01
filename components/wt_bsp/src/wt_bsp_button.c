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

#include "wt_bsp_button.h"

#if WT_BSP_BUTTON_ENABLE_IS_ENABLED

#include <stdlib.h>
#include "esp_log.h"
#include "driver/gpio.h"

/* ==================== [Defines] =========================================== */

#define WT_BSP_BUTTON_DEFAULT_POLL_PERIOD_MS       10U
#define WT_BSP_BUTTON_DEFAULT_DEBOUNCE_PRESS_MS    20U
#define WT_BSP_BUTTON_DEFAULT_DEBOUNCE_RELEASE_MS  20U
#define WT_BSP_BUTTON_DEFAULT_CLICK_MIN_MS         20U
#define WT_BSP_BUTTON_DEFAULT_CLICK_MAX_MS         500U
#define WT_BSP_BUTTON_DEFAULT_MULTI_CLICK_MS       400U
#define WT_BSP_BUTTON_DEFAULT_KEEPALIVE_PERIOD_MS  1000U
#define WT_BSP_BUTTON_DEFAULT_MAX_CLICK_COUNT      3U

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static uint8_t wt_bsp_button_lwbtn_get_state(struct lwbtn *lwobj, struct lwbtn_btn *btn);
static void wt_bsp_button_lwbtn_event(struct lwbtn *lwobj, struct lwbtn_btn *btn, lwbtn_evt_t evt);
static void wt_bsp_button_timer_cb(void *arg);
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

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << button->info.gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = (button->info.active_level == WT_BSP_BUTTON_ACTIVE_LOW) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (button->info.active_level == WT_BSP_BUTTON_ACTIVE_HIGH) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "button gpio config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    button->lwbtn_btn.arg = button;
    if (!lwbtn_init_ex(&button->lwbtn, &button->lwbtn_btn, 1,
                       wt_bsp_button_lwbtn_get_state,
                       wt_bsp_button_lwbtn_event)) {
        wt_bsp_button_cleanup(button);
        return ESP_FAIL;
    }

    button->lwbtn_btn.time_debounce = WT_BSP_BUTTON_DEFAULT_DEBOUNCE_PRESS_MS;
    button->lwbtn_btn.time_debounce_release = WT_BSP_BUTTON_DEFAULT_DEBOUNCE_RELEASE_MS;
    button->lwbtn_btn.time_click_pressed_min = WT_BSP_BUTTON_DEFAULT_CLICK_MIN_MS;
    button->lwbtn_btn.time_click_pressed_max = WT_BSP_BUTTON_DEFAULT_CLICK_MAX_MS;
    button->lwbtn_btn.time_click_multi_max = WT_BSP_BUTTON_DEFAULT_MULTI_CLICK_MS;
    button->lwbtn_btn.time_keepalive_period = WT_BSP_BUTTON_DEFAULT_KEEPALIVE_PERIOD_MS;
    button->lwbtn_btn.max_consecutive = WT_BSP_BUTTON_DEFAULT_MAX_CLICK_COUNT;

    esp_timer_create_args_t timer_args = {
        .callback = wt_bsp_button_timer_cb,
        .arg = button,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "wt_bsp_btn",
    };

    ret = esp_timer_create(&timer_args, &button->timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "button timer create failed: %s", esp_err_to_name(ret));
        wt_bsp_button_cleanup(button);
        return ret;
    }

    ret = esp_timer_start_periodic(button->timer, WT_BSP_BUTTON_DEFAULT_POLL_PERIOD_MS * 1000ULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "button timer start failed: %s", esp_err_to_name(ret));
        wt_bsp_button_cleanup(button);
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

    return lwbtn_reset(&button->lwbtn, &button->lwbtn_btn) ? ESP_OK : ESP_FAIL;
}

uint8_t wt_bsp_button_get_click_count(wt_bsp_button_t button)
{
    if (button == NULL) {
        ESP_LOGE(TAG, "Invalid argument: button=%p", button);
        return 0;
    }

    return lwbtn_click_get_count(&button->lwbtn_btn);
}

uint16_t wt_bsp_button_get_keepalive_count(wt_bsp_button_t button)
{
    if (button == NULL) {
        ESP_LOGE(TAG, "Invalid argument: button=%p", button);
        return 0;
    }

    return lwbtn_keepalive_get_count(&button->lwbtn_btn);
}

/* ==================== [Static Functions] ================================== */

static uint8_t wt_bsp_button_lwbtn_get_state(struct lwbtn *lwobj, struct lwbtn_btn *btn)
{
    wt_bsp_button_t button = (wt_bsp_button_t)btn->arg;

    (void)lwobj;

    return (gpio_get_level(button->info.gpio_num) == (int)button->info.active_level);
}

static void wt_bsp_button_lwbtn_event(struct lwbtn *lwobj, struct lwbtn_btn *btn, lwbtn_evt_t evt)
{
    wt_bsp_button_t button = (wt_bsp_button_t)btn->arg;

    (void)lwobj;

    if (button == NULL || button->event_cb == NULL) {
        ESP_LOGE(TAG, "Invalid argument: button=%p", button);
        return;
    }

    button->event_cb(button, evt, button->user_data);
}

static void wt_bsp_button_timer_cb(void *arg)
{
    wt_bsp_button_t button = (wt_bsp_button_t)arg;

    if (button == NULL) {
        return;
    }

    (void)lwbtn_process_ex(&button->lwbtn, (lwbtn_time_t)(esp_timer_get_time() / 1000ULL));
}

static void wt_bsp_button_cleanup(wt_bsp_button_t button)
{
    if (button == NULL) {
        ESP_LOGE(TAG, "Invalid argument: button=%p", button);
        return;
    }

    if (button->timer == NULL) {
        return;
    }

    esp_err_t ret = esp_timer_stop(button->timer);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "button timer stop failed: %s", esp_err_to_name(ret));
    }
    ret = esp_timer_delete(button->timer);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "button timer delete failed: %s", esp_err_to_name(ret));
    }
    button->timer = NULL;

}

#endif // WT_BSP_BUTTON_ENABLE_IS_ENABLED
