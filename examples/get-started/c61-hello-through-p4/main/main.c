/**
 * @file main.c
 * @author Wireless-Tag
 * @brief ESP32-C61 hello example flashed through the ESP32-P4 bridge.
 * @version 0.1
 * @date 2026-07-07
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "c61_hello";

void app_main(void)
{
    esp_chip_info_t chip_info = {0};
    uint32_t flash_size = 0;

    esp_chip_info(&chip_info);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        flash_size = 0;
    }

    ESP_LOGI(TAG, "Hello Wireless-tag from ESP32-C61!");
    ESP_LOGI(TAG, "Chip cores: %d, revision: v%d.%d, flash: %lu MB",
             chip_info.cores,
             chip_info.revision / 100,
             chip_info.revision % 100,
             (unsigned long)(flash_size / (1024 * 1024)));

    while (true) {
        ESP_LOGI(TAG, "Hello Wireless-tag");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
