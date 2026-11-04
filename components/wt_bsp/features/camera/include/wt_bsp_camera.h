/**
 * @file wt_bsp_camera.h
 * @author Wireless-Tag
 * @brief Independent board-owned JPEG camera capture.
 * @version 0.1
 * @date 2026-09-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __WT_BSP_CAMERA_H__
#define __WT_BSP_CAMERA_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"

/** @brief Board-owned camera handle, valid until wt_bsp_deinit(); never free it. */
typedef struct wt_bsp_camera_obj_t *wt_bsp_camera_t;

#if WT_BSP_CAMERA_ENABLED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/** @brief A borrowed JPEG frame; data is valid only inside the capture callback. */
typedef struct {
    const uint8_t *data; /*!< Complete JPEG bytes, owned by the BSP camera feature. */
    size_t length;      /*!< JPEG length in bytes. */
    uint32_t width;     /*!< Image width in pixels. */
    uint32_t height;    /*!< Image height in pixels. */
    int64_t timestamp_us; /*!< Monotonic capture timestamp in microseconds. */
} wt_bsp_camera_frame_t;

/** @brief Fixed format selected by the board's sensor configuration. */
typedef struct {
    uint32_t width;  /*!< JPEG width in pixels. */
    uint32_t height; /*!< JPEG height in pixels. */
    uint32_t fps;    /*!< Nominal frames per second. */
} wt_bsp_camera_format_t;

/** @brief Sensor and JPEG capture health. */
typedef struct {
    bool ready;             /*!< Sensor detected and video devices prepared. */
    esp_err_t last_error;    /*!< Latest preparation/capture error; ESP_OK when healthy. */
    uint64_t frames;         /*!< Successfully captured frames since initialization. */
    int64_t last_frame_us;   /*!< Last successful capture time; zero before the first frame. */
} wt_bsp_camera_status_t;

/**
 * @brief Consume one JPEG synchronously in the calling application task.
 * @param[in] frame Borrowed frame, invalid after the callback returns.
 * @param[in] user_data Context supplied to wt_bsp_camera_capture().
 * @return ESP_OK on success or an application error propagated to the caller.
 * @note Do not reenter camera or BSP lifecycle functions from this callback.
 *       Keep work bounded: camera lifecycle operations wait for frame release.
 */
typedef esp_err_t (*wt_bsp_camera_frame_cb_t)(const wt_bsp_camera_frame_t *frame,
                                           void *user_data);

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Detect and prepare the selected camera without starting a recording.
 * @param[in] camera Board-owned handle, returned by wt_bsp_get_camera().
 * @param[out] format Selected JPEG format; written only on success.
 * @return ESP_OK on success or repeated preparation, ESP_ERR_INVALID_ARG for
 *         NULL arguments, ESP_ERR_INVALID_STATE before initialization, or a
 *         hardware error. Failure keeps other BSP resources available.
 */
esp_err_t wt_bsp_camera_prepare(wt_bsp_camera_t camera, wt_bsp_camera_format_t *format);

/**
 * @brief Capture and synchronously consume one JPEG, automatically starting capture.
 * @param[in] camera Board-owned handle. Applications coordinate recording,
 *                   streaming and other consumers; this API has no USB policy.
 * @param[in] callback Required frame consumer; no frame ownership is transferred.
 * @param[in] user_data Optional consumer context.
 * @return ESP_OK, ESP_ERR_INVALID_ARG for NULL handle/callback,
 *         ESP_ERR_INVALID_STATE before initialization,
 *         a hardware error, or the consumer's error code.
 * @note Capture uses the board's bounded dequeue timeout. Do not call from an ISR
 *       or concurrently with wt_bsp_deinit(). Serialize init/deinit with all
 *       camera operations and stop application consumers before teardown.
 */
esp_err_t wt_bsp_camera_capture(wt_bsp_camera_t camera, wt_bsp_camera_frame_cb_t callback,
                              void *user_data);

/**
 * @brief Read sensor and JPEG capture health.
 * @param[in] camera Board-owned camera handle.
 * @param[out] status Snapshot of current camera health.
 * @return ESP_OK, ESP_ERR_INVALID_ARG for NULL arguments, or
 *         ESP_ERR_INVALID_STATE before initialization.
 */
esp_err_t wt_bsp_camera_get_status(wt_bsp_camera_t camera, wt_bsp_camera_status_t *status);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_CAMERA_ENABLED
#endif // __WT_BSP_CAMERA_H__
