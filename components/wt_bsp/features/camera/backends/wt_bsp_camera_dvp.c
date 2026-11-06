/**
 * @file wt_bsp_camera_dvp.c
 * @author Wireless-Tag
 * @brief Reusable DVP capture, serialized JPEG frames and video resource ownership.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_camera_dvp.h"
#include "wt_bsp_camera_sensor.h"

#if WT_BSP_CAMERA_ENABLED && CONFIG_SOC_LCDCAM_CAM_SUPPORTED && CONFIG_SOC_JPEG_ENCODE_SUPPORTED

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "driver/jpeg_encode.h"
#include "esp_cam_sensor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"
#include "linux/videodev2.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* ==================== [Defines] =========================================== */

#define CAMERA_DVP_MAX_BUFFERS 3
#define CAMERA_DVP_VIDEO_FLAGS (ESP_VIDEO_INIT_FLAGS_DVP | ESP_VIDEO_INIT_FLAGS_JPEG_ENC)

/* ==================== [Typedefs] ========================================== */

typedef struct {
    uint8_t *buf;
    size_t len;
    uint32_t width;
    uint32_t height;
    struct timeval timestamp;
} camera_dvp_frame_t;

typedef struct {
    const wt_bsp_camera_info_t *info;
    const wt_bsp_camera_sensor_ops_t *sensor;
    void *sensor_state;
    int capture_fd;
    int codec_fd;
    uint8_t *capture_buffer[CAMERA_DVP_MAX_BUFFERS];
    size_t capture_buffer_size[CAMERA_DVP_MAX_BUFFERS];
    uint8_t *codec_capture_buffer;
    size_t codec_capture_capacity;
    size_t jpeg_buffer_capacity;
    camera_dvp_frame_t frame;
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
    bool buffers_ready;
    uint32_t startup_frames_to_discard;
    bool video_initialized;
    bool frame_borrowed;
    jpeg_encoder_handle_t jpeg_encoder;
    SemaphoreHandle_t stream_mutex;
    wt_bsp_camera_status_t health;
} camera_dvp_context_t;

/* ==================== [Static Prototypes] ================================= */

static esp_err_t camera_prepare(camera_dvp_context_t *ctx);
static esp_err_t camera_video_init(camera_dvp_context_t *ctx);
static esp_err_t camera_video_deinit(camera_dvp_context_t *ctx);
static esp_err_t camera_open_devices(camera_dvp_context_t *ctx);
static esp_err_t camera_close_devices(camera_dvp_context_t *ctx);
static esp_err_t camera_set_dequeue_timeout(camera_dvp_context_t *ctx, int fd, const char *name);
static esp_err_t camera_verify_sensor(camera_dvp_context_t *ctx);
static esp_err_t camera_start(camera_dvp_context_t *ctx);
static esp_err_t camera_stop(camera_dvp_context_t *ctx);
static camera_dvp_frame_t *camera_get_frame(camera_dvp_context_t *ctx);
static void camera_return_frame(camera_dvp_context_t *ctx);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_camera_dvp";
/* The SoC has one DVP capture peripheral. Objects retain separate state, but
 * only this owner may select a sensor and initialize its esp_video devices. */
static wt_bsp_camera_t s_dvp_owner;
static const wt_bsp_camera_ops_t s_camera_ops = {
    .deinit = wt_bsp_camera_dvp_deinit,
    .prepare = wt_bsp_camera_dvp_prepare,
    .capture = wt_bsp_camera_dvp_capture,
    .get_status = wt_bsp_camera_dvp_get_status,
};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

bool wt_bsp_camera_sensor_is_selected(const wt_bsp_camera_sensor_ops_t *sensor)
{
    if (sensor == NULL || s_dvp_owner == NULL || s_dvp_owner->backend == NULL) {
        return false;
    }
    camera_dvp_context_t *ctx = s_dvp_owner->backend;
    return ctx->sensor == sensor;
}

