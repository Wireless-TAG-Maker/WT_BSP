/**
 * @file main.c
 * @author Wireless-Tag
 * @brief TINY 板卡 CSI/DVP 摄像头 USB Device UVC 示例。
 * @version 0.1
 * @date 2026-07-28
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp.h"
#include "camera_uvc.h"

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
    // Suppress the non-fatal per-frame subwindow alignment warning while keeping AWB errors visible.
    esp_log_level_set("ISP_AWB", ESP_LOG_ERROR);

    ESP_LOGI(TAG, "Initializing Wireless-Tag BSP");

    esp_err_t ret = wt_bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP initialization failed: %s", esp_err_to_name(ret));
        return;
    }

#if !WT_BSP_USB_DEVICE_UVC_ENABLED
    ESP_LOGE(TAG, "Enable USB Device UVC in menuconfig");
    return;
#endif

#if WT_BSP_CSI_ENABLED
    if (wt_bsp_get_csi() == NULL) {
        ESP_LOGE(TAG, "CSI camera is unavailable");
        return;
    }
#endif

    ret = camera_uvc_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera-to-UVC bridge failed: %s", esp_err_to_name(ret));
        esp_err_t stop_ret = camera_uvc_stop();
        if (stop_ret == ESP_OK) {
            wt_bsp_deinit();
        } else {
            ESP_LOGE(TAG, "Bridge cleanup failed; retaining BSP resources: %s", esp_err_to_name(stop_ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Application camera-to-UVC bridge is ready. Connect USB OTG to the host.");
}

/* ==================== [Static Functions] ================================== */
