/**
 * @file wt_bsp_camera_port.h
 * @author Wireless-Tag
 * @brief Hardware configuration for the reusable JPEG camera feature.
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

/** @brief Unified camera operations selected by the internal interface layer. */
typedef struct wt_bsp_camera_ops_t wt_bsp_camera_ops_t;

/** @brief A named integer camera setting understood by the selected driver. */
typedef struct {
    const char *key; /*!< Static setting name, for example "saturation". */
    int32_t value;   /*!< Setting value; the selected driver validates its range. */
} wt_bsp_camera_property_t;

/** @brief Eight-bit DVP wiring and sensor control bus supplied by the board. */
typedef struct {
    uint8_t sccb_port;        /*!< I2C controller used for sensor control. */
    uint32_t sccb_freq_hz;    /*!< Sensor control bus frequency. */
    int sccb_scl_gpio;        /*!< SCCB clock GPIO. */
    int sccb_sda_gpio;        /*!< SCCB data GPIO. */
    int reset_gpio;           /*!< Sensor reset GPIO, or -1 if not controlled. */
    int pwdn_gpio;            /*!< Sensor power-down GPIO, or -1 if not controlled. */
    int data_gpio[8];         /*!< DVP D0 through D7 GPIOs. */
    int vsync_gpio;           /*!< Frame synchronization GPIO. */
    int de_gpio;              /*!< Data-enable GPIO. */
    int pclk_gpio;            /*!< Pixel clock input GPIO. */
    int xclk_gpio;            /*!< Sensor reference clock output GPIO. */
    uint32_t xclk_freq_hz;    /*!< Sensor reference clock frequency. */
} wt_bsp_camera_dvp_info_t;

/** @brief Board-selected hardware, image format and resource budget. */
typedef struct wt_bsp_camera_info_t {
    const char *sensor_name; /*!< Registered driver identifier, for example "gc2145". */
    const wt_bsp_camera_property_t *properties; /*!< Optional common setting entries, borrowed until deinit succeeds. */
    size_t property_count; /*!< Number of entries; property keys must be unique. */
    wt_bsp_camera_format_t format; /*!< Fixed JPEG output dimensions and rate. */
    wt_bsp_camera_dvp_info_t dvp;  /*!< Physical sensor wiring and clock. */
    uint8_t capture_buffer_count; /*!< Number of DVP user buffers, 1 through 3. */
    size_t jpeg_buffer_capacity;  /*!< Maximum encoded JPEG frame size in bytes. */
    uint32_t startup_frames_to_discard; /*!< Frames discarded after capture starts. */
    int dequeue_timeout_ms;       /*!< Positive V4L2 dequeue timeout. */
    int jpeg_quality;             /*!< Hardware JPEG quality, 1 through 100. */
    int jpeg_encode_timeout_ms;   /*!< Positive hardware JPEG engine deadline. */
} wt_bsp_camera_info_t;

/** @brief Board-owned object; the feature owns its backend resources. */
typedef struct wt_bsp_camera_obj_t {
    wt_bsp_camera_info_t info; /*!< Hardware configuration copied during initialization. */
    const wt_bsp_camera_ops_t *ops; /*!< Unified operations returned by driver initialization. */
    void *backend;            /*!< Private capture, encoder, locking and frame state. */
    bool is_initialized;      /*!< Feature prepared; sensor probing remains lazy. */
} wt_bsp_camera_obj_t;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Prepare a board-owned camera object without probing the sensor.
 * @param[in,out] camera Static board object, initially zero-initialized.
 * @param[in] info Physical configuration and resource budget, copied here.
 *                sensor_name, properties and their keys are borrowed; the board
 *                must keep them unchanged until deinitialization succeeds.
 * @return ESP_OK, including repeated initialization; ESP_ERR_INVALID_ARG for
 *         missing or invalid configuration; ESP_ERR_NO_MEM on allocation failure;
 *         ESP_ERR_NOT_SUPPORTED for an unavailable sensor profile or detection mode;
 *         ESP_ERR_NOT_FOUND when no registered driver matches sensor_name;
 *         ESP_ERR_INVALID_STATE if another camera owns the DVP peripheral.
 */
esp_err_t wt_bsp_camera_init(wt_bsp_camera_t camera, const wt_bsp_camera_info_t *info);

/**
 * @brief Stop capture and release feature-owned sensor and encoder resources.
 *
 * Stop application consumers first. A cleanup error retains the initialized
 * object and any resources still borrowed by the video driver for a retry.
 * @param[in,out] camera Board-owned object, never freed by this function.
 * @return ESP_OK, including repeated calls; ESP_ERR_INVALID_ARG for NULL;
 *         or the hardware cleanup error with ownership retained.
 */
esp_err_t wt_bsp_camera_deinit(wt_bsp_camera_t camera);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_CAMERA_ENABLED
#endif // __WT_BSP_CAMERA_PORT_H__
