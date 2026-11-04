/**
 * @file board_camera.c
 * @author Wireless-Tag
 * @brief WT9932S31-TINY independent sensor and JPEG capture adapter.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/*
 * SPDX-FileCopyrightText: 2026 Wireless-Tag
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* ==================== [Includes] ========================================== */

#include "board.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "sdkconfig.h"

#include "esp_err.h"
#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "linux/videodev2.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
#include "driver/jpeg_encode.h"
#include "gc2145.h"
#include "board_gc2145.h"
#elif CONFIG_WT_BSP_S31_CAMERA_SENSOR_OV3660
#include "ov3660.h"
#endif

/* ==================== [Defines] =========================================== */

#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
/* DVP keeps a backup buffer while this frame is borrowed. One user buffer
 * leaves room for application JPEG snapshots within 16 MiB PSRAM. */
#define BOARD_CAPTURE_BUFFER_COUNT            1
#define BOARD_CAMERA_SENSOR_NAME             "GC2145"
#define BOARD_CAMERA_SENSOR_PID              0x2145
#define BOARD_CAMERA_SCCB_ADDR               GC2145_SCCB_ADDR
#define BOARD_CAMERA_DETECT                  gc2145_detect
#define BOARD_CAMERA_EXPECTED_CAPTURE_FORMAT V4L2_PIX_FMT_UYVY
#define BOARD_CAMERA_DATA_PATH               "UYVY -> hardware JPEG"
#define BOARD_GC2145_STARTUP_DISCARD_COUNT    2
#elif CONFIG_WT_BSP_S31_CAMERA_SENSOR_OV3660
#define BOARD_CAPTURE_BUFFER_COUNT            2
#define BOARD_CAMERA_SENSOR_NAME             "OV3660"
#define BOARD_CAMERA_SENSOR_PID              0x3660
#define BOARD_CAMERA_SCCB_ADDR               OV3660_SCCB_ADDR
#define BOARD_CAMERA_DETECT                  ov3660_detect
#define BOARD_CAMERA_EXPECTED_CAPTURE_FORMAT V4L2_PIX_FMT_JPEG
#define BOARD_CAMERA_DATA_PATH               "sensor-native JPEG"
#define BOARD_OV3660_SATURATION_REG_BASE     0x5381
#define BOARD_OV3660_DETAIL_CTRL_REG          0x5308
#else
#error "Select a supported camera sensor in menuconfig"
#endif

#if CONFIG_CAMERA_GC2145_AUTO_DETECT_DVP_INTERFACE_SENSOR || \
    CONFIG_CAMERA_OV3660_AUTO_DETECT_DVP_INTERFACE_SENSOR
#error "Component sensor auto-detect must stay disabled; the project registers the menuconfig-selected sensor"
#endif

/* ==================== [Typedefs] ========================================== */

typedef struct {
    uint8_t *buf;
    size_t len;
    uint32_t width;
    uint32_t height;
    struct timeval timestamp;
} board_camera_buffer_t;

typedef struct {
    int capture_fd;
    int codec_fd;
    uint8_t *capture_buffer[BOARD_CAPTURE_BUFFER_COUNT];
    uint8_t *codec_capture_buffer;
    size_t jpeg_buffer_capacity;
    board_camera_buffer_t frame;
    uint32_t capture_buffer_count;
    uint32_t frame_width;
    uint32_t frame_height;
    bool capture_streaming;
    bool codec_output_streaming;
    bool codec_capture_streaming;
    bool codec_frame_outstanding;
    bool direct_jpeg;
    bool capture_frame_outstanding;
    uint32_t capture_frame_index;
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
    bool buffers_ready;
    uint32_t startup_frames_to_discard;
#endif
    SemaphoreHandle_t stream_mutex;
    bool frame_lock_held;
#if WT_BSP_CAMERA_ENABLED
    wt_bsp_camera_status_t health;
#endif
} board_camera_context_t;

/* ==================== [Static Prototypes] ================================= */

static esp_err_t board_video_init(void);
static esp_err_t board_video_deinit(void);
static esp_err_t board_prepare_camera(void);
static esp_err_t board_open_video_devices(board_camera_context_t *ctx);
static esp_err_t board_set_dqbuf_timeout(int fd, const char *name);
static esp_err_t board_get_sensor_id(int fd, esp_cam_sensor_id_t *sensor_id);
static esp_err_t board_verify_configured_sensor(int fd);
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_OV3660
static esp_err_t board_set_sensor_ae_level(int fd);
static esp_err_t board_set_sensor_jpeg_quality(int fd);
static esp_err_t board_sensor_register_access(int fd, uint32_t command, esp_cam_sensor_reg_val_t *reg_value);
static esp_err_t board_set_ov3660_saturation(int fd);
static esp_err_t board_set_ov3660_detail_tuning(int fd);
#endif
static esp_err_t board_camera_start_cb(int width, int height, int rate, void *cb_ctx);
static void board_camera_stop_cb(void *cb_ctx);
static board_camera_buffer_t *board_camera_frame_get_cb(void *cb_ctx);
static void board_camera_frame_return_cb(board_camera_buffer_t *frame, void *cb_ctx);
static esp_err_t board_camera_start_impl(int width, int height, int rate, void *cb_ctx);
static void board_camera_stop_impl(void *cb_ctx);
static board_camera_buffer_t *board_camera_frame_get_impl(void *cb_ctx);
static void board_camera_frame_return_impl(board_camera_buffer_t *frame, void *cb_ctx);
static void board_close_video_devices(board_camera_context_t *ctx);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "board_camera";

ESP_CAM_SENSOR_DETECT_FN(board_selected_sensor_detect,
                         ESP_CAM_SENSOR_DVP,
                         BOARD_CAMERA_SCCB_ADDR)
{
    esp_cam_sensor_config_t *sensor_config = config;

    sensor_config->sensor_port = ESP_CAM_SENSOR_DVP;
    return BOARD_CAMERA_DETECT(sensor_config);
}

#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_OV3660
/*
 * OV3660 saturation matrices from esp_cam_sensor 2.3.0 for levels -2..+2.
 * The VGA format table starts at -2; level 0 restores the sensor's neutral
 * color matrix without patching the managed component.
 */