esp_err_t wt_bsp_camera_dvp_init(wt_bsp_camera_t camera,
                               const wt_bsp_camera_sensor_ops_t *sensor,
                               const wt_bsp_camera_ops_t **ops)
{
    if (camera == NULL || sensor == NULL || ops == NULL || sensor->name == NULL ||
            sensor->name[0] == '\0' || sensor->pixel_format == 0) {
        ESP_LOGE(TAG, "Invalid camera object or sensor profile");
        return ESP_ERR_INVALID_ARG;
    }
    if (camera->backend != NULL) {
        camera_dvp_context_t *existing = camera->backend;
        if (s_dvp_owner != camera || existing->sensor != sensor) {
            return ESP_ERR_INVALID_STATE;
        }
        *ops = &s_camera_ops;
        return ESP_OK;
    }
    if (s_dvp_owner != NULL) {
        ESP_LOGE(TAG, "Another camera owns the DVP peripheral");
        return ESP_ERR_INVALID_STATE;
    }
    const wt_bsp_camera_info_t *info = &camera->info;
    if (info->capture_buffer_count == 0 || info->capture_buffer_count > CAMERA_DVP_MAX_BUFFERS ||
            info->jpeg_buffer_capacity == 0 || info->dequeue_timeout_ms <= 0 ||
            info->jpeg_encode_timeout_ms <= 0 || info->jpeg_quality < 1 ||
            info->jpeg_quality > 100 || info->dvp.sccb_freq_hz == 0 ||
            info->dvp.xclk_freq_hz == 0) {
        ESP_LOGE(TAG, "Invalid DVP camera wiring or resource budget");
        return ESP_ERR_INVALID_ARG;
    }

    camera_dvp_context_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ctx->stream_mutex = xSemaphoreCreateRecursiveMutex();
    if (ctx->stream_mutex == NULL) {
        free(ctx);
        return ESP_ERR_NO_MEM;
    }
    ctx->info = info;
    ctx->sensor = sensor;
    ctx->capture_fd = -1;
    ctx->codec_fd = -1;
    ctx->jpeg_buffer_capacity = info->jpeg_buffer_capacity;
    ctx->health.last_error = ESP_ERR_INVALID_STATE;
    camera->backend = ctx;
    s_dvp_owner = camera;
    *ops = &s_camera_ops;
    return ESP_OK;
}

