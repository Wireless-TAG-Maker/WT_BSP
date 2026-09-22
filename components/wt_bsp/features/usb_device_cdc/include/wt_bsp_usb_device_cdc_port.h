/**
 * @file wt_bsp_usb_device_cdc_port.h
 * @author Wireless-Tag
 * @brief USB Device CDC board port interface.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __WT_BSP_USB_DEVICE_CDC_PORT_H__
#define __WT_BSP_USB_DEVICE_CDC_PORT_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_usb_device_cdc.h"

#if WT_BSP_USB_DEVICE_CDC_ENABLED

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/** @brief CDC configuration on the SoC default USB OTG port. */
typedef struct {
    uint8_t port; /*!< CDC interface index; composite mode currently provides port 0. */
} wt_bsp_usb_device_cdc_info_t;

/** @brief Static board object owning the TinyUSB driver and CDC interface. */
typedef struct wt_bsp_usb_device_cdc_obj_t {
    wt_bsp_usb_device_cdc_info_t info; /*!< Board configuration. */
    bool driver_installed;            /*!< USB driver is owned by this object. */
    bool is_initialized;              /*!< CDC interface is ready for I/O. */
} wt_bsp_usb_device_cdc_obj_t;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Initialize a CDC object, owning USB only in standalone mode.
 * @param[in,out] cdc Static board object. In composite mode the board owns USB;
 *                    otherwise no other USB stack may be running.
 * @param[in] info CDC configuration, copied by this call.
 * @return ESP_OK on success or repeated initialization, ESP_ERR_INVALID_ARG for
 *         invalid parameters, or the TinyUSB initialization error after cleanup.
 */
esp_err_t wt_bsp_usb_device_cdc_init(wt_bsp_usb_device_cdc_t cdc,
                                    const wt_bsp_usb_device_cdc_info_t *info);

/**
 * @brief Release CDC and USB resources, including partially initialized state.
 * @param[in,out] cdc Board object. Application I/O must already have stopped.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for NULL, or the first driver error.
 */
esp_err_t wt_bsp_usb_device_cdc_deinit(wt_bsp_usb_device_cdc_t cdc);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_USB_DEVICE_CDC_ENABLED

#endif // __WT_BSP_USB_DEVICE_CDC_PORT_H__
