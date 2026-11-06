/**
 * @file wt_bsp_camera_dvp.h
 * @author Wireless-Tag
 * @brief Internal DVP and JPEG capture backend lifecycle.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __WT_BSP_CAMERA_DVP_H__
#define __WT_BSP_CAMERA_DVP_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_camera_port.h"
#include "wt_bsp_camera_sensor.h"
#include "../interface/wt_bsp_camera_interface.h"

#if WT_BSP_CAMERA_ENABLED && CONFIG_SOC_LCDCAM_CAM_SUPPORTED && CONFIG_SOC_JPEG_ENCODE_SUPPORTED

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */
/* ==================== [Typedefs] ========================================== */
/* ==================== [Macros] ============================================ */
/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Allocate backend state without probing the configured sensor.
 * @param[in,out] camera Board-owned camera containing the copied configuration.
 * @param[in] sensor Private static sensor profile, validated by its driver initializer.
 * @param[out] ops Unified static camera operations, written only on success.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_NOT_SUPPORTED, ESP_ERR_NO_MEM,
 *         or ESP_ERR_INVALID_STATE when another object owns the DVP peripheral.
 */
esp_err_t wt_bsp_camera_dvp_init(wt_bsp_camera_t camera,
                               const wt_bsp_camera_sensor_ops_t *sensor,
                               const wt_bsp_camera_ops_t **ops);

/**
 * @brief Stop capture and release backend state after application consumers stop.
 * @param[in,out] camera Board-owned object, never freed by this function.
 * @return ESP_OK on success or an error with backend ownership preserved for retry.
 */
esp_err_t wt_bsp_camera_dvp_deinit(wt_bsp_camera_t camera);

/**
 * @brief Prepare the sensor and devices lazily.
 * @param[in,out] camera Initialized camera object.
 * @param[out] format Selected output format, written on success.
 * @return ESP_OK or the sensor/device preparation error.
 */
esp_err_t wt_bsp_camera_dvp_prepare(wt_bsp_camera_t camera, wt_bsp_camera_format_t *format);

/**
 * @brief Capture one JPEG and synchronously lend it to the consumer.
 * @param[in,out] camera Initialized camera object.
 * @param[in] callback Borrowed-frame consumer; camera reentry is not permitted.
 * @param[in] user_data Optional consumer context.
 * @return ESP_OK, a capture error, or the error returned by the consumer.
 */
esp_err_t wt_bsp_camera_dvp_capture(wt_bsp_camera_t camera,
                                  wt_bsp_camera_frame_cb_t callback, void *user_data);

/**
 * @brief Read backend health under the capture mutex.
 * @param[in] camera Initialized camera object.
 * @param[out] status Current preparation and capture status.
 * @return ESP_OK or ESP_ERR_INVALID_STATE for an unavailable backend.
 */
esp_err_t wt_bsp_camera_dvp_get_status(wt_bsp_camera_t camera, wt_bsp_camera_status_t *status);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_CAMERA_ENABLED && CONFIG_SOC_LCDCAM_CAM_SUPPORTED && CONFIG_SOC_JPEG_ENCODE_SUPPORTED
#endif // __WT_BSP_CAMERA_DVP_H__
