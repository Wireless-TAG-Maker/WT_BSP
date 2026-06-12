#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wt_bsp.h"
#include "lv_demos.h"

static const char *TAG = "dsi_example";

static void touch_callback(lv_indev_t *indev, lv_indev_data_t *data)
{
    wt_bsp_touch_t touch = (wt_bsp_touch_t)lv_indev_get_user_data(indev);
    uint16_t x, y;
    uint8_t num;

    if (wt_bsp_touch_get_coordinates(touch, &x, &y, NULL, &num, 1)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing BSP");
    ESP_ERROR_CHECK(wt_bsp_init());

    wt_bsp_dsi_t dsi = wt_bsp_get_dsi();
    if (dsi == NULL) {
        ESP_LOGE(TAG, "Failed to get DSI handle");
        return;
    }

    ESP_LOGI(TAG, "Starting LVGL");
    lv_display_t *display = wt_bsp_dsi_lvgl_start(dsi, NULL);
    if (display == NULL) {
        ESP_LOGE(TAG, "Failed to start LVGL");
        return;
    }

    // Initialize Touch if enabled
    wt_bsp_touch_t touch = wt_bsp_get_touch();
    if (touch) {
        ESP_LOGI(TAG, "Starting Touch");
        lv_indev_t *indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_display(indev, display);
        lv_indev_set_user_data(indev, touch);
        lv_indev_set_read_cb(indev, touch_callback);
    }

    ESP_LOGI(TAG, "Turning on display and setting brightness");
    wt_bsp_dsi_display_on(dsi);
    wt_bsp_dsi_set_brightness(dsi, 100);

    // Use LVGL API to create UI
    // Note: LVGL API calls must be protected by lock/unlock
    if (wt_bsp_dsi_lvgl_lock(0)) {
        lv_demo_widgets();
        wt_bsp_dsi_lvgl_unlock();
    }

    ESP_LOGI(TAG, "DSI example ready");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
