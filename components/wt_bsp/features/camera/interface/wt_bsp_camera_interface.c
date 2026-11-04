/**
 * @file wt_bsp_camera_interface.c
 * @author Wireless-Tag
 * @brief Registered camera driver routing and common property lookup.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_camera_interface.h"

#if WT_BSP_CAMERA_ENABLED

#include <string.h>
#include "esp_log.h"

/* ==================== [Defines] =========================================== */
/* ==================== [Typedefs] ========================================== */
/* ==================== [Static Prototypes] ================================= */

static esp_err_t camera_validate_properties(const wt_bsp_camera_info_t *info);
static bool camera_ops_complete(const wt_bsp_camera_ops_t *ops);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_camera_interface";

extern const wt_bsp_camera_driver_t __wt_bsp_camera_driver_array_start[];
extern const wt_bsp_camera_driver_t __wt_bsp_camera_driver_array_end[];

/* ==================== [Macros] ============================================ */
/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_camera_interface_init(wt_bsp_camera_t camera, const wt_bsp_camera_ops_t **ops)
{
    if (camera == NULL || ops == NULL || camera->info.sensor_name == NULL ||
            camera->info.sensor_name[0] == '\0') {
        ESP_LOGE(TAG, "Invalid camera driver selection arguments");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = camera_validate_properties(&camera->info);
    if (ret != ESP_OK) {
        return ret;
    }

    const wt_bsp_camera_driver_t *selected = NULL;
    for (const wt_bsp_camera_driver_t *driver = __wt_bsp_camera_driver_array_start;
            driver < __wt_bsp_camera_driver_array_end; driver++) {
        if (driver->name == NULL || driver->name[0] == '\0' || driver->init == NULL) {
            ESP_LOGE(TAG, "Camera driver registry contains an invalid registration");
            return ESP_ERR_INVALID_STATE;
        }
        for (const wt_bsp_camera_driver_t *previous = __wt_bsp_camera_driver_array_start;
                previous < driver; previous++) {
            if (strcmp(previous->name, driver->name) == 0) {
                ESP_LOGE(TAG, "Duplicate camera driver registration: %s", driver->name);
                return ESP_ERR_INVALID_STATE;
            }
        }
        if (strcmp(driver->name, camera->info.sensor_name) == 0) {
            selected = driver;
        }
    }
    if (selected == NULL) {
        ESP_LOGE(TAG, "No registered camera driver named %s", camera->info.sensor_name);
        return ESP_ERR_NOT_FOUND;
    }

    const wt_bsp_camera_ops_t *selected_ops = NULL;
    ret = selected->init(camera, &camera->info, &selected_ops);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera driver %s initialization failed: %s", selected->name, esp_err_to_name(ret));
        return ret;
    }
    if (!camera_ops_complete(selected_ops)) {
        ESP_LOGE(TAG, "Camera driver %s returned incomplete operations", selected->name);
        if (selected_ops != NULL && selected_ops->deinit != NULL) {
            esp_err_t cleanup = selected_ops->deinit(camera);
            if (cleanup != ESP_OK) {
                ESP_LOGE(TAG, "Invalid camera driver cleanup failed: %s", esp_err_to_name(cleanup));
            }
        }
        return ESP_ERR_INVALID_STATE;
    }
    *ops = selected_ops;
    return ESP_OK;
}

esp_err_t wt_bsp_camera_info_get_property(const wt_bsp_camera_info_t *info,
                                        const char *key, int32_t *value)
{
    if (info == NULL || key == NULL || key[0] == '\0' || value == NULL) {
        ESP_LOGE(TAG, "Invalid camera property lookup arguments");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = camera_validate_properties(info);
    if (ret != ESP_OK) {
        return ret;
    }
    for (size_t i = 0; i < info->property_count; i++) {
        if (strcmp(info->properties[i].key, key) == 0) {
            *value = info->properties[i].value;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

/* ==================== [Static Functions] ================================== */

static esp_err_t camera_validate_properties(const wt_bsp_camera_info_t *info)
{
    if (info->property_count > 0 && info->properties == NULL) {
        ESP_LOGE(TAG, "Camera property array is missing");
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < info->property_count; i++) {
        const char *key = info->properties[i].key;
        if (key == NULL || key[0] == '\0') {
            ESP_LOGE(TAG, "Camera property has an empty key");
            return ESP_ERR_INVALID_ARG;
        }
        for (size_t j = 0; j < i; j++) {
            if (strcmp(info->properties[j].key, key) == 0) {
                ESP_LOGE(TAG, "Duplicate camera property: %s", key);
                return ESP_ERR_INVALID_ARG;
            }
        }
    }
    return ESP_OK;
}

static bool camera_ops_complete(const wt_bsp_camera_ops_t *ops)
{
    return ops != NULL && ops->deinit != NULL && ops->prepare != NULL &&
           ops->capture != NULL && ops->get_status != NULL;
}

#endif // WT_BSP_CAMERA_ENABLED
