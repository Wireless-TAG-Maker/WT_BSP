/**
 * @file wt_bsp_camera_gc2145.h
 * @author Wireless-Tag
 * @brief GC2145 driver factory using the unified camera configuration.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __WT_BSP_CAMERA_GC2145_H__
#define __WT_BSP_CAMERA_GC2145_H__

/* ==================== [Includes] ========================================== */

#include "../../interface/wt_bsp_camera_interface.h"

#if WT_BSP_CAMERA_ENABLED && CONFIG_WT_BSP_CAMERA_SENSOR_GC2145

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */
/* ==================== [Typedefs] ========================================== */
/* ==================== [Macros] ============================================ */
/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Initialize the GC2145 driver through the unified camera interface.
 *
 * The interface selects this registered factory using sensor_name="gc2145".
 * The supported profile is 1600x1200 UYVY at 8 fps with a 20 MHz XCLK and no
 * additional properties. Hardware is prepared lazily by the returned ops.
 * @param[in,out] camera Board-owned camera containing the copied configuration.
 * @param[in] info Unified immutable hardware and mode settings.
 * @param[out] ops Static unified operations, written only on success.
 * @return ESP_OK, ESP_ERR_INVALID_ARG for malformed arguments or properties,
 *         ESP_ERR_NOT_SUPPORTED for unsupported mode or detection settings,
 *         ESP_ERR_INVALID_STATE for an occupied DVP peripheral, or ESP_ERR_NO_MEM.
 */
esp_err_t wt_bsp_camera_gc2145_init(wt_bsp_camera_t camera,
                                    const wt_bsp_camera_info_t *info,
                                    const wt_bsp_camera_ops_t **ops);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_CAMERA_ENABLED && CONFIG_WT_BSP_CAMERA_SENSOR_GC2145
#endif // __WT_BSP_CAMERA_GC2145_H__
