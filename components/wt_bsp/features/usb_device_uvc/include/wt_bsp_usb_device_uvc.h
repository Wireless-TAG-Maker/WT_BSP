/**
 * @file wt_bsp_usb_device_uvc.h
 * @author Wireless-Tag
 * @brief USB Device UVC interface.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __WT_BSP_USB_DEVICE_UVC_H__
#define __WT_BSP_USB_DEVICE_UVC_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/** @brief Board-owned UVC object. Applications must not release this handle. */
typedef struct wt_bsp_usb_device_uvc_obj_t *wt_bsp_usb_device_uvc_t;

#if WT_BSP_USB_DEVICE_UVC_ENABLED

#include <stdbool.h>
#include "esp_err.h"
#include "usb_device_uvc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Start USB UVC with an application-provided video frame source.
 *
 * The BSP prepares the object without starting USB or a capture peripheral.
 * This call copies the camera 0 configuration and starts the USB device. The
 * application owns all callbacks, their context and the transfer buffer; they
 * must remain valid until @ref wt_bsp_usb_device_uvc_stop succeeds. Configure
 * and prepare the capture/encoding pipeline separately in the application.
 * Call from an application control task, never from a USB callback. No other
 * independently initialized USB device stack may be active concurrently.
 * Serialize start, stop and BSP teardown operations.
 *
 * @param[in,out] uvc Board-owned UVC handle, obtained from the BSP.
 * @param[in] stream Camera 0 callbacks and nonempty transfer buffer, copied here.
 * @return ESP_OK after the USB device starts.
 * @return ESP_ERR_INVALID_ARG for a missing handle, buffer or callback.
 * @return ESP_ERR_INVALID_STATE when the BSP object is unavailable, already
 *         started, or another object owns the UVC device stack.
 * @return The underlying USB configuration or initialization error on failure.
 */
esp_err_t wt_bsp_usb_device_uvc_start(wt_bsp_usb_device_uvc_t uvc,
                                     const uvc_device_config_t *stream);

/**
 * @brief Stop USB UVC before the application releases its frame source.
 *
 * The application must allow frame-return and stop callbacks to finish while
 * this call waits. Call outside USB callbacks and without holding locks needed
 * by those callbacks. A shutdown error preserves ownership and the borrowed
 * configuration; retain the buffers and callback context and retry.
 *
 * @param[in,out] uvc Board-owned UVC handle, never freed by this function.
 * @return ESP_OK after stopping, or when the device is not started.
 * @return ESP_ERR_INVALID_ARG for NULL.
 * @return ESP_ERR_INVALID_STATE for an inconsistent USB owner.
 * @return The underlying USB shutdown error, with frame-source ownership intact.
 */
esp_err_t wt_bsp_usb_device_uvc_stop(wt_bsp_usb_device_uvc_t uvc);

/**
 * @brief Report whether this object owns a started UVC device stack.
 * @param[in] uvc Board-owned UVC handle, or NULL.
 * @return true after start succeeds and before stop succeeds; false for NULL,
 *         an uninitialized object, or a stopped device. Host streaming or
 *         enumeration is not required for a started device.
 */
bool wt_bsp_usb_device_uvc_is_started(wt_bsp_usb_device_uvc_t uvc);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_USB_DEVICE_UVC_ENABLED

#endif // __WT_BSP_USB_DEVICE_UVC_H__
