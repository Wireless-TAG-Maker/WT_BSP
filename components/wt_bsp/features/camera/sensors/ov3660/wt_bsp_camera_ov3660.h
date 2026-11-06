/**
 * @file wt_bsp_camera_ov3660.h
 * @author Wireless-Tag
 * @brief OV3660 driver factory using the unified camera configuration.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __WT_BSP_CAMERA_OV3660_H__
#define __WT_BSP_CAMERA_OV3660_H__

/* ==================== [Includes] ========================================== */

#include "../../interface/wt_bsp_camera_interface.h"

#if WT_BSP_CAMERA_ENABLED && CONFIG_WT_BSP_CAMERA_SENSOR_OV3660

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */
/* ==================== [Typedefs] ========================================== */
/* ==================== [Macros] ============================================ */
/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Initialize the OV3660 driver through the unified camera interface.
 *
 * The interface selects this registered factory using sensor_name="ov3660".
 * The supported profile is JPEG 640x480 at 25 fps and a 20 MHz XCLK. The common
 * properties must contain exactly one ae_level, jpeg_quality, saturation,
 * sharpness and denoise value, in any order. All property storage is borrowed
 * until camera deinitialization succeeds; model-specific types stay internal.
 * @param[in,out] camera Board-owned camera containing the copied configuration.
 * @param[in] info Unified immutable hardware, mode and property settings.
 * @param[out] ops Static unified operations, written only on success.
 * @return ESP_OK, ESP_ERR_INVALID_ARG for malformed arguments or properties,
 *         ESP_ERR_NOT_SUPPORTED for unsupported mode or detection settings,
 *         ESP_ERR_INVALID_STATE for an occupied DVP peripheral, or ESP_ERR_NO_MEM.
 */
esp_err_t wt_bsp_camera_ov3660_init(wt_bsp_camera_t camera,
                                    const wt_bsp_camera_info_t *info,
                                    const wt_bsp_camera_ops_t **ops);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_CAMERA_ENABLED && CONFIG_WT_BSP_CAMERA_SENSOR_OV3660
#endif // __WT_BSP_CAMERA_OV3660_H__
