/**
 * @file wt_bsp_usb_device_cdc.h
 * @author Wireless-Tag
 * @brief USB Device CDC serial interface.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __WT_BSP_USB_DEVICE_CDC_H__
#define __WT_BSP_USB_DEVICE_CDC_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"

/** @brief Board-owned CDC handle, valid until wt_bsp_deinit(). Do not free it. */
typedef struct wt_bsp_usb_device_cdc_obj_t *wt_bsp_usb_device_cdc_t;

#if WT_BSP_USB_DEVICE_CDC_ENABLED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Check whether a USB host configured the shared or standalone device.
 * @param[in] cdc Board-owned CDC handle, or NULL.
 * @return true after USB configuration, false when unavailable/unmounted.
 *         Opening a serial application is not required. Bus suspension alone
 *         does not clear this state; this is not a physical VBUS measurement.
 */
bool wt_bsp_usb_device_cdc_is_mounted(wt_bsp_usb_device_cdc_t cdc);

/**
 * @brief Check whether the host asserted CDC DTR (typically an open serial port).
 * @param[in] cdc Board-owned CDC handle, or NULL.
 * @return true if initialized with DTR asserted, otherwise false.
 */
bool wt_bsp_usb_device_cdc_is_connected(wt_bsp_usb_device_cdc_t cdc);

/**
 * @brief Read currently available bytes without blocking.
 *
 * Call from one application task. Do not deinitialize the BSP during I/O.
 * @param[in] cdc Board-owned CDC handle.
 * @param[out] buffer Destination buffer.
 * @param[in] capacity Nonzero destination capacity in bytes.
 * @param[out] bytes_read Bytes copied; zero when no data is available.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for invalid parameters,
 *         ESP_ERR_INVALID_STATE if uninitialized, or a driver error.
 */
esp_err_t wt_bsp_usb_device_cdc_read(wt_bsp_usb_device_cdc_t cdc, void *buffer,
                                    size_t capacity, size_t *bytes_read);

/**
 * @brief Queue bytes and flush the USB transmit buffer.
 *
 * Call from one application task, outside USB callbacks. A short write is
 * possible: retry only the unqueued suffix. Queued bytes may still be pending
 * when flushing times out. Success does not acknowledge host application reads.
 * @param[in] cdc Board-owned CDC handle, never released by this call.
 * @param[in] buffer Source buffer, required when length is nonzero.
 * @param[in] length Number of bytes to queue; zero only flushes pending data.
 * @param[out] bytes_written Bytes queued, including when flushing returns an error.
 * @param[in] timeout_ms Flush timeout in milliseconds; zero is nonblocking.
 * @return ESP_OK on flush completion, ESP_ERR_INVALID_ARG for invalid parameters,
 *         ESP_ERR_INVALID_STATE if uninitialized or the host has not asserted DTR,
 *         ESP_ERR_NOT_FINISHED if a nonblocking flush is pending,
 *         ESP_ERR_TIMEOUT on flush timeout, or a driver error.
 */
esp_err_t wt_bsp_usb_device_cdc_write(wt_bsp_usb_device_cdc_t cdc, const void *buffer,
                                     size_t length, size_t *bytes_written, uint32_t timeout_ms);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_USB_DEVICE_CDC_ENABLED

#endif // __WT_BSP_USB_DEVICE_CDC_H__
