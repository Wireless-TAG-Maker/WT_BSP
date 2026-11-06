/**
 * @file camera_uvc.h
 * @author Wireless-Tag
 * @brief Application camera-to-UVC composition shared by repository examples.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __CAMERA_UVC_H__
#define __CAMERA_UVC_H__

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Connect an available BSP camera source to the independent UVC device.
 *
 * Call after wt_bsp_init(). The application owns bridge buffers and chooses the
 * source: CSI/JPEG when CSI is enabled, otherwise the board's JPEG camera API.
 * @return ESP_OK on success/repeated start, ESP_ERR_NOT_SUPPORTED when the
 *         required features are disabled, or a source/USB initialization error.
 */
esp_err_t camera_uvc_start(void);

/**
 * @brief Stop USB delivery and release application bridge resources.
 * @return ESP_OK on success or an underlying stop error. On error buffers are
 *         retained for retry. Call successfully before wt_bsp_deinit().
 */
esp_err_t camera_uvc_stop(void);

/**
 * @brief Report whether the host currently owns the bridge's camera stream.
 * @return true during host streaming, false when idle or unavailable.
 *         Applications should also serialize their camera consumers.
 */
bool camera_uvc_is_streaming(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __CAMERA_UVC_H__
