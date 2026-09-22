/**
 * @file wt_bsp_camera_port.h
 * @author Wireless-Tag
 * @brief Board adapter for serialized JPEG capture.
 * @version 0.1
 * @date 2026-09-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __WT_BSP_CAMERA_PORT_H__
#define __WT_BSP_CAMERA_PORT_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_camera.h"

#if WT_BSP_CAMERA_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/** @brief Board operations; the adapter owns locking, hardware and frame storage. */
typedef struct {
    esp_err_t (*prepare)(wt_bsp_camera_format_t *format); /*!< Prepare sensor resources. */
    esp_err_t (*capture)(wt_bsp_camera_frame_cb_t callback, void *user_data); /*!< Consume a JPEG. */
    esp_err_t (*get_status)(wt_bsp_camera_status_t *status); /*!< Snapshot capture health. */
} wt_bsp_camera_info_t;

/** @brief Static camera object; hardware teardown remains with the owning board. */
typedef struct wt_bsp_camera_obj_t {
    wt_bsp_camera_info_t info; /*!< Board operations copied during initialization. */
    bool is_initialized;      /*!< The adapter is available; sensor may still be absent. */
} wt_bsp_camera_obj_t;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Initialize a board-owned adapter without probing the sensor.
 * @param[in,out] camera Static board object, initially zero-initialized.
 * @param[in] info Required adapter operations, copied into the object.
 * @return ESP_OK or ESP_ERR_INVALID_ARG for a missing object/operation.
 */
esp_err_t wt_bsp_camera_init(wt_bsp_camera_t camera, const wt_bsp_camera_info_t *info);

/**
 * @brief Invalidate the adapter after the board stops all capture consumers.
 * @param[in,out] camera Board object. The board releases its hardware separately.
 * @return ESP_OK, including repeated calls, or ESP_ERR_INVALID_ARG for NULL.
 */
esp_err_t wt_bsp_camera_deinit(wt_bsp_camera_t camera);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_CAMERA_ENABLED
#endif // __WT_BSP_CAMERA_PORT_H__
