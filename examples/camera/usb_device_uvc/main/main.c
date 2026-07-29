/**
 * @file main.c
 * @author Wireless-Tag
 * @brief ESP32-P4 TINY 板卡 CSI 摄像头 USB Device UVC 示例。
 * @version 0.1
 * @date 2026-07-28
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp.h"

#include "esp_log.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

static const char *TAG = "usb_device_uvc";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing Wireless-Tag BSP");

    esp_err_t ret = wt_bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP initialization failed: %s", esp_err_to_name(ret));
        return;
    }

    if (wt_bsp_get_csi() == NULL) {
        ESP_LOGE(TAG, "CSI camera is unavailable");
        return;
    }

    ESP_LOGI(TAG, "USB Device UVC is ready. Connect HUSB to the USB host.");
}

/* ==================== [Static Functions] ================================== */
