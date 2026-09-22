/**
 * @file main.c
 * @author Wireless-Tag
 * @brief USB CDC serial echo through the BSP interface.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

static const char *TAG = "usb_device_cdc";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

void app_main(void)
{
    esp_err_t ret = wt_bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP initialization failed: %s", esp_err_to_name(ret));
        return;
    }

#if WT_BSP_USB_DEVICE_CDC_ENABLED
    wt_bsp_usb_device_cdc_t cdc = wt_bsp_get_usb_device_cdc();
    if (cdc == NULL) {
        ESP_LOGE(TAG, "USB CDC is unavailable");
        return;
    }
    ESP_LOGI(TAG, "Connect the board USB OTG port and open the CDC port with DTR enabled for echo");
    uint8_t buffer[256];
    while (1) {
        size_t received = 0;
        ret = wt_bsp_usb_device_cdc_read(cdc, buffer, sizeof(buffer), &received);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "CDC read failed: %s", esp_err_to_name(ret));
            break;
        }

        // Preserve the unqueued suffix under host backpressure. USB I/O runs
        // in this application task, with no blocking work in USB callbacks.
        size_t offset = 0;
        while (offset < received) {
            size_t written = 0;
            ret = wt_bsp_usb_device_cdc_write(cdc, buffer + offset, received - offset, &written, 20);
            offset += written;
            if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT && ret != ESP_ERR_NOT_FINISHED) {
                break;
            }
            if (offset < received) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    wt_bsp_deinit();
#else
    ESP_LOGE(TAG, "Enable USB Device CDC in menuconfig");
#endif
}

/* ==================== [Static Functions] ================================== */
