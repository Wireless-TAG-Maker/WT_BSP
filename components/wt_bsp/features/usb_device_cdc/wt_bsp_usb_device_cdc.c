/**
 * @file wt_bsp_usb_device_cdc.c
 * @author Wireless-Tag
 * @brief USB Device CDC lifecycle and task-based serial I/O.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_usb_device_cdc_port.h"

#if WT_BSP_USB_DEVICE_CDC_ENABLED

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if CONFIG_WT_BSP_USB_DEVICE_COMPOSITE
#include "tusb.h"
#define WT_BSP_CDC_PORT_COUNT CFG_TUD_CDC
#else
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_default_config.h"
#define WT_BSP_CDC_PORT_COUNT CONFIG_TINYUSB_CDC_COUNT
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_usb_device_cdc";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_usb_device_cdc_init(wt_bsp_usb_device_cdc_t cdc,
                                    const wt_bsp_usb_device_cdc_info_t *info)
{
    if (cdc == NULL || info == NULL || info->port >= WT_BSP_CDC_PORT_COUNT) {
        ESP_LOGE(TAG, "Invalid CDC configuration");
        return ESP_ERR_INVALID_ARG;
    }
    if (cdc->is_initialized) {
        ESP_LOGW(TAG, "CDC is already initialized");
        return ESP_OK;
    }
    cdc->info = *info;
#if !CONFIG_WT_BSP_USB_DEVICE_COMPOSITE
    esp_err_t ret = ESP_OK;
    if (cdc->driver_installed) {
        ret = wt_bsp_usb_device_cdc_deinit(cdc);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    const tinyusb_config_t usb_config = TINYUSB_DEFAULT_CONFIG();
    ret = tinyusb_driver_install(&usb_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB driver: %s", esp_err_to_name(ret));
        return ret;
    }
    cdc->driver_installed = true;

    const tinyusb_config_cdcacm_t cdc_config = {
        .cdc_port = info->port,
    };
    ret = tinyusb_cdcacm_init(&cdc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize CDC: %s", esp_err_to_name(ret));
        wt_bsp_usb_device_cdc_deinit(cdc);
        return ret;
    }
#endif
    cdc->is_initialized = true;
    return ESP_OK;
}

esp_err_t wt_bsp_usb_device_cdc_deinit(wt_bsp_usb_device_cdc_t cdc)
{
    esp_err_t result = ESP_OK;
    if (cdc == NULL) {
        ESP_LOGE(TAG, "CDC is NULL");
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_WT_BSP_USB_DEVICE_COMPOSITE
    cdc->is_initialized = false;
#else
    esp_err_t ret = ESP_OK;
    if (cdc->is_initialized) {
        ret = tinyusb_cdcacm_deinit(cdc->info.port);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to deinitialize CDC: %s", esp_err_to_name(ret));
            result = ret;
        }
        cdc->is_initialized = false;
    }
    if (cdc->driver_installed) {
        ret = tinyusb_driver_uninstall();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to uninstall USB driver: %s", esp_err_to_name(ret));
            if (result == ESP_OK) {
                result = ret;
            }
        } else {
            cdc->driver_installed = false;
        }
    }
#endif
    return result;
}

esp_err_t wt_bsp_usb_device_cdc_read(wt_bsp_usb_device_cdc_t cdc, void *buffer,
                                    size_t capacity, size_t *bytes_read)
{
    if (bytes_read != NULL) {
        *bytes_read = 0;
    }
    if (cdc == NULL || buffer == NULL || capacity == 0 || bytes_read == NULL) {
        ESP_LOGE(TAG, "Invalid CDC read arguments");
        return ESP_ERR_INVALID_ARG;
    }
    if (!cdc->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
#if CONFIG_WT_BSP_USB_DEVICE_COMPOSITE
    *bytes_read = tud_cdc_n_read(cdc->info.port, buffer, capacity);
    return ESP_OK;
#else
    return tinyusb_cdcacm_read(cdc->info.port, buffer, capacity, bytes_read);
#endif
}

esp_err_t wt_bsp_usb_device_cdc_write(wt_bsp_usb_device_cdc_t cdc, const void *buffer,
                                     size_t length, size_t *bytes_written, uint32_t timeout_ms)
{
    if (bytes_written != NULL) {
        *bytes_written = 0;
    }
    if (cdc == NULL || (buffer == NULL && length != 0) || bytes_written == NULL) {
        ESP_LOGE(TAG, "Invalid CDC write arguments");
        return ESP_ERR_INVALID_ARG;
    }
    if (!cdc->is_initialized || !tud_cdc_n_connected(cdc->info.port)) {
        return ESP_ERR_INVALID_STATE;
    }
#if CONFIG_WT_BSP_USB_DEVICE_COMPOSITE
    if (length != 0) {
        *bytes_written = tud_cdc_n_write(cdc->info.port, buffer, length);
    }
    TickType_t started = xTaskGetTickCount();
    do {
        tud_cdc_n_write_flush(cdc->info.port);
        if (tud_cdc_n_write_available(cdc->info.port) == CFG_TUD_CDC_TX_BUFSIZE) {
            return ESP_OK;
        }
        if (!tud_cdc_n_connected(cdc->info.port)) {
            return ESP_ERR_INVALID_STATE;
        }
        if (timeout_ms == 0) {
            return ESP_ERR_NOT_FINISHED;
        }
        vTaskDelay(1);
    } while (xTaskGetTickCount() - started < pdMS_TO_TICKS(timeout_ms));
    return ESP_ERR_TIMEOUT;
#else
    if (length != 0) {
        *bytes_written = tinyusb_cdcacm_write_queue(cdc->info.port, buffer, length);
    }
    return tinyusb_cdcacm_write_flush(cdc->info.port, pdMS_TO_TICKS(timeout_ms));
#endif
}

bool wt_bsp_usb_device_cdc_is_mounted(wt_bsp_usb_device_cdc_t cdc)
{
    return cdc != NULL && cdc->is_initialized && tud_mounted();
}

bool wt_bsp_usb_device_cdc_is_connected(wt_bsp_usb_device_cdc_t cdc)
{
    return cdc != NULL && cdc->is_initialized && tud_cdc_n_connected(cdc->info.port);
}

/* ==================== [Static Functions] ================================== */

#endif // WT_BSP_USB_DEVICE_CDC_ENABLED
