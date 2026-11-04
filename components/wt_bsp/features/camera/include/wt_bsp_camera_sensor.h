/**
 * @file wt_bsp_camera_sensor.h
 * @author Wireless-Tag
 * @brief Extensible sensor operations for the JPEG camera backend.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __WT_BSP_CAMERA_SENSOR_H__
#define __WT_BSP_CAMERA_SENSOR_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_camera_port.h"

#if WT_BSP_CAMERA_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */
/* ==================== [Typedefs] ========================================== */

/**
 * @brief Sensor metadata and lifecycle operations supplied by a sensor module.
 *
 * A registered driver initializer supplies this private profile to the common
 * DVP backend. Board configuration contains only unified info values.
 * prepare/configure/release run under the backend's capture mutex.
 * The caller serializes lifecycle operations; callbacks must not reenter them.
 * The implementation owns the type of its private state and settings; common
 * camera code never interprets either object.
 */
typedef struct wt_bsp_camera_sensor_ops_t {
    const char *name; /*!< Static sensor name for diagnostics. */
    uint16_t pid; /*!< Expected sensor chip identifier. */
    uint32_t pixel_format; /*!< V4L2 capture pixel format for this profile. */
    bool reuse_buffers; /*!< Retain the fixed-format buffer pool across stream restarts. */
    /** @brief Apply a sensor mode after opening capture, before allocating buffers.
     * Publish any allocated state before submitting borrowed pointers to the
     * driver, including on failure. State survives until release(). Optional.
     * @param[in] capture_fd Open V4L2 capture descriptor.
     * @param[in] info Borrowed board configuration and sensor settings.
     * @param[in,out] state Initially NULL; receives module-owned persistent state.
     * @return ESP_OK or the sensor preparation error. */
    esp_err_t (*prepare)(int capture_fd, const wt_bsp_camera_info_t *info, void **state);
    /** @brief Apply optional image tuning after V4L2 capture format selection.
     * @param[in] capture_fd Open V4L2 capture descriptor.
     * @param[in] info Borrowed board configuration and sensor settings.
     * @param[in,out] state Private state from prepare(), or NULL.
     * @return ESP_OK or a nonfatal tuning error; the backend logs and continues.
     * Optional; it must preserve the format and any prepared state. */
    esp_err_t (*configure)(int capture_fd, const wt_bsp_camera_info_t *info, void *state);
    /** @brief Free private state only after video/sensor destruction succeeds.
     * Must accept NULL and must not fail. Required when prepare() allocates
     * state. The backend clears its state pointer after this call.
     * @param[in] state Module-owned state to destroy, or NULL. */
    void (*release)(void *state);
} wt_bsp_camera_sensor_ops_t;

/* ==================== [Macros] ============================================ */
/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Check whether a sensor owns the current DVP camera's detection path.
 *
 * Sensor modules may register their own ESP_CAM_SENSOR_DETECT_FN in their .c
 * file. Its callback uses this function to avoid detecting an unselected sensor
 * sharing the same SCCB address. The caller serializes camera lifecycle.
 * @param[in] sensor Static operation table belonging to the registering module.
 * @return true when the initialized DVP owner selected these operations.
 */
bool wt_bsp_camera_sensor_is_selected(const wt_bsp_camera_sensor_ops_t *sensor);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_CAMERA_ENABLED
#endif // __WT_BSP_CAMERA_SENSOR_H__
