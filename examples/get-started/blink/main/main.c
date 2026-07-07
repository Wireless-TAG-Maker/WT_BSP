#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wt_bsp.h"
#include "esp_random.h"

static const char *TAG = "blink";

static bool s_config_button_triggered = false;

/* Functions required by lvgl_demo_ui.c implemented using BSP */

void example_set_led_color(uint8_t r, uint8_t g, uint8_t b)
{
    wt_bsp_rgb_t rgb = wt_bsp_get_rgb();
    if (rgb) {
        wt_bsp_rgb_set_pixel(rgb, 0, (wt_bsp_rgb_color_t){r, g, b});
        wt_bsp_rgb_refresh(rgb);
    }
}

static void config_button_event_cb(wt_bsp_button_t button,
                                   wt_bsp_button_event_t event,
                                   void *user_data)
{
    (void)button;
    (void)user_data;

    if (event == WT_BSP_BUTTON_EVENT_CLICK) {
        s_config_button_triggered = !s_config_button_triggered;
    }
}

void app_main(void)
{
    uint32_t r = 255;
    uint32_t g = 255;
    uint32_t b = 255;

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
            ESP_LOGI(TAG, "Long press the onboard button to enter configuration mode");
        }
    }

    while (1) {

        if(s_config_button_triggered) {
            r = 0;
            g = 0;
            b = 0;
        } else {
            r = esp_random() % 255;
            g = esp_random() % 255;
            b = esp_random() % 255;
        }

        example_set_led_color(r, g, b);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "Initializing Wireless-Tag BSP");
    }
}
