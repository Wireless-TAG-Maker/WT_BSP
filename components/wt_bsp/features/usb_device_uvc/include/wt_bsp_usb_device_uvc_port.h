/**
 * @file wt_bsp_usb_device_uvc_port.h
 * @author Wireless-Tag
 * @brief USB Device UVC board resource interface.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __WT_BSP_USB_DEVICE_UVC_PORT_H__
#define __WT_BSP_USB_DEVICE_UVC_PORT_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_usb_device_uvc.h"

#if WT_BSP_USB_DEVICE_UVC_ENABLED

#include <stdbool.h>
#include "esp_err.h"
#include "usb_device_uvc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/** @brief Static port-owned object; one object may own the USB UVC stack at a time. */
typedef struct wt_bsp_usb_device_uvc_obj_t {
    uvc_device_config_t stream;      /*!< Borrowed application frame source, valid only while started. */
    bool is_initialized;             /*!< Board object is available to the application. */
    bool is_started;                 /*!< USB tasks may use the borrowed frame source. */
} wt_bsp_usb_device_uvc_obj_t;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Prepare a board-owned UVC object without starting USB.
 *
 * The board supplies a zero-initialized static object.
 * No CSI/DVP capture, encoder, application frame source or USB worker is
 * initialized by this call. The application starts streaming explicitly.
 *
 * @param[in,out] uvc Port-owned UVC object, not freed by this function.
 * @return ESP_OK on success or repeated initialization of the object.
 * @return ESP_ERR_INVALID_ARG for a missing object.
 */
esp_err_t wt_bsp_usb_device_uvc_init(wt_bsp_usb_device_uvc_t uvc);

/**
 * @brief Stop USB UVC and invalidate the board-owned object.
 *
 * The caller must allow frame-return and stop callbacks to finish while this
 * call waits. Applications should stop and release their own bridge before
 * BSP teardown. On failure the object remains initialized: retain the callback
 * context and buffers and retry before releasing them.
 *
 * @param[in,out] uvc Port-owned UVC object, never freed by this function.
 * @return ESP_OK on success or when the object is not initialized.
 * @return ESP_ERR_INVALID_ARG for NULL.
 * @return ESP_ERR_INVALID_STATE for an inconsistent USB owner.
 * @return The underlying USB shutdown error, with ownership preserved.
 */
esp_err_t wt_bsp_usb_device_uvc_deinit(wt_bsp_usb_device_uvc_t uvc);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_USB_DEVICE_UVC_ENABLED

#endif // __WT_BSP_USB_DEVICE_UVC_PORT_H__