static const uint8_t s_ov3660_saturation_levels[5][11] = {
    {0x1d, 0x60, 0x03, 0x0a, 0x60, 0x6a, 0x64, 0x56, 0x0e, 0x01, 0x98},
    {0x1d, 0x60, 0x03, 0x0b, 0x6c, 0x77, 0x70, 0x60, 0x10, 0x01, 0x98},
    {0x1d, 0x60, 0x03, 0x0c, 0x78, 0x84, 0x7d, 0x6b, 0x12, 0x01, 0x98},
    {0x1d, 0x60, 0x03, 0x0d, 0x84, 0x91, 0x8a, 0x76, 0x14, 0x01, 0x98},
    {0x1d, 0x60, 0x03, 0x0e, 0x90, 0x9e, 0x96, 0x80, 0x16, 0x01, 0x98},
};
#endif

static board_camera_context_t s_camera = {
    .capture_fd = -1,
    .codec_fd = -1,
};

#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
/* esp_video borrows this handle; keep it alive until its video device is gone. */
static jpeg_encoder_handle_t s_jpeg_encoder;
#endif
static StaticSemaphore_t s_stream_mutex_storage;
static bool s_camera_is_initialized;
static bool s_video_initialized;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t board_camera_init(void)
{
    if (s_camera_is_initialized) {
        ESP_LOGW(TAG, "Camera adapter is already initialized");
        return ESP_OK;
    }
    if (s_camera.stream_mutex == NULL) {
        s_camera.stream_mutex = xSemaphoreCreateRecursiveMutexStatic(&s_stream_mutex_storage);
        if (s_camera.stream_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create camera stream mutex");
            return ESP_ERR_NO_MEM;
        }
    }
    s_camera.jpeg_buffer_capacity =
        (size_t)BOARD_CAMERA_FRAME_WIDTH * BOARD_CAMERA_FRAME_HEIGHT * 2;
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
    // Match the fixed-format JPEG output pool to fit in 16 MB PSRAM.
    s_camera.jpeg_buffer_capacity /= 2;
#endif
    s_camera.health = (wt_bsp_camera_status_t) {
        .last_error = ESP_ERR_INVALID_STATE,
    };
    s_camera_is_initialized = true;
    return ESP_OK;
}

esp_err_t board_camera_deinit(void)
{
    esp_err_t ret = ESP_OK;

    board_close_video_devices(&s_camera);
    ret = board_video_deinit();
    s_camera.jpeg_buffer_capacity = 0;
    if (s_camera.stream_mutex != NULL) {
        vSemaphoreDelete(s_camera.stream_mutex);
        s_camera.stream_mutex = NULL;
    }
    s_camera_is_initialized = false;
    return ret;
}

