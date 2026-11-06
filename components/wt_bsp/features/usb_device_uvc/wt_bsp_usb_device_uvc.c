/**
 * @file wt_bsp_usb_device_uvc.c
 * @author Wireless-Tag
 * @brief Independent USB Device UVC lifecycle with application frame sources.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_usb_device_uvc_port.h"

#if WT_BSP_USB_DEVICE_UVC_ENABLED

#include <string.h>
#include "esp_log.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_usb_uvc";
static wt_bsp_usb_device_uvc_t s_uvc_owner;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_usb_device_uvc_init(wt_bsp_usb_device_uvc_t uvc)
{
    if (uvc == NULL) {
        ESP_LOGE(TAG, "USB UVC object is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (uvc->is_initialized) {
        ESP_LOGW(TAG, "USB UVC object is already initialized");
        return ESP_OK;
    }

    memset(uvc, 0, sizeof(*uvc));
    uvc->is_initialized = true;
    return ESP_OK;
}

esp_err_t wt_bsp_usb_device_uvc_start(wt_bsp_usb_device_uvc_t uvc,
                                     const uvc_device_config_t *stream)
{
    esp_err_t ret = ESP_OK;

    if (uvc == NULL || stream == NULL || stream->uvc_buffer == NULL ||
            stream->uvc_buffer_size == 0 || stream->start_cb == NULL ||
            stream->stop_cb == NULL || stream->fb_get_cb == NULL ||
            stream->fb_return_cb == NULL) {
        ESP_LOGE(TAG, "Invalid USB UVC frame-source configuration");
        return ESP_ERR_INVALID_ARG;
    }
    if (!uvc->is_initialized || uvc->is_started) {
        ESP_LOGE(TAG, "USB UVC object is unavailable or already started");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_uvc_owner != NULL) {
        ESP_LOGE(TAG, "Another object owns the USB UVC stack");
        return ESP_ERR_INVALID_STATE;
    }

    uvc->stream = *stream;
    ret = uvc_device_config(0, &uvc->stream);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure USB UVC: %s", esp_err_to_name(ret));
        memset(&uvc->stream, 0, sizeof(uvc->stream));
        return ret;
    }

    ret = uvc_device_init();
    if (ret != ESP_OK) {
        // usb_device_uvc 1.3 starts worker tasks only after successful USB init.
        ESP_LOGE(TAG, "Failed to initialize USB UVC: %s", esp_err_to_name(ret));
        memset(&uvc->stream, 0, sizeof(uvc->stream));
        return ret;
    }

    uvc->is_started = true;
    s_uvc_owner = uvc;
    return ESP_OK;
}

esp_err_t wt_bsp_usb_device_uvc_stop(wt_bsp_usb_device_uvc_t uvc)
{
    esp_err_t ret = ESP_OK;

    if (uvc == NULL) {
        ESP_LOGE(TAG, "USB UVC object is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!uvc->is_started) {
        return ESP_OK;
    }
    if (s_uvc_owner != uvc) {
        ESP_LOGE(TAG, "USB UVC owner does not match initialized object");
        return ESP_ERR_INVALID_STATE;
    }

    ret = uvc_device_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop USB UVC: %s", esp_err_to_name(ret));
        return ret;
    }

    s_uvc_owner = NULL;
    uvc->is_started = false;
    memset(&uvc->stream, 0, sizeof(uvc->stream));
    return ESP_OK;
}

bool wt_bsp_usb_device_uvc_is_started(wt_bsp_usb_device_uvc_t uvc)
{
    return uvc != NULL && uvc->is_initialized && uvc->is_started && s_uvc_owner == uvc;
}

esp_err_t wt_bsp_usb_device_uvc_deinit(wt_bsp_usb_device_uvc_t uvc)
{
    esp_err_t ret = ESP_OK;

    if (uvc == NULL) {
        ESP_LOGE(TAG, "USB UVC object is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ret = wt_bsp_usb_device_uvc_stop(uvc);
    if (ret != ESP_OK) {
        return ret;
    }
    memset(uvc, 0, sizeof(*uvc));
    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

#endif // WT_BSP_USB_DEVICE_UVC_ENABLED
