#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wt_bsp.h"

static const char *TAG = "dsi_example";

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
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

        wt_bsp_dsi_lvgl_unlock();
    }

    ESP_LOGI(TAG, "DSI example ready");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