esp_err_t board_camera_prepare(wt_bsp_camera_format_t *format)
{
    if (format == NULL) {
        ESP_LOGE(TAG, "Camera format output is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_camera_is_initialized || s_camera.stream_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTakeRecursive(s_camera.stream_mutex, portMAX_DELAY);
    esp_err_t ret = board_prepare_camera();
    if (ret == ESP_OK) {
        *format = (wt_bsp_camera_format_t) {
            .width = BOARD_CAMERA_FRAME_WIDTH,
            .height = BOARD_CAMERA_FRAME_HEIGHT,
            .fps = BOARD_CAMERA_FRAME_RATE,
        };
    }
    xSemaphoreGiveRecursive(s_camera.stream_mutex);
    return ret;
}

esp_err_t board_camera_capture(wt_bsp_camera_frame_cb_t callback, void *user_data)
{
    if (callback == NULL) {
        ESP_LOGE(TAG, "Camera consumer callback is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_camera_is_initialized || s_camera.stream_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTakeRecursive(s_camera.stream_mutex, portMAX_DELAY);
    esp_err_t ret = board_prepare_camera();
    if (ret != ESP_OK) {
        goto done;
    }
    if (!s_camera.capture_streaming) {
        ret = board_camera_start_cb(BOARD_CAMERA_FRAME_WIDTH, BOARD_CAMERA_FRAME_HEIGHT,
                                    BOARD_CAMERA_FRAME_RATE, &s_camera);
        if (ret != ESP_OK) {
            goto done;
        }
    }
    board_camera_buffer_t *frame = board_camera_frame_get_cb(&s_camera);
    if (frame == NULL) {
        ret = s_camera.health.last_error;
        if (ret == ESP_OK) {
            ret = ESP_FAIL;
        }
        goto done;
    }
    const wt_bsp_camera_frame_t jpeg = {
        .data = frame->buf,
        .length = frame->len,
        .width = frame->width,
        .height = frame->height,
        .timestamp_us = (int64_t)frame->timestamp.tv_sec * 1000000 + frame->timestamp.tv_usec,
    };
    ret = callback(&jpeg, user_data);
    board_camera_frame_return_cb(frame, &s_camera);
done:
    xSemaphoreGiveRecursive(s_camera.stream_mutex);
    return ret;
}

esp_err_t board_camera_get_status(wt_bsp_camera_status_t *status)
{
    if (status == NULL) {
        ESP_LOGE(TAG, "Camera status output is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_camera_is_initialized || s_camera.stream_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTakeRecursive(s_camera.stream_mutex, portMAX_DELAY);
    *status = s_camera.health;
    xSemaphoreGiveRecursive(s_camera.stream_mutex);
    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static esp_err_t board_prepare_camera(void)
{
    if (s_video_initialized && s_camera.capture_fd >= 0 && s_camera.codec_fd >= 0) {
        return ESP_OK;
    }
    esp_err_t ret = board_video_init();
    if (ret == ESP_OK) {
        ret = board_open_video_devices(&s_camera);
    }
    if (ret != ESP_OK) {
        board_close_video_devices(&s_camera);
        board_video_deinit();
    }
#if WT_BSP_CAMERA_ENABLED
    s_camera.health.ready = ret == ESP_OK;
    s_camera.health.last_error = ret;
#endif
    return ret;
}

static esp_err_t board_video_init(void)
{
    esp_err_t ret;
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
    const jpeg_encode_engine_cfg_t engine_config = {
        .intr_priority = 0,
        .timeout_ms = CONFIG_WT_BSP_S31_JPEG_ENCODE_TIMEOUT_MS,
    };
    ret = jpeg_new_encoder_engine(&engine_config, &s_jpeg_encoder);
    if (ret != ESP_OK) {
        return ret;
    }
    const esp_video_init_jpeg_enc_config_t jpeg_config = {
        .enc_handle = s_jpeg_encoder,
    };
    ESP_LOGI(TAG, "Hardware JPEG encode timeout=%d ms", engine_config.timeout_ms);
#endif
    const esp_video_init_dvp_config_t dvp_config = {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port = BOARD_DVP_SCCB_PORT,
                .scl_pin = BOARD_DVP_SCCB_SCL_GPIO,
                .sda_pin = BOARD_DVP_SCCB_SDA_GPIO,
            },
            .freq = BOARD_DVP_SCCB_FREQ_HZ,
        },
        .reset_pin = BOARD_DVP_RESET_GPIO,
        .pwdn_pin = BOARD_DVP_PWDN_GPIO,
        .dvp_pin = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {
                BOARD_DVP_D0_GPIO,
                BOARD_DVP_D1_GPIO,
                BOARD_DVP_D2_GPIO,
                BOARD_DVP_D3_GPIO,
                BOARD_DVP_D4_GPIO,
                BOARD_DVP_D5_GPIO,
                BOARD_DVP_D6_GPIO,
                BOARD_DVP_D7_GPIO,
            },
            .vsync_io = BOARD_DVP_VSYNC_GPIO,
            .de_io = BOARD_DVP_DE_GPIO,
            .pclk_io = BOARD_DVP_PCLK_GPIO,
            .xclk_io = BOARD_DVP_XCLK_GPIO,
        },
        .xclk_freq = BOARD_DVP_XCLK_FREQ_HZ,
    };
    const esp_video_init_config_t video_config = {
        .dvp = &dvp_config,
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
        .jpeg_enc = &jpeg_config,
#endif
    };

    s_video_initialized = true;
    ret = esp_video_init_with_flags(&video_config,
                                    ESP_VIDEO_INIT_FLAGS_DVP |
                                    ESP_VIDEO_INIT_FLAGS_JPEG_ENC);
    if (ret != ESP_OK) {
        board_video_deinit();
    }
    return ret;
}

static esp_err_t board_video_deinit(void)
{
    esp_err_t ret = ESP_OK;

    if (s_video_initialized) {
        ret = esp_video_deinit_with_flags(ESP_VIDEO_INIT_FLAGS_DVP | ESP_VIDEO_INIT_FLAGS_JPEG_ENC);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Video deinitialization failed: %s", esp_err_to_name(ret));
            return ret;
        }
        s_video_initialized = false;
    }
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
    // The sensor and video device have released their borrowed pointers.
    board_gc2145_release_format();
    if (s_jpeg_encoder != NULL) {
        ret = jpeg_del_encoder_engine(s_jpeg_encoder);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "JPEG engine deletion failed: %s", esp_err_to_name(ret));
            return ret;
        }
        s_jpeg_encoder = NULL;
    }
#endif
    return ESP_OK;
}

static esp_err_t board_open_video_devices(board_camera_context_t *ctx)
{
    struct v4l2_ext_control control = {
        .id = V4L2_CID_JPEG_COMPRESSION_QUALITY,
        .value = CONFIG_WT_BSP_S31_JPEG_QUALITY,
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CID_JPEG_CLASS,
        .count = 1,
        .controls = &control,
    };
    esp_err_t ret;

    ctx->capture_fd = open(ESP_VIDEO_DVP_DEVICE_NAME, O_RDONLY);
    if (ctx->capture_fd < 0) {
        ESP_LOGE(TAG,
                 "Cannot open DVP device %s (no supported camera detected, errno=%d)",
                 ESP_VIDEO_DVP_DEVICE_NAME,
                 errno);
        return ESP_ERR_NOT_FOUND;
    }

    ret = board_verify_configured_sensor(ctx->capture_fd);
    if (ret != ESP_OK) {
        return ret;
    }

#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
    ret = board_gc2145_configure_uxga(ctx->capture_fd);
    if (ret != ESP_OK) {
        return ret;
    }
#endif

    ctx->codec_fd = open(ESP_VIDEO_JPEG_ENC_DEVICE_NAME, O_RDONLY);
    if (ctx->codec_fd < 0) {
        ESP_LOGE(TAG, "Cannot open JPEG device %s (errno=%d)", ESP_VIDEO_JPEG_ENC_DEVICE_NAME, errno);
        return ESP_ERR_NOT_FOUND;
    }

    ret = board_set_dqbuf_timeout(ctx->capture_fd, "DVP");
    if (ret != ESP_OK) {
        return ret;
    }
    ret = board_set_dqbuf_timeout(ctx->codec_fd, "JPEG");
    if (ret != ESP_OK) {
        return ret;
    }

    if (ioctl(ctx->codec_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "Could not set JPEG quality to %d (errno=%d)", CONFIG_WT_BSP_S31_JPEG_QUALITY, errno);
    }

    return ESP_OK;
}

static esp_err_t board_set_dqbuf_timeout(int fd, const char *name)
{
    struct timeval timeout = {
        .tv_sec = CONFIG_WT_BSP_S31_DQBUF_TIMEOUT_MS / 1000,
        .tv_usec = (CONFIG_WT_BSP_S31_DQBUF_TIMEOUT_MS % 1000) * 1000,
    };

    if (ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) != 0) {
        ESP_LOGE(TAG, "Failed to set %s dequeue timeout (errno=%d)", name, errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t board_get_sensor_id(int fd, esp_cam_sensor_id_t *sensor_id)
{
    struct v4l2_ext_control control = {
        .id = ESP_CAM_SENSOR_IOC_G_CHIP_ID,
        .p_u8 = (uint8_t *)sensor_id,
        .size = sizeof(*sensor_id),
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CTRL_CLASS_ESP_CAM_IOCTL,
        .count = 1,
        .controls = &control,
    };

    if (ioctl(fd, VIDIOC_G_EXT_CTRLS, &controls) != 0) {
        ESP_LOGE(TAG, "Could not read camera sensor ID (errno=%d)", errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t board_verify_configured_sensor(int fd)
{
    esp_cam_sensor_id_t sensor_id = {0};

    if (board_get_sensor_id(fd, &sensor_id) != ESP_OK) {
        return ESP_FAIL;
    }
    if (sensor_id.pid != BOARD_CAMERA_SENSOR_PID) {
        ESP_LOGE(TAG,
                 "Camera selection mismatch: menuconfig=%s PID=0x%04x, detected PID=0x%04x",
                 BOARD_CAMERA_SENSOR_NAME,
                 (unsigned int)BOARD_CAMERA_SENSOR_PID,
                 (unsigned int)sensor_id.pid);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG,
             "Configured camera detected: %s PID=0x%04x",
             BOARD_CAMERA_SENSOR_NAME,
             (unsigned int)sensor_id.pid);
    return ESP_OK;
}

#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_OV3660
static esp_err_t board_set_sensor_ae_level(int fd)
{
    struct v4l2_query_ext_ctrl query = {
        .id = V4L2_CID_CAMERA_AE_LEVEL,
    };
    struct v4l2_ext_control control = {
        .id = V4L2_CID_CAMERA_AE_LEVEL,
        .value = CONFIG_WT_BSP_S31_OV3660_AE_LEVEL,
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CID_CAMERA_CLASS,
        .count = 1,
        .controls = &control,
    };

    if (ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &query) != 0) {
        ESP_LOGW(TAG, "Sensor does not expose an AE target-level control (errno=%d)", errno);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (CONFIG_WT_BSP_S31_OV3660_AE_LEVEL < query.minimum ||
            CONFIG_WT_BSP_S31_OV3660_AE_LEVEL > query.maximum) {
        ESP_LOGE(TAG,
                 "Configured AE target level %d is outside sensor range %d..%d",
                 CONFIG_WT_BSP_S31_OV3660_AE_LEVEL,
                 (int)query.minimum,
                 (int)query.maximum);
        return ESP_ERR_INVALID_ARG;
    }
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG,
                 "Could not set sensor AE target level to %d (errno=%d)",
                 CONFIG_WT_BSP_S31_OV3660_AE_LEVEL,
                 errno);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "OV3660 auto-exposure target level=%d (sensor range %d..%d)",
             CONFIG_WT_BSP_S31_OV3660_AE_LEVEL,
             (int)query.minimum,
             (int)query.maximum);
    return ESP_OK;
}

static esp_err_t board_set_sensor_jpeg_quality(int fd)
{
    struct v4l2_query_ext_ctrl query = {
        .id = V4L2_CID_JPEG_COMPRESSION_QUALITY,
    };
    struct v4l2_ext_control control = {
        .id = V4L2_CID_JPEG_COMPRESSION_QUALITY,
        .value = CONFIG_WT_BSP_S31_OV3660_JPEG_QUALITY,
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CID_JPEG_CLASS,
        .count = 1,
        .controls = &control,
    };

    if (ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &query) != 0) {
        ESP_LOGW(TAG, "Sensor does not expose JPEG quality control (errno=%d)", errno);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (CONFIG_WT_BSP_S31_OV3660_JPEG_QUALITY < query.minimum ||
            CONFIG_WT_BSP_S31_OV3660_JPEG_QUALITY > query.maximum) {
        ESP_LOGE(TAG,
                 "Configured sensor JPEG quality %d is outside range %d..%d",
                 CONFIG_WT_BSP_S31_OV3660_JPEG_QUALITY,
                 (int)query.minimum,
                 (int)query.maximum);
        return ESP_ERR_INVALID_ARG;
    }
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG,
                 "Could not set OV3660 JPEG quality to %d (errno=%d)",
                 CONFIG_WT_BSP_S31_OV3660_JPEG_QUALITY,
                 errno);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "OV3660 native JPEG quality=%d (sensor range %d..%d)",
             CONFIG_WT_BSP_S31_OV3660_JPEG_QUALITY,
             (int)query.minimum,
             (int)query.maximum);
    return ESP_OK;
}

static esp_err_t board_sensor_register_access(int fd,
                                                uint32_t command,
                                                esp_cam_sensor_reg_val_t *reg_value)
{
    struct v4l2_ext_control control = {
        .id = command,
        .p_u8 = (uint8_t *)reg_value,
        .size = sizeof(*reg_value),
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CTRL_CLASS_ESP_CAM_IOCTL,
        .count = 1,
        .controls = &control,
    };
    int request;

    if (command == ESP_CAM_SENSOR_IOC_G_REG) {
        request = VIDIOC_G_EXT_CTRLS;
    } else if (command == ESP_CAM_SENSOR_IOC_S_REG) {
        request = VIDIOC_S_EXT_CTRLS;
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    return ioctl(fd, request, &controls) == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t board_set_ov3660_saturation(int fd)
{
    esp_cam_sensor_id_t sensor_id = {0};

    if (board_get_sensor_id(fd, &sensor_id) != ESP_OK) {
        ESP_LOGW(TAG, "Could not read sensor ID before color tuning (errno=%d)", errno);
        return ESP_FAIL;
    }
    if (sensor_id.pid != BOARD_CAMERA_SENSOR_PID) {
        ESP_LOGW(TAG,
                 "Skipping OV3660 color tuning for sensor PID=0x%x",
                 (unsigned int)sensor_id.pid);
        return ESP_ERR_NOT_SUPPORTED;
    }

    const uint8_t *values =
        s_ov3660_saturation_levels[CONFIG_WT_BSP_S31_OV3660_SATURATION + 2];
    for (size_t i = 0; i < sizeof(s_ov3660_saturation_levels[0]); i++) {
        esp_cam_sensor_reg_val_t reg_value = {
            .regaddr = BOARD_OV3660_SATURATION_REG_BASE + i,
            .value = values[i],
        };
        if (board_sensor_register_access(fd, ESP_CAM_SENSOR_IOC_S_REG, &reg_value) != ESP_OK) {
            ESP_LOGW(TAG,
                     "Could not set OV3660 saturation register 0x%x (errno=%d)",
                     (unsigned int)reg_value.regaddr,
                     errno);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "OV3660 saturation level=%d", CONFIG_WT_BSP_S31_OV3660_SATURATION);
    return ESP_OK;
}

static esp_err_t board_set_ov3660_detail_tuning(int fd)
{
    esp_cam_sensor_id_t sensor_id = {0};
    esp_cam_sensor_reg_val_t detail_ctrl = {
        .regaddr = BOARD_OV3660_DETAIL_CTRL_REG,
    };

    if (board_get_sensor_id(fd, &sensor_id) != ESP_OK) {
        ESP_LOGW(TAG, "Could not read sensor ID before detail tuning (errno=%d)", errno);
        return ESP_FAIL;
    }
    if (sensor_id.pid != BOARD_CAMERA_SENSOR_PID) {
        ESP_LOGW(TAG,
                 "Skipping OV3660 detail tuning for sensor PID=0x%x",
                 (unsigned int)sensor_id.pid);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (board_sensor_register_access(fd, ESP_CAM_SENSOR_IOC_G_REG, &detail_ctrl) != ESP_OK) {
        ESP_LOGW(TAG, "Could not read OV3660 detail-control register (errno=%d)", errno);
        return ESP_FAIL;
    }

    const uint8_t original_detail_ctrl = detail_ctrl.value;
    const uint8_t sharpness_offset_2 = (CONFIG_WT_BSP_S31_OV3660_SHARPNESS + 3) * 8;
    const struct {
        uint32_t regaddr;
        uint8_t value;
    } sharpness_registers[] = {
        {0x5300, 0x10},
        {0x5301, 0x10},
        {0x5302, sharpness_offset_2 + 1},
        {0x5303, sharpness_offset_2},
        {0x5309, 0x10},
        {0x530a, 0x10},
        {0x530b, 0x04},
        {0x530c, 0x06},
    };

    /* Bit 6 selects automatic sharpness; clear it for the configured level. */
    detail_ctrl.value &= ~0x40;
    if (CONFIG_WT_BSP_S31_OV3660_DENOISE > 0) {
        detail_ctrl.value |= 0x10;
    } else {
        detail_ctrl.value &= ~0x10;
    }
    if (board_sensor_register_access(fd, ESP_CAM_SENSOR_IOC_S_REG, &detail_ctrl) != ESP_OK) {
        ESP_LOGW(TAG, "Could not set OV3660 detail-control register (errno=%d)", errno);
        return ESP_FAIL;
    }

    for (size_t i = 0; i < sizeof(sharpness_registers) / sizeof(sharpness_registers[0]); i++) {
        esp_cam_sensor_reg_val_t reg_value = {
            .regaddr = sharpness_registers[i].regaddr,
            .value = sharpness_registers[i].value,
        };
        if (board_sensor_register_access(fd, ESP_CAM_SENSOR_IOC_S_REG, &reg_value) != ESP_OK) {
            ESP_LOGW(TAG,
                     "Could not set OV3660 sharpness register 0x%x (errno=%d)",
                     (unsigned int)reg_value.regaddr,
                     errno);
            return ESP_FAIL;
        }
    }

    if (CONFIG_WT_BSP_S31_OV3660_DENOISE > 0) {
        esp_cam_sensor_reg_val_t denoise_offset = {
            .regaddr = 0x5306,
            .value = (CONFIG_WT_BSP_S31_OV3660_DENOISE - 1) * 4,
        };
        if (board_sensor_register_access(fd, ESP_CAM_SENSOR_IOC_S_REG, &denoise_offset) != ESP_OK) {
            ESP_LOGW(TAG, "Could not set OV3660 denoise level (errno=%d)", errno);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG,
             "OV3660 detail tuning: sharpness=%d, denoise=%d (control 0x%02x->0x%02x)",
             CONFIG_WT_BSP_S31_OV3660_SHARPNESS,
             CONFIG_WT_BSP_S31_OV3660_DENOISE,
             (unsigned int)original_detail_ctrl,
             (unsigned int)detail_ctrl.value);
    return ESP_OK;
}
#endif

static esp_err_t board_camera_start_cb(int width, int height, int rate, void *cb_ctx)
{
    board_camera_context_t *ctx = (board_camera_context_t *)cb_ctx;
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTakeRecursive(ctx->stream_mutex, portMAX_DELAY);
    esp_err_t ret = board_prepare_camera();
    if (ret == ESP_OK) {
        ret = board_camera_start_impl(width, height, rate, cb_ctx);
    }
#if WT_BSP_CAMERA_ENABLED
    ctx->health.last_error = ret;
#endif
    xSemaphoreGiveRecursive(ctx->stream_mutex);
    return ret;
}

static void board_camera_stop_cb(void *cb_ctx)
{
    board_camera_context_t *ctx = (board_camera_context_t *)cb_ctx;
    if (ctx == NULL) {
        return;
    }
    xSemaphoreTakeRecursive(ctx->stream_mutex, portMAX_DELAY);
    board_camera_stop_impl(cb_ctx);
    xSemaphoreGiveRecursive(ctx->stream_mutex);
}

static board_camera_buffer_t *board_camera_frame_get_cb(void *cb_ctx)
{
    board_camera_context_t *ctx = (board_camera_context_t *)cb_ctx;
    if (ctx == NULL) {
        return NULL;
    }
    xSemaphoreTakeRecursive(ctx->stream_mutex, portMAX_DELAY);
    board_camera_buffer_t *frame = board_camera_frame_get_impl(cb_ctx);
#if WT_BSP_CAMERA_ENABLED
    if (frame != NULL && (frame->len < 4 || frame->buf[0] != 0xff || frame->buf[1] != 0xd8 ||
            frame->buf[frame->len - 2] != 0xff || frame->buf[frame->len - 1] != 0xd9)) {
        board_camera_frame_return_impl(frame, cb_ctx);
        frame = NULL;
        ESP_LOGE(TAG, "Camera returned an incomplete JPEG");
    }
    ctx->health.last_error = frame != NULL ? ESP_OK : ESP_FAIL;
    if (frame != NULL) {
        ctx->health.frames++;
        ctx->health.last_frame_us = esp_timer_get_time();
    }
#endif
    if (frame != NULL) {
        /* Retain the camera lock until the synchronous consumer returns
         * the frame, so a stop cannot reset queues or overwrite its JPEG. */
        ctx->frame_lock_held = true;
    } else {
        xSemaphoreGiveRecursive(ctx->stream_mutex);
    }
    return frame;
}

static void board_camera_frame_return_cb(board_camera_buffer_t *frame, void *cb_ctx)
{
    board_camera_context_t *ctx = (board_camera_context_t *)cb_ctx;
    if (ctx == NULL) {
        return;
    }
    xSemaphoreTakeRecursive(ctx->stream_mutex, portMAX_DELAY);
    bool release_frame_lock = ctx->frame_lock_held;
    board_camera_frame_return_impl(frame, cb_ctx);
    ctx->frame_lock_held = false;
    xSemaphoreGiveRecursive(ctx->stream_mutex);
    if (release_frame_lock) {
        /* Empty-frame cleanup has no retained successful-get reference. */
        xSemaphoreGiveRecursive(ctx->stream_mutex);
    }
}

static esp_err_t board_camera_start_impl(int width, int height, int rate, void *cb_ctx)
{
    board_camera_context_t *ctx = (board_camera_context_t *)cb_ctx;
    struct v4l2_buffer buffer;
    struct v4l2_requestbuffers request;
    struct v4l2_format video_format;
    uint32_t capture_format = 0;
    bool reuse_buffers = false;
    int type;

    if (ctx == NULL) {
        ESP_LOGE(TAG, "Camera context is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (width != BOARD_CAMERA_FRAME_WIDTH ||
            height != BOARD_CAMERA_FRAME_HEIGHT ||
            rate != BOARD_CAMERA_FRAME_RATE) {
        ESP_LOGE(TAG, "Unsupported camera mode %dx%d@%dfps", width, height, rate);
        return ESP_ERR_NOT_SUPPORTED;
    }

    board_camera_stop_cb(ctx);
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
    if (ctx->capture_streaming || ctx->codec_output_streaming || ctx->codec_capture_streaming) {
        ESP_LOGE(TAG, "Cannot restart GC2145 while a video stream has not stopped");
        return ESP_ERR_INVALID_STATE;
    }
    reuse_buffers = ctx->buffers_ready;
    /* Any partial setup below must not be reused as a complete buffer pool. */
    ctx->buffers_ready = false;
    if (reuse_buffers) {
        ESP_LOGI(TAG, "GC2145 fixed-format buffers reused");
    }
#endif

    for (uint32_t index = 0; capture_format == 0; index++) {
        struct v4l2_fmtdesc format_desc = {
            .index = index,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        };
        if (ioctl(ctx->capture_fd, VIDIOC_ENUM_FMT, &format_desc) != 0) {
            break;
        }
        if (format_desc.pixelformat == BOARD_CAMERA_EXPECTED_CAPTURE_FORMAT) {
            capture_format = format_desc.pixelformat;
        }
    }

    if (capture_format == 0) {
        ESP_LOGE(TAG, "Configured %s capture format is unavailable", BOARD_CAMERA_SENSOR_NAME);
        return ESP_ERR_NOT_SUPPORTED;
    }

    memset(&video_format, 0, sizeof(video_format));
    video_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_format.fmt.pix.width = width;
    video_format.fmt.pix.height = height;
    video_format.fmt.pix.pixelformat = capture_format;
    if (ioctl(ctx->capture_fd, VIDIOC_S_FMT, &video_format) != 0) {
        ESP_LOGE(TAG, "Failed to configure DVP capture format (errno=%d)", errno);
        return ESP_FAIL;
    }
    capture_format = video_format.fmt.pix.pixelformat;
    ctx->direct_jpeg = capture_format == V4L2_PIX_FMT_JPEG;
    if (capture_format != BOARD_CAMERA_EXPECTED_CAPTURE_FORMAT ||
            video_format.fmt.pix.width != (uint32_t)width ||
            video_format.fmt.pix.height != (uint32_t)height) {
        ESP_LOGE(TAG,
                 "Configured %s returned unexpected mode %c%c%c%c %ux%u",
                 BOARD_CAMERA_SENSOR_NAME,
                 (char)(capture_format & 0xff),
                 (char)((capture_format >> 8) & 0xff),
                 (char)((capture_format >> 16) & 0xff),
                 (char)((capture_format >> 24) & 0xff),
                 (unsigned int)video_format.fmt.pix.width,
                 (unsigned int)video_format.fmt.pix.height);
        return ESP_ERR_NOT_SUPPORTED;
    }
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_OV3660
    if (board_set_sensor_ae_level(ctx->capture_fd) != ESP_OK) {
        ESP_LOGW(TAG, "Continuing with the sensor's default auto-exposure target");
    }
    if (ctx->direct_jpeg && board_set_sensor_jpeg_quality(ctx->capture_fd) != ESP_OK) {
        ESP_LOGW(TAG, "Continuing with the sensor's default native JPEG quality");
    }
    if (ctx->direct_jpeg && board_set_ov3660_saturation(ctx->capture_fd) != ESP_OK) {
        ESP_LOGW(TAG, "Continuing with the sensor's format-default color matrix");
    }
    if (ctx->direct_jpeg && board_set_ov3660_detail_tuning(ctx->capture_fd) != ESP_OK) {
        ESP_LOGW(TAG, "Continuing after OV3660 detail tuning was unavailable or incomplete");
    }
#endif
    ESP_LOGI(TAG,
             "DVP capture format: %c%c%c%c (%s)",
             (char)(capture_format & 0xff),
             (char)((capture_format >> 8) & 0xff),
             (char)((capture_format >> 16) & 0xff),
             (char)((capture_format >> 24) & 0xff),
             ctx->direct_jpeg ? "sensor native JPEG" : "hardware JPEG input");

    if (!reuse_buffers) {
        memset(&request, 0, sizeof(request));
        request.count = BOARD_CAPTURE_BUFFER_COUNT;
        request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        request.memory = V4L2_MEMORY_MMAP;
        if (ioctl(ctx->capture_fd, VIDIOC_REQBUFS, &request) != 0 || request.count == 0) {
            ESP_LOGE(TAG, "Failed to request DVP buffers (errno=%d)", errno);
            return ESP_FAIL;
        }
        ctx->capture_buffer_count =
            request.count < BOARD_CAPTURE_BUFFER_COUNT ? request.count : BOARD_CAPTURE_BUFFER_COUNT;
    }

    for (uint32_t i = 0; i < ctx->capture_buffer_count; i++) {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        if (!reuse_buffers) {
            if (ioctl(ctx->capture_fd, VIDIOC_QUERYBUF, &buffer) != 0) {
                ESP_LOGE(TAG, "Failed to query DVP buffer %u (errno=%d)", (unsigned int)i, errno);
                return ESP_FAIL;
            }

            ctx->capture_buffer[i] = mmap(NULL,
                                          buffer.length,
                                          PROT_READ | PROT_WRITE,
                                          MAP_SHARED,
                                          ctx->capture_fd,
                                          buffer.m.offset);
            if (ctx->capture_buffer[i] == MAP_FAILED) {
                ctx->capture_buffer[i] = NULL;
                ESP_LOGE(TAG, "Failed to map DVP buffer %u (errno=%d)", (unsigned int)i, errno);
                return ESP_FAIL;
            }
        }
        if (ioctl(ctx->capture_fd, VIDIOC_QBUF, &buffer) != 0) {
            ESP_LOGE(TAG, "Failed to queue DVP buffer %u (errno=%d)", (unsigned int)i, errno);
            return ESP_FAIL;
        }
    }

    if (ctx->direct_jpeg) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(ctx->capture_fd, VIDIOC_STREAMON, &type) != 0) {
            ESP_LOGE(TAG, "Failed to start native JPEG DVP stream (errno=%d)", errno);
            return ESP_FAIL;
        }
        ctx->capture_streaming = true;
        ctx->frame_width = width;
        ctx->frame_height = height;
        ESP_LOGI(TAG, "Camera capture started with sensor-native JPEG: %dx%d@%dfps", width, height, rate);
        return ESP_OK;
    }

    memset(&video_format, 0, sizeof(video_format));
    video_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    video_format.fmt.pix.width = width;
    video_format.fmt.pix.height = height;
    video_format.fmt.pix.pixelformat = capture_format;
    if (ioctl(ctx->codec_fd, VIDIOC_S_FMT, &video_format) != 0) {
        ESP_LOGE(TAG, "Failed to configure JPEG input format (errno=%d)", errno);
        return ESP_FAIL;
    }

    if (!reuse_buffers) {
        memset(&request, 0, sizeof(request));
        request.count = 1;
        request.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        request.memory = V4L2_MEMORY_USERPTR;
        if (ioctl(ctx->codec_fd, VIDIOC_REQBUFS, &request) != 0) {
            ESP_LOGE(TAG, "Failed to request JPEG input buffer (errno=%d)", errno);
            return ESP_FAIL;
        }
    }

    memset(&video_format, 0, sizeof(video_format));
    video_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_format.fmt.pix.width = width;
    video_format.fmt.pix.height = height;
    video_format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    video_format.fmt.pix.sizeimage = ctx->jpeg_buffer_capacity;
    if (ioctl(ctx->codec_fd, VIDIOC_S_FMT, &video_format) != 0) {
        ESP_LOGE(TAG, "Failed to configure JPEG output format (errno=%d)", errno);
        return ESP_FAIL;
    }

    if (!reuse_buffers) {
        memset(&request, 0, sizeof(request));
        request.count = 1;
        request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        request.memory = V4L2_MEMORY_MMAP;
        if (ioctl(ctx->codec_fd, VIDIOC_REQBUFS, &request) != 0) {
            ESP_LOGE(TAG, "Failed to request JPEG output buffer (errno=%d)", errno);
            return ESP_FAIL;
        }
    }

    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = 0;
    if (!reuse_buffers) {
        if (ioctl(ctx->codec_fd, VIDIOC_QUERYBUF, &buffer) != 0) {
            ESP_LOGE(TAG, "Failed to query JPEG output buffer (errno=%d)", errno);
            return ESP_FAIL;
        }
        ctx->codec_capture_buffer = mmap(NULL,
                                         buffer.length,
                                         PROT_READ | PROT_WRITE,
                                         MAP_SHARED,
                                         ctx->codec_fd,
                                         buffer.m.offset);
        if (ctx->codec_capture_buffer == MAP_FAILED) {
            ctx->codec_capture_buffer = NULL;
            ESP_LOGE(TAG, "Failed to map JPEG output buffer (errno=%d)", errno);
            return ESP_FAIL;
        }
    }
    if (ioctl(ctx->codec_fd, VIDIOC_QBUF, &buffer) != 0) {
        ESP_LOGE(TAG, "Failed to queue JPEG output buffer (errno=%d)", errno);
        return ESP_FAIL;
    }
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
    /* STREAMOFF retains this fixed-format pool and resets its queues. Reusing
     * it avoids fragmenting PSRAM before the 3.84 MB DVP backup allocation. */
    ctx->buffers_ready = true;
#endif

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(ctx->codec_fd, VIDIOC_STREAMON, &type) != 0) {
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
        ctx->buffers_ready = false;
#endif
        ESP_LOGE(TAG, "Failed to start JPEG output stream (errno=%d)", errno);
        return ESP_FAIL;
    }
    ctx->codec_capture_streaming = true;

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(ctx->codec_fd, VIDIOC_STREAMON, &type) != 0) {
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
        ctx->buffers_ready = false;
#endif
        ESP_LOGE(TAG, "Failed to start JPEG input stream (errno=%d)", errno);
        board_camera_stop_cb(ctx);
        return ESP_FAIL;
    }
    ctx->codec_output_streaming = true;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(ctx->capture_fd, VIDIOC_STREAMON, &type) != 0) {
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
        ctx->buffers_ready = false;
#endif
        ESP_LOGE(TAG, "Failed to start DVP stream (errno=%d)", errno);
        board_camera_stop_cb(ctx);
        return ESP_FAIL;
    }
    ctx->capture_streaming = true;
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
    ctx->startup_frames_to_discard = BOARD_GC2145_STARTUP_DISCARD_COUNT;
#endif
    ctx->frame_width = width;
    ctx->frame_height = height;

    ESP_LOGI(TAG,
             "Camera capture started with hardware JPEG: %dx%d@%dfps",
             width,
             height,
             rate);
    return ESP_OK;
}

static void board_camera_stop_impl(void *cb_ctx)
{
    board_camera_context_t *ctx = (board_camera_context_t *)cb_ctx;
    int type;

    if (ctx == NULL) {
        return;
    }

#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
    ctx->startup_frames_to_discard = 0;
#endif
    if (ctx->capture_streaming) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
        if (ioctl(ctx->capture_fd, VIDIOC_STREAMOFF, &type) != 0) {
            ESP_LOGE(TAG, "Failed to stop DVP stream (errno=%d)", errno);
            return;
        }
#else
        ioctl(ctx->capture_fd, VIDIOC_STREAMOFF, &type);
#endif
        ctx->capture_streaming = false;
    }
    if (ctx->codec_output_streaming) {
        type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
        if (ioctl(ctx->codec_fd, VIDIOC_STREAMOFF, &type) != 0) {
            ESP_LOGE(TAG, "Failed to stop JPEG input stream (errno=%d)", errno);
            return;
        }
#else
        ioctl(ctx->codec_fd, VIDIOC_STREAMOFF, &type);
#endif
        ctx->codec_output_streaming = false;
    }
    if (ctx->codec_capture_streaming) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
        if (ioctl(ctx->codec_fd, VIDIOC_STREAMOFF, &type) != 0) {
            ESP_LOGE(TAG, "Failed to stop JPEG output stream (errno=%d)", errno);
            return;
        }
#else
        ioctl(ctx->codec_fd, VIDIOC_STREAMOFF, &type);
#endif
        ctx->codec_capture_streaming = false;
    }

    ctx->codec_frame_outstanding = false;
    ctx->capture_frame_outstanding = false;
    ctx->direct_jpeg = false;
    ctx->frame_width = 0;
    ctx->frame_height = 0;
}

static board_camera_buffer_t *board_camera_frame_get_impl(void *cb_ctx)
{
    board_camera_context_t *ctx = (board_camera_context_t *)cb_ctx;
    struct v4l2_buffer capture_buffer;
    struct v4l2_buffer codec_input_buffer;
    struct v4l2_buffer codec_output_buffer;
    int64_t timestamp_us;

    if (ctx == NULL || !ctx->capture_streaming ||
            ctx->capture_frame_outstanding || ctx->codec_frame_outstanding) {
        return NULL;
    }

    for (;;) {
        memset(&capture_buffer, 0, sizeof(capture_buffer));
        capture_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        capture_buffer.memory = V4L2_MEMORY_MMAP;
        if (ioctl(ctx->capture_fd, VIDIOC_DQBUF, &capture_buffer) != 0) {
            ESP_LOGE(TAG, "DVP frame dequeue timed out or failed (errno=%d)", errno);
            return NULL;
        }
        if (capture_buffer.index >= ctx->capture_buffer_count) {
            ESP_LOGE(TAG, "DVP returned invalid buffer index %u", (unsigned int)capture_buffer.index);
            board_camera_stop_cb(ctx);
            return NULL;
        }
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
        /* DVP can start mid-frame while bytesused still reports a full UXGA
         * buffer. Drain the startup frames before encoding real camera data. */
        if (ctx->startup_frames_to_discard > 0) {
            if (ioctl(ctx->capture_fd, VIDIOC_QBUF, &capture_buffer) != 0) {
                ESP_LOGE(TAG, "Failed to recycle GC2145 startup frame (errno=%d)", errno);
                board_camera_stop_cb(ctx);
                return NULL;
            }
            ctx->startup_frames_to_discard--;
            continue;
        }
#endif
        break;
    }

    if (ctx->direct_jpeg) {
        if (capture_buffer.bytesused == 0) {
            ioctl(ctx->capture_fd, VIDIOC_QBUF, &capture_buffer);
            ESP_LOGW(TAG, "%s returned an empty JPEG frame", BOARD_CAMERA_SENSOR_NAME);
            return NULL;
        }

        ctx->capture_frame_outstanding = true;
        ctx->capture_frame_index = capture_buffer.index;
        timestamp_us = esp_timer_get_time();
        ctx->frame.buf = ctx->capture_buffer[capture_buffer.index];
        ctx->frame.len = capture_buffer.bytesused;
        ctx->frame.width = ctx->frame_width;
        ctx->frame.height = ctx->frame_height;
        ctx->frame.timestamp.tv_sec = timestamp_us / 1000000;
        ctx->frame.timestamp.tv_usec = timestamp_us % 1000000;
        return &ctx->frame;
    }

    memset(&codec_input_buffer, 0, sizeof(codec_input_buffer));
    codec_input_buffer.index = 0;
    codec_input_buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    codec_input_buffer.memory = V4L2_MEMORY_USERPTR;
    codec_input_buffer.m.userptr =
        (unsigned long)ctx->capture_buffer[capture_buffer.index];
    codec_input_buffer.length = capture_buffer.bytesused;
    if (ioctl(ctx->codec_fd, VIDIOC_QBUF, &codec_input_buffer) != 0) {
        ioctl(ctx->capture_fd, VIDIOC_QBUF, &capture_buffer);
        ESP_LOGE(TAG, "Failed to queue JPEG input frame (errno=%d)", errno);
        return NULL;
    }

    memset(&codec_output_buffer, 0, sizeof(codec_output_buffer));
    codec_output_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    codec_output_buffer.memory = V4L2_MEMORY_MMAP;
    if (ioctl(ctx->codec_fd, VIDIOC_DQBUF, &codec_output_buffer) != 0) {
        ESP_LOGE(TAG, "JPEG frame dequeue timed out or failed (errno=%d)", errno);
        board_camera_stop_cb(ctx);
        return NULL;
    }
    ctx->codec_frame_outstanding = true;

    if (ioctl(ctx->capture_fd, VIDIOC_QBUF, &capture_buffer) != 0) {
        ESP_LOGE(TAG, "Failed to recycle DVP buffer (errno=%d)", errno);
        board_camera_stop_cb(ctx);
        return NULL;
    }
    if (ioctl(ctx->codec_fd, VIDIOC_DQBUF, &codec_input_buffer) != 0) {
        ESP_LOGE(TAG, "Failed to recycle JPEG input buffer (errno=%d)", errno);
        board_camera_stop_cb(ctx);
        return NULL;
    }

    if (codec_output_buffer.bytesused == 0) {
        board_camera_frame_return_cb(NULL, ctx);
        ESP_LOGW(TAG, "Hardware JPEG encoder returned an empty frame");
        return NULL;
    }

    timestamp_us = esp_timer_get_time();
    ctx->frame.buf = ctx->codec_capture_buffer;
    ctx->frame.len = codec_output_buffer.bytesused;
    ctx->frame.width = ctx->frame_width;
    ctx->frame.height = ctx->frame_height;
    ctx->frame.timestamp.tv_sec = timestamp_us / 1000000;
    ctx->frame.timestamp.tv_usec = timestamp_us % 1000000;

    return &ctx->frame;
}

static void board_camera_frame_return_impl(board_camera_buffer_t *frame, void *cb_ctx)
{
    board_camera_context_t *ctx = (board_camera_context_t *)cb_ctx;
    struct v4l2_buffer codec_output_buffer;

    (void)frame;

    if (ctx == NULL) {
        return;
    }

    if (ctx->direct_jpeg) {
        if (!ctx->capture_streaming || !ctx->capture_frame_outstanding) {
            return;
        }

        struct v4l2_buffer capture_buffer = {
            .index = ctx->capture_frame_index,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };
        if (ioctl(ctx->capture_fd, VIDIOC_QBUF, &capture_buffer) != 0) {
            ESP_LOGE(TAG, "Failed to recycle native JPEG buffer (errno=%d)", errno);
        }
        ctx->capture_frame_outstanding = false;
        return;
    }

    if (!ctx->codec_capture_streaming || !ctx->codec_frame_outstanding) {
        return;
    }

    memset(&codec_output_buffer, 0, sizeof(codec_output_buffer));
    codec_output_buffer.index = 0;
    codec_output_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    codec_output_buffer.memory = V4L2_MEMORY_MMAP;
    if (ioctl(ctx->codec_fd, VIDIOC_QBUF, &codec_output_buffer) != 0) {
        ESP_LOGE(TAG, "Failed to recycle JPEG output buffer (errno=%d)", errno);
    }
    ctx->codec_frame_outstanding = false;
}

static void board_close_video_devices(board_camera_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->stream_mutex != NULL) {
        board_camera_stop_cb(ctx);
    }

    if (ctx->codec_fd >= 0) {
        close(ctx->codec_fd);
        ctx->codec_fd = -1;
    }
    if (ctx->capture_fd >= 0) {
        close(ctx->capture_fd);
        ctx->capture_fd = -1;
    }

    ctx->codec_capture_buffer = NULL;
    ctx->capture_buffer_count = 0;
    ctx->capture_frame_outstanding = false;
    ctx->direct_jpeg = false;
    memset(ctx->capture_buffer, 0, sizeof(ctx->capture_buffer));
    ctx->capture_streaming = false;
    ctx->codec_output_streaming = false;
    ctx->codec_capture_streaming = false;
    ctx->codec_frame_outstanding = false;
    ctx->frame_lock_held = false;
#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
    ctx->buffers_ready = false;
#endif
}