esp_err_t wt_bsp_camera_dvp_deinit(wt_bsp_camera_t camera)
{
    if (camera == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    camera_dvp_context_t *ctx = camera->backend;
    if (ctx == NULL) {
        return ESP_OK;
    }
    if (s_dvp_owner != camera) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTakeRecursive(ctx->stream_mutex, portMAX_DELAY);
    if (ctx->frame_borrowed) {
        xSemaphoreGiveRecursive(ctx->stream_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = camera_close_devices(ctx);
    if (ret == ESP_OK) {
        ret = camera_video_deinit(ctx);
    }
    if (ret != ESP_OK) {
        ctx->health.ready = false;
        ctx->health.last_error = ret;
        xSemaphoreGiveRecursive(ctx->stream_mutex);
        return ret;
    }
    SemaphoreHandle_t mutex = ctx->stream_mutex;
    camera->backend = NULL;
    s_dvp_owner = NULL;
    xSemaphoreGiveRecursive(mutex);
    vSemaphoreDelete(mutex);
    free(ctx);
    return ESP_OK;
}

esp_err_t wt_bsp_camera_dvp_prepare(wt_bsp_camera_t camera, wt_bsp_camera_format_t *format)
{
    camera_dvp_context_t *ctx = camera != NULL ? camera->backend : NULL;
    if (ctx == NULL || format == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTakeRecursive(ctx->stream_mutex, portMAX_DELAY);
    esp_err_t ret = ctx->frame_borrowed ? ESP_ERR_INVALID_STATE : camera_prepare(ctx);
    if (ret == ESP_OK) {
        *format = ctx->info->format;
    }
    xSemaphoreGiveRecursive(ctx->stream_mutex);
    return ret;
}

esp_err_t wt_bsp_camera_dvp_capture(wt_bsp_camera_t camera,
                                  wt_bsp_camera_frame_cb_t callback, void *user_data)
{
    camera_dvp_context_t *ctx = camera != NULL ? camera->backend : NULL;
    if (ctx == NULL || callback == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTakeRecursive(ctx->stream_mutex, portMAX_DELAY);
    esp_err_t ret = ESP_OK;
    if (ctx->frame_borrowed) {
        ret = ESP_ERR_INVALID_STATE;
        goto done;
    }
    ret = camera_prepare(ctx);
    if (ret != ESP_OK) {
        goto done;
    }
    if (!ctx->capture_streaming) {
        ret = camera_start(ctx);
        ctx->health.last_error = ret;
        if (ret != ESP_OK) {
            esp_err_t cleanup = camera_close_devices(ctx);
            if (cleanup == ESP_OK) {
                cleanup = camera_video_deinit(ctx);
            }
            ctx->health.ready = false;
            if (cleanup != ESP_OK) {
                ESP_LOGE(TAG, "Capture startup cleanup failed: %s", esp_err_to_name(cleanup));
            }
            goto done;
        }
    }
    camera_dvp_frame_t *frame = camera_get_frame(ctx);
    if (frame != NULL && (frame->len < 4 || frame->buf[0] != 0xff || frame->buf[1] != 0xd8 ||
            frame->buf[frame->len - 2] != 0xff || frame->buf[frame->len - 1] != 0xd9)) {
        camera_return_frame(ctx);
        frame = NULL;
        ESP_LOGE(TAG, "Camera returned an incomplete JPEG");
    }
    ctx->health.last_error = frame != NULL ? ESP_OK : ESP_FAIL;
    if (frame == NULL) {
        ret = ESP_FAIL;
        goto done;
    }
    ctx->health.frames++;
    ctx->health.last_frame_us = esp_timer_get_time();
    const wt_bsp_camera_frame_t jpeg = {
        .data = frame->buf,
        .length = frame->len,
        .width = frame->width,
        .height = frame->height,
        .timestamp_us = (int64_t)frame->timestamp.tv_sec * 1000000 + frame->timestamp.tv_usec,
    };
    ctx->frame_borrowed = true;
    ret = callback(&jpeg, user_data);
    ctx->frame_borrowed = false;
    camera_return_frame(ctx);
done:
    xSemaphoreGiveRecursive(ctx->stream_mutex);
    return ret;
}

esp_err_t wt_bsp_camera_dvp_get_status(wt_bsp_camera_t camera, wt_bsp_camera_status_t *status)
{
    camera_dvp_context_t *ctx = camera != NULL ? camera->backend : NULL;
    if (ctx == NULL || status == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTakeRecursive(ctx->stream_mutex, portMAX_DELAY);
    *status = ctx->health;
    xSemaphoreGiveRecursive(ctx->stream_mutex);
    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static esp_err_t camera_prepare(camera_dvp_context_t *ctx)
{
    if (ctx->health.ready && ctx->video_initialized && ctx->capture_fd >= 0 && ctx->codec_fd >= 0) {
        return ESP_OK;
    }
    /* A previous partial setup may still retain driver pointers after failed
     * cleanup. Finish that cleanup before replacing the profile or encoder. */
    esp_err_t ret = camera_close_devices(ctx);
    if (ret == ESP_OK) {
        ret = camera_video_deinit(ctx);
    }
    if (ret == ESP_OK) {
        ret = camera_video_init(ctx);
    }
    if (ret == ESP_OK) {
        ret = camera_open_devices(ctx);
    }
    if (ret != ESP_OK) {
        esp_err_t cleanup = camera_close_devices(ctx);
        if (cleanup == ESP_OK) {
            cleanup = camera_video_deinit(ctx);
        }
        if (cleanup != ESP_OK) {
            ESP_LOGE(TAG, "Camera preparation cleanup failed: %s", esp_err_to_name(cleanup));
        }
    }
    ctx->health.ready = ret == ESP_OK;
    ctx->health.last_error = ret;
    return ret;
}

static esp_err_t camera_video_init(camera_dvp_context_t *ctx)
{
    esp_err_t ret = ESP_OK;
    esp_video_init_jpeg_enc_config_t jpeg_config = {0};
    if (ctx->sensor->pixel_format != V4L2_PIX_FMT_JPEG) {
        const jpeg_encode_engine_cfg_t engine_config = {
            .intr_priority = 0,
            .timeout_ms = ctx->info->jpeg_encode_timeout_ms,
        };
        ret = jpeg_new_encoder_engine(&engine_config, &ctx->jpeg_encoder);
        if (ret != ESP_OK) {
            return ret;
        }
        jpeg_config.enc_handle = ctx->jpeg_encoder;
        ESP_LOGI(TAG, "Hardware JPEG encode timeout=%d ms", engine_config.timeout_ms);
    }
    const wt_bsp_camera_dvp_info_t *pins = &ctx->info->dvp;
    const esp_video_init_dvp_config_t dvp_config = {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port = pins->sccb_port,
                .scl_pin = pins->sccb_scl_gpio,
                .sda_pin = pins->sccb_sda_gpio,
            },
            .freq = pins->sccb_freq_hz,
        },
        .reset_pin = pins->reset_gpio,
        .pwdn_pin = pins->pwdn_gpio,
        .dvp_pin = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {
                pins->data_gpio[0], pins->data_gpio[1], pins->data_gpio[2], pins->data_gpio[3],
                pins->data_gpio[4], pins->data_gpio[5], pins->data_gpio[6], pins->data_gpio[7],
            },
            .vsync_io = pins->vsync_gpio,
            .de_io = pins->de_gpio,
            .pclk_io = pins->pclk_gpio,
            .xclk_io = pins->xclk_gpio,
        },
        .xclk_freq = pins->xclk_freq_hz,
    };
    const esp_video_init_config_t video_config = {
        .dvp = &dvp_config,
        .jpeg_enc = ctx->jpeg_encoder != NULL ? &jpeg_config : NULL,
    };
    // Even failed initialization can leave devices borrowing these resources.
    ctx->video_initialized = true;
    return esp_video_init_with_flags(&video_config, CAMERA_DVP_VIDEO_FLAGS);
}

static esp_err_t camera_video_deinit(camera_dvp_context_t *ctx)
{
    esp_err_t ret = ESP_OK;
    if (ctx->video_initialized) {
        ret = esp_video_deinit_with_flags(CAMERA_DVP_VIDEO_FLAGS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Video deinitialization failed: %s", esp_err_to_name(ret));
            return ret;
        }
        ctx->video_initialized = false;
    }
    // The sensor and video device have now released their borrowed pointers.
    if (ctx->sensor_state != NULL) {
        if (ctx->sensor->release == NULL) {
            ESP_LOGE(TAG, "Sensor %s retained state without a release operation", ctx->sensor->name);
            return ESP_ERR_INVALID_STATE;
        }
        ctx->sensor->release(ctx->sensor_state);
        ctx->sensor_state = NULL;
    }
    if (ctx->jpeg_encoder != NULL) {
        ret = jpeg_del_encoder_engine(ctx->jpeg_encoder);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "JPEG engine deletion failed: %s", esp_err_to_name(ret));
            return ret;
        }
        ctx->jpeg_encoder = NULL;
    }
    return ESP_OK;
}

static esp_err_t camera_open_devices(camera_dvp_context_t *ctx)
{
    ctx->capture_fd = open(ESP_VIDEO_DVP_DEVICE_NAME, O_RDONLY);
    if (ctx->capture_fd < 0) {
        ESP_LOGE(TAG, "Cannot open DVP device (sensor unavailable, errno=%d)", errno);
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t ret = camera_verify_sensor(ctx);
    if (ret != ESP_OK) {
        return ret;
    }
    if (ctx->sensor->prepare != NULL) {
        ret = ctx->sensor->prepare(ctx->capture_fd, ctx->info, &ctx->sensor_state);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    ctx->codec_fd = open(ESP_VIDEO_JPEG_ENC_DEVICE_NAME, O_RDONLY);
    if (ctx->codec_fd < 0) {
        ESP_LOGE(TAG, "Cannot open JPEG device (errno=%d)", errno);
        return ESP_ERR_NOT_FOUND;
    }
    ret = camera_set_dequeue_timeout(ctx, ctx->capture_fd, "DVP");
    if (ret == ESP_OK) {
        ret = camera_set_dequeue_timeout(ctx, ctx->codec_fd, "JPEG");
    }
    if (ret != ESP_OK) {
        return ret;
    }
    struct v4l2_ext_control control = {
        .id = V4L2_CID_JPEG_COMPRESSION_QUALITY,
        .value = ctx->info->jpeg_quality,
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CID_JPEG_CLASS,
        .count = 1,
        .controls = &control,
    };
    if (ioctl(ctx->codec_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "Could not set JPEG quality to %d (errno=%d)", ctx->info->jpeg_quality, errno);
    }
    return ESP_OK;
}

static esp_err_t camera_set_dequeue_timeout(camera_dvp_context_t *ctx, int fd, const char *name)
{
    struct timeval timeout = {
        .tv_sec = ctx->info->dequeue_timeout_ms / 1000,
        .tv_usec = (ctx->info->dequeue_timeout_ms % 1000) * 1000,
    };
    if (ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) != 0) {
        ESP_LOGE(TAG, "Failed to set %s dequeue timeout (errno=%d)", name, errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t camera_verify_sensor(camera_dvp_context_t *ctx)
{
    esp_cam_sensor_id_t sensor_id = {0};
    struct v4l2_ext_control control = {
        .id = ESP_CAM_SENSOR_IOC_G_CHIP_ID,
        .p_u8 = (uint8_t *)&sensor_id,
        .size = sizeof(sensor_id),
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CTRL_CLASS_ESP_CAM_IOCTL,
        .count = 1,
        .controls = &control,
    };
    if (ioctl(ctx->capture_fd, VIDIOC_G_EXT_CTRLS, &controls) != 0) {
        ESP_LOGE(TAG, "Could not read camera sensor ID (errno=%d)", errno);
        return ESP_FAIL;
    }
    if (sensor_id.pid != ctx->sensor->pid) {
        ESP_LOGE(TAG, "Camera selection mismatch: configured=%s PID=0x%04x, detected=0x%04x",
                 ctx->sensor->name, (unsigned int)ctx->sensor->pid, (unsigned int)sensor_id.pid);
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "Configured camera detected: %s PID=0x%04x", ctx->sensor->name, (unsigned int)sensor_id.pid);
    return ESP_OK;
}

static esp_err_t camera_start(camera_dvp_context_t *ctx)
{
    uint32_t width = ctx->info->format.width;
    uint32_t height = ctx->info->format.height;
    uint32_t rate = ctx->info->format.fps;
    struct v4l2_buffer buffer;
    struct v4l2_requestbuffers request;
    struct v4l2_format video_format;
    uint32_t capture_format = 0;
    bool reuse_buffers = false;
    int type;

    esp_err_t stop_ret = camera_stop(ctx);
    if (stop_ret != ESP_OK) {
        return stop_ret;
    }
    if (ctx->sensor->reuse_buffers) {
        reuse_buffers = ctx->buffers_ready;
        /* Any partial setup below must not be reused as a complete buffer pool. */
        ctx->buffers_ready = false;
        if (reuse_buffers) {
            ESP_LOGI(TAG, "Fixed-format camera buffers reused");
        }
    }

    for (uint32_t index = 0; capture_format == 0; index++) {
        struct v4l2_fmtdesc format_desc = {
            .index = index,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        };
        if (ioctl(ctx->capture_fd, VIDIOC_ENUM_FMT, &format_desc) != 0) {
            break;
        }
        if (format_desc.pixelformat == ctx->sensor->pixel_format) {
            capture_format = format_desc.pixelformat;
        }
    }

    if (capture_format == 0) {
        ESP_LOGE(TAG, "Configured %s capture format is unavailable", ctx->sensor->name);
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
    if (capture_format != ctx->sensor->pixel_format ||
            video_format.fmt.pix.width != (uint32_t)width ||
            video_format.fmt.pix.height != (uint32_t)height) {
        ESP_LOGE(TAG,
                 "Configured %s returned unexpected mode %c%c%c%c %ux%u",
                 ctx->sensor->name,
                 (char)(capture_format & 0xff),
                 (char)((capture_format >> 8) & 0xff),
                 (char)((capture_format >> 16) & 0xff),
                 (char)((capture_format >> 24) & 0xff),
                 (unsigned int)video_format.fmt.pix.width,
                 (unsigned int)video_format.fmt.pix.height);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (ctx->sensor->configure != NULL) {
        esp_err_t tuning = ctx->sensor->configure(ctx->capture_fd, ctx->info, ctx->sensor_state);
        if (tuning != ESP_OK) {
            ESP_LOGW(TAG, "Continuing after optional %s tuning failed: %s",
                     ctx->sensor->name, esp_err_to_name(tuning));
        }
    }
    ESP_LOGI(TAG,
             "DVP capture format: %c%c%c%c (%s)",
             (char)(capture_format & 0xff),
             (char)((capture_format >> 8) & 0xff),
             (char)((capture_format >> 16) & 0xff),
             (char)((capture_format >> 24) & 0xff),
             ctx->direct_jpeg ? "sensor native JPEG" : "hardware JPEG input");

    if (!reuse_buffers) {
        memset(&request, 0, sizeof(request));
        request.count = ctx->info->capture_buffer_count;
        request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        request.memory = V4L2_MEMORY_MMAP;
        if (ioctl(ctx->capture_fd, VIDIOC_REQBUFS, &request) != 0 || request.count == 0) {
            ESP_LOGE(TAG, "Failed to request DVP buffers (errno=%d)", errno);
            return ESP_FAIL;
        }
        if (request.count > ctx->info->capture_buffer_count) {
            ESP_LOGE(TAG, "DVP buffer allocation exceeds the configured budget");
            return ESP_ERR_NOT_SUPPORTED;
        }
        ctx->capture_buffer_count = request.count;
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

            ctx->capture_buffer_size[i] = buffer.length;
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
        ctx->buffers_ready = ctx->sensor->reuse_buffers;
        ctx->startup_frames_to_discard = ctx->info->startup_frames_to_discard;
        ctx->frame_width = width;
        ctx->frame_height = height;
        ESP_LOGI(TAG, "Camera capture started with sensor-native JPEG: %ux%u@%ufps",
                 (unsigned int)width, (unsigned int)height, (unsigned int)rate);
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
    if (video_format.fmt.pix.width != width || video_format.fmt.pix.height != height ||
            video_format.fmt.pix.pixelformat != capture_format) {
        ESP_LOGE(TAG, "JPEG encoder changed the requested input format");
        return ESP_ERR_NOT_SUPPORTED;
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
    if (video_format.fmt.pix.width != width || video_format.fmt.pix.height != height ||
            video_format.fmt.pix.pixelformat != V4L2_PIX_FMT_JPEG) {
        ESP_LOGE(TAG, "JPEG encoder changed the requested output format");
        return ESP_ERR_NOT_SUPPORTED;
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
        ctx->codec_capture_capacity = buffer.length;
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
    if (ctx->sensor->reuse_buffers) {
        /* STREAMOFF retains the fixed-format pool and resets its queues.
         * Reuse avoids fragmenting PSRAM before the large DVP backup allocation. */
        ctx->buffers_ready = true;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(ctx->codec_fd, VIDIOC_STREAMON, &type) != 0) {
        ctx->buffers_ready = false;
        ESP_LOGE(TAG, "Failed to start JPEG output stream (errno=%d)", errno);
        return ESP_FAIL;
    }
    ctx->codec_capture_streaming = true;

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(ctx->codec_fd, VIDIOC_STREAMON, &type) != 0) {
        ctx->buffers_ready = false;
        ESP_LOGE(TAG, "Failed to start JPEG input stream (errno=%d)", errno);
        camera_stop(ctx);
        return ESP_FAIL;
    }
    ctx->codec_output_streaming = true;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(ctx->capture_fd, VIDIOC_STREAMON, &type) != 0) {
        ctx->buffers_ready = false;
        ESP_LOGE(TAG, "Failed to start DVP stream (errno=%d)", errno);
        camera_stop(ctx);
        return ESP_FAIL;
    }
    ctx->capture_streaming = true;
    ctx->startup_frames_to_discard = ctx->info->startup_frames_to_discard;
    ctx->frame_width = width;
    ctx->frame_height = height;

    ESP_LOGI(TAG,
             "Camera capture started with hardware JPEG: %ux%u@%ufps",
             (unsigned int)width,
             (unsigned int)height,
             (unsigned int)rate);
    return ESP_OK;
}

static camera_dvp_frame_t *camera_get_frame(camera_dvp_context_t *ctx)
{
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
        if (capture_buffer.index >= ctx->capture_buffer_count ||
                capture_buffer.bytesused > ctx->capture_buffer_size[capture_buffer.index]) {
            ESP_LOGE(TAG, "DVP returned invalid buffer index %u", (unsigned int)capture_buffer.index);
            camera_stop(ctx);
            return NULL;
        }
        /* DVP can start mid-frame while bytesused still reports a full UXGA
         * buffer. Drain the startup frames before encoding real camera data. */
        if (ctx->startup_frames_to_discard > 0) {
            if (ioctl(ctx->capture_fd, VIDIOC_QBUF, &capture_buffer) != 0) {
                ESP_LOGE(TAG, "Failed to recycle camera startup frame (errno=%d)", errno);
                camera_stop(ctx);
                return NULL;
            }
            ctx->startup_frames_to_discard--;
            continue;
        }
        break;
    }

    if (ctx->direct_jpeg) {
        if (capture_buffer.bytesused == 0) {
            ioctl(ctx->capture_fd, VIDIOC_QBUF, &capture_buffer);
            ESP_LOGW(TAG, "%s returned an empty JPEG frame", ctx->sensor->name);
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
        camera_stop(ctx);
        return NULL;
    }
    ctx->codec_frame_outstanding = true;

    if (ioctl(ctx->capture_fd, VIDIOC_QBUF, &capture_buffer) != 0) {
        ESP_LOGE(TAG, "Failed to recycle DVP buffer (errno=%d)", errno);
        camera_stop(ctx);
        return NULL;
    }
    if (ioctl(ctx->codec_fd, VIDIOC_DQBUF, &codec_input_buffer) != 0) {
        ESP_LOGE(TAG, "Failed to recycle JPEG input buffer (errno=%d)", errno);
        camera_stop(ctx);
        return NULL;
    }

    if (codec_output_buffer.bytesused == 0 ||
            codec_output_buffer.bytesused > ctx->codec_capture_capacity) {
        camera_return_frame(ctx);
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

static void camera_return_frame(camera_dvp_context_t *ctx)
{
    struct v4l2_buffer codec_output_buffer;

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

static esp_err_t camera_stop(camera_dvp_context_t *ctx)
{
    int type;
    ctx->startup_frames_to_discard = 0;
    if (ctx->capture_streaming) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(ctx->capture_fd, VIDIOC_STREAMOFF, &type) != 0) {
            ESP_LOGE(TAG, "Failed to stop DVP stream (errno=%d)", errno);
            return ESP_FAIL;
        }
        ctx->capture_streaming = false;
    }
    if (ctx->codec_output_streaming) {
        type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        if (ioctl(ctx->codec_fd, VIDIOC_STREAMOFF, &type) != 0) {
            ESP_LOGE(TAG, "Failed to stop JPEG input stream (errno=%d)", errno);
            return ESP_FAIL;
        }
        ctx->codec_output_streaming = false;
    }
    if (ctx->codec_capture_streaming) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(ctx->codec_fd, VIDIOC_STREAMOFF, &type) != 0) {
            ESP_LOGE(TAG, "Failed to stop JPEG output stream (errno=%d)", errno);
            return ESP_FAIL;
        }
        ctx->codec_capture_streaming = false;
    }
    ctx->codec_frame_outstanding = false;
    ctx->capture_frame_outstanding = false;
    ctx->direct_jpeg = false;
    ctx->frame_width = 0;
    ctx->frame_height = 0;
    return ESP_OK;
}

static esp_err_t camera_close_devices(camera_dvp_context_t *ctx)
{
    esp_err_t ret = camera_stop(ctx);
    if (ret != ESP_OK) {
        return ret;
    }
    if (ctx->codec_fd >= 0) {
        if (close(ctx->codec_fd) != 0) {
            ESP_LOGE(TAG, "Failed to close JPEG device (errno=%d)", errno);
            return ESP_FAIL;
        }
        ctx->codec_fd = -1;
    }
    if (ctx->capture_fd >= 0) {
        if (close(ctx->capture_fd) != 0) {
            ESP_LOGE(TAG, "Failed to close DVP device (errno=%d)", errno);
            return ESP_FAIL;
        }
        ctx->capture_fd = -1;
    }
    ctx->codec_capture_buffer = NULL;
    ctx->codec_capture_capacity = 0;
    ctx->capture_buffer_count = 0;
    memset(ctx->capture_buffer, 0, sizeof(ctx->capture_buffer));
    memset(ctx->capture_buffer_size, 0, sizeof(ctx->capture_buffer_size));
    ctx->buffers_ready = false;
    return ESP_OK;
}

#endif // WT_BSP_CAMERA_ENABLED && CONFIG_SOC_LCDCAM_CAM_SUPPORTED && CONFIG_SOC_JPEG_ENCODE_SUPPORTED
