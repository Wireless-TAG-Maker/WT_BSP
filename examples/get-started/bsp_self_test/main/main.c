/**
 * @file main.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-05-10
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wt_bsp.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

typedef struct {
    wt_bsp_rgb_t rgb;
    bool led_on;
} bsp_self_test_ctx_t;

/* ==================== [Static Prototypes] ================================= */

static void bsp_self_test_button_event_cb(wt_bsp_button_t button,
                                          wt_bsp_button_event_t event,
                                          void *user_data);
static esp_err_t bsp_self_test_set_led(bsp_self_test_ctx_t *ctx, bool on);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "bsp_self_test";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

void app_main(void)
{
    esp_err_t ret = ESP_OK;
    bsp_self_test_ctx_t ctx = {0};

    ret = wt_bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bsp init failed: %s", esp_err_to_name(ret));
        return;
    }

    ctx.rgb = wt_bsp_get_rgb();
    if (ctx.rgb == NULL) {
        ESP_LOGE(TAG, "rgb get failed");
        return;
    }

    wt_bsp_button_t button = wt_bsp_get_button();
    if (button == NULL) {
        ESP_LOGE(TAG, "button get failed");
        return;
    }

    (void)bsp_self_test_set_led(&ctx, false);
    (void)wt_bsp_button_register_event_cb(button, bsp_self_test_button_event_cb, &ctx);

    ESP_LOGI(TAG, "WT BSP self test ready");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ==================== [Static Functions] ================================== */

static void bsp_self_test_button_event_cb(wt_bsp_button_t button,
                                          wt_bsp_button_event_t event,
                                          void *user_data)
{
    bsp_self_test_ctx_t *ctx = (bsp_self_test_ctx_t *)user_data;

    (void)button;

    if (ctx == NULL || event != WT_BSP_BUTTON_EVENT_CLICK) {
        return;
    }

    ctx->led_on = !ctx->led_on;
    (void)bsp_self_test_set_led(ctx, ctx->led_on);
    ESP_LOGI(TAG, "rgb led %s", ctx->led_on ? "on" : "off");
}

static esp_err_t bsp_self_test_set_led(bsp_self_test_ctx_t *ctx, bool on)
{
    wt_bsp_rgb_color_t color = on ? (wt_bsp_rgb_color_t){ .r = 0x20, .g = 0x20, .b = 0x20 } :
                                    (wt_bsp_rgb_color_t){ .r = 0x00, .g = 0x00, .b = 0x00 };

    if (ctx == NULL || ctx->rgb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return wt_bsp_rgb_set_color(ctx->rgb, color);
}
