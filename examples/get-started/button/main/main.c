#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wt_bsp.h"
#include "esp_random.h"

static const char *TAG = "button";

static wt_bsp_button_event_t s_config_button_event = -1;

static void config_button_event_cb(wt_bsp_button_t button,
                                   wt_bsp_button_event_t event,
                                   void *user_data)
{
    (void)button;
    (void)user_data;

    s_config_button_event = event;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing Wireless-Tag BSP");
    esp_err_t ret = wt_bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP init failed: %s", esp_err_to_name(ret));
        return;
    }

    wt_bsp_button_t config_button = wt_bsp_get_button();
    if (config_button == NULL) {
        ESP_LOGW(TAG, "Button not available; runtime configuration trigger disabled");
    } else {
        ret = wt_bsp_button_register_event_cb(config_button, config_button_event_cb, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register button callback: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGW(TAG, "Please operate the button");
        }
    }

    while (1) {
        if(s_config_button_event == WT_BSP_BUTTON_EVENT_PRESS) {
            ESP_LOGI(TAG, "Button press");
        } else if(s_config_button_event == WT_BSP_BUTTON_EVENT_RELEASE) {
            ESP_LOGI(TAG, "Button release");
        } else if(s_config_button_event == WT_BSP_BUTTON_EVENT_CLICK) {
            ESP_LOGI(TAG, "Button click");
        } else if(s_config_button_event == WT_BSP_BUTTON_EVENT_KEEPALIVE) {
            ESP_LOGI(TAG, "Button long press");
        }

        s_config_button_event = -1;

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
