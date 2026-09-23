/**
 * @file wt_bsp_camera.c
 * @author Wireless-Tag
 * @brief Public access to a board's serialized JPEG capture adapter.
 * @version 0.1
 * @date 2026-09-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_camera_port.h"

#if WT_BSP_CAMERA_ENABLED

#include <string.h>
#include "esp_log.h"

/* ==================== [Defines] =========================================== */
/* ==================== [Typedefs] ========================================== */
/* ==================== [Static Prototypes] ================================= */
/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_camera";

/* ==================== [Macros] ============================================ */
/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_camera_init(wt_bsp_camera_t camera, const wt_bsp_camera_info_t *info)
{
    if (camera == NULL || info == NULL || info->prepare == NULL ||
            info->capture == NULL || info->get_status == NULL) {
        ESP_LOGE(TAG, "Invalid camera adapter");
        return ESP_ERR_INVALID_ARG;
    }
    camera->info = *info;
    camera->is_initialized = true;
    return ESP_OK;
}

esp_err_t wt_bsp_camera_deinit(wt_bsp_camera_t camera)
{
    if (camera == NULL) {
        ESP_LOGE(TAG, "Camera is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    memset(camera, 0, sizeof(*camera));
    return ESP_OK;
}

esp_err_t wt_bsp_camera_prepare(wt_bsp_camera_t camera, wt_bsp_camera_format_t *format)
{
    if (camera == NULL || format == NULL) {
        ESP_LOGE(TAG, "Invalid camera prepare arguments");
        return ESP_ERR_INVALID_ARG;
    }
    return camera->is_initialized ? camera->info.prepare(format) : ESP_ERR_INVALID_STATE;
}

esp_err_t wt_bsp_camera_capture(wt_bsp_camera_t camera, wt_bsp_camera_frame_cb_t callback,
                              void *user_data)
{
    if (camera == NULL || callback == NULL) {
        ESP_LOGE(TAG, "Invalid camera capture arguments");
        return ESP_ERR_INVALID_ARG;
    }
    return camera->is_initialized ? camera->info.capture(callback, user_data) : ESP_ERR_INVALID_STATE;
}

esp_err_t wt_bsp_camera_get_status(wt_bsp_camera_t camera, wt_bsp_camera_status_t *status)
{
    if (camera == NULL || status == NULL) {
        ESP_LOGE(TAG, "Invalid camera status arguments");
        return ESP_ERR_INVALID_ARG;
    }
    return camera->is_initialized ? camera->info.get_status(status) : ESP_ERR_INVALID_STATE;
}

/* ==================== [Static Functions] ================================== */

#endif // WT_BSP_CAMERA_ENABLED
