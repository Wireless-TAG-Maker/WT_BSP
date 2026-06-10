#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wt_bsp.h"

static const char *TAG = "dsi_example";

static void touch_callback(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    wt_bsp_touch_t touch = (wt_bsp_touch_t)indev_drv->user_data;
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
        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.disp = display;
        indev_drv.user_data = touch;
        indev_drv.read_cb = touch_callback;
        lv_indev_drv_register(&indev_drv);
    }

    ESP_LOGI(TAG, "Turning on display and setting brightness");
    wt_bsp_dsi_display_on(dsi);
    wt_bsp_dsi_set_brightness(dsi, 100);

    // Use LVGL API to create UI
    // Note: LVGL API calls must be protected by lock/unlock
    if (wt_bsp_dsi_lvgl_lock(0)) {
        lv_obj_t *scr = lv_scr_act();
        
        lv_obj_t *label = lv_label_create(scr);
        lv_label_set_text(label, "Hello Wireless-Tag DSI!");
        lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, -40);

        if (touch) {
            lv_obj_t *touch_label = lv_label_create(scr);
            lv_label_set_text(touch_label, "Touch supported");
            lv_obj_align(touch_label, LV_ALIGN_CENTER, 0, 40);
        }

        wt_bsp_dsi_lvgl_unlock();
    }

    ESP_LOGI(TAG, "DSI example ready");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
