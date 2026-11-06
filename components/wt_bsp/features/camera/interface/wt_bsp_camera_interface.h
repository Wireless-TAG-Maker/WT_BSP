/**
 * @file wt_bsp_camera_interface.h
 * @author Wireless-Tag
 * @brief Camera driver routing and unified operation interface.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __WT_BSP_CAMERA_INTERFACE_H__
#define __WT_BSP_CAMERA_INTERFACE_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_camera_port.h"

#if WT_BSP_CAMERA_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */
/* ==================== [Typedefs] ========================================== */

/** @brief Unified camera operations returned by every driver initializer. */
struct wt_bsp_camera_ops_t {
    /** @brief Release the camera; errors preserve ownership for a retry.
     * @param[in,out] camera Initialized board-owned object.
     * @return ESP_OK or the cleanup error, with resources retained on failure. */
    esp_err_t (*deinit)(wt_bsp_camera_t camera);
    /** @brief Prepare the sensor and write its selected format on success.
     * @param[in,out] camera Initialized board-owned object.
     * @param[out] format Output mode, written only on success.
     * @return ESP_OK or a driver preparation error. */
    esp_err_t (*prepare)(wt_bsp_camera_t camera, wt_bsp_camera_format_t *format);
    /** @brief Capture a JPEG and synchronously lend it to the consumer.
     * @param[in,out] camera Initialized board-owned object.
     * @param[in] callback Required bounded consumer; frame data is borrowed.
     * @param[in] user_data Optional consumer context.
     * @return ESP_OK, a capture error or the consumer's error. */
    esp_err_t (*capture)(wt_bsp_camera_t camera, wt_bsp_camera_frame_cb_t callback, void *user_data);
    /** @brief Return a snapshot of sensor and capture health.
     * @param[in] camera Initialized board-owned object.
     * @param[out] status Output health snapshot.
     * @return ESP_OK or a driver state error. */
    esp_err_t (*get_status)(wt_bsp_camera_t camera, wt_bsp_camera_status_t *status);
};

/** @brief Driver registration owned by a sensor implementation. */
typedef struct {
    const char *name; /*!< Unique identifier matched against info.sensor_name. */
    /** @brief Validate common info and prepare a camera without probing hardware.
     * @param[in,out] camera Board-owned object containing the copied info.
     * @param[in] info Unified immutable hardware, mode and property settings.
     * @param[out] ops Static unified operations, written only on success.
     * @return ESP_OK or an initialization error. On failure the initializer
     *         must release any resources it allocated. */
    esp_err_t (*init)(wt_bsp_camera_t camera, const wt_bsp_camera_info_t *info,
                      const wt_bsp_camera_ops_t **ops);
} wt_bsp_camera_driver_t;

/* ==================== [Macros] ============================================ */

/**
 * @brief Register a driver within its own .c file, without a central model list.
 * @param id Unique C identifier for this registration.
 * @param name_ Static string identifier used by board configuration.
 * @param init_ Function accepting unified info and returning unified operations.
 */
#define WT_BSP_CAMERA_DRIVER_REGISTER(id, name_, init_) \
    const wt_bsp_camera_driver_t wt_bsp_camera_driver_##id \
        __attribute__((used, aligned(__alignof__(wt_bsp_camera_driver_t)), \
                       section(".wt_bsp_camera_drivers." #id))) = { \
            .name = (name_), \
            .init = (init_), \
        }

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Route the copied configuration to the uniquely registered driver.
 * @param[in,out] camera Board-owned object with camera->info already copied.
 * @param[out] ops Driver-provided operation table, written only on success.
 * @return ESP_OK, ESP_ERR_INVALID_ARG for malformed configuration,
 *         ESP_ERR_NOT_FOUND for an unknown driver, ESP_ERR_INVALID_STATE for
 *         duplicate registrations, or the selected initializer's error.
 */
esp_err_t wt_bsp_camera_interface_init(wt_bsp_camera_t camera, const wt_bsp_camera_ops_t **ops);

/**
 * @brief Read a named integer property from the unified configuration.
 * @param[in] info Immutable board configuration.
 * @param[in] key Property name, matched exactly.
 * @param[out] value Property value, written only on success.
 * @return ESP_OK, ESP_ERR_NOT_FOUND for an absent key, or
 *         ESP_ERR_INVALID_ARG for missing arguments or duplicate keys.
 */
esp_err_t wt_bsp_camera_info_get_property(const wt_bsp_camera_info_t *info,
                                        const char *key, int32_t *value);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_CAMERA_ENABLED
#endif // __WT_BSP_CAMERA_INTERFACE_H__
