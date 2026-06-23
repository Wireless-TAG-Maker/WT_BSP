#include <stdio.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "driver/gpio.h"
#include "esp_log.h"
#include "wt_bsp.h"
#include "esp_random.h"

static const char *TAG = "blink";

/* Functions required by lvgl_demo_ui.c implemented using BSP */

void example_set_led_color(uint8_t r, uint8_t g, uint8_t b)
{
    wt_bsp_rgb_t rgb = wt_bsp_get_rgb();
    if (rgb) {
        wt_bsp_rgb_set_pixel(rgb, 0, (wt_bsp_rgb_color_t){r, g, b});
        wt_bsp_rgb_refresh(rgb);
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

    while (1) {

        r = esp_random() % 255;
        g = esp_random() % 255;
        b = esp_random() % 255;
    
        example_set_led_color(r, g, b);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
