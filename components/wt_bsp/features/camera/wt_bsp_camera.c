/**
 * @file wt_bsp_camera.c
 * @author Wireless-Tag
 * @brief Reusable JPEG camera lifecycle and public capture API.
 * @version 0.1
 * @date 2026-09-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_camera_port.h"

#if WT_BSP_CAMERA_ENABLED

#include "interface/wt_bsp_camera_interface.h"

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
    esp_err_t ret = ESP_OK;

    if (camera == NULL || info == NULL || info->sensor_name == NULL ||
            info->sensor_name[0] == '\0' || info->format.width == 0 ||
            info->format.height == 0 || info->format.fps == 0) {
        ESP_LOGE(TAG, "Invalid camera configuration");
        return ESP_ERR_INVALID_ARG;
    }
    if (camera->is_initialized) {
        ESP_LOGW(TAG, "Camera is already initialized");
        return ESP_OK;
    }

    memset(camera, 0, sizeof(*camera));
    camera->info = *info;
    ret = wt_bsp_camera_interface_init(camera, &camera->ops);
    if (ret != ESP_OK) {
        memset(camera, 0, sizeof(*camera));
        return ret;
    }
    camera->is_initialized = true;
    return ESP_OK;
}

esp_err_t wt_bsp_camera_deinit(wt_bsp_camera_t camera)
{
    if (camera == NULL) {
        ESP_LOGE(TAG, "Camera is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!camera->is_initialized) {
        return ESP_OK;
    }
    esp_err_t ret = camera->ops->deinit(camera);
    if (ret == ESP_OK) {
        memset(camera, 0, sizeof(*camera));
    }
    return ret;
}

esp_err_t wt_bsp_camera_prepare(wt_bsp_camera_t camera, wt_bsp_camera_format_t *format)
{
    if (camera == NULL || format == NULL) {
        ESP_LOGE(TAG, "Invalid camera prepare arguments");
        return ESP_ERR_INVALID_ARG;
    }
    return camera->is_initialized ? camera->ops->prepare(camera, format) : ESP_ERR_INVALID_STATE;
}

esp_err_t wt_bsp_camera_capture(wt_bsp_camera_t camera, wt_bsp_camera_frame_cb_t callback,
                              void *user_data)
{
    if (camera == NULL || callback == NULL) {
        ESP_LOGE(TAG, "Invalid camera capture arguments");
        return ESP_ERR_INVALID_ARG;
    }
    return camera->is_initialized ? camera->ops->capture(camera, callback, user_data) : ESP_ERR_INVALID_STATE;
}

esp_err_t wt_bsp_camera_get_status(wt_bsp_camera_t camera, wt_bsp_camera_status_t *status)
{
    if (camera == NULL || status == NULL) {
        ESP_LOGE(TAG, "Invalid camera status arguments");
        return ESP_ERR_INVALID_ARG;
    }
    return camera->is_initialized ? camera->ops->get_status(camera, status) : ESP_ERR_INVALID_STATE;
}

/* ==================== [Static Functions] ================================== */

#endif // WT_BSP_CAMERA_ENABLED
