/**
 * @file camera_uvc_csi.c
 * @author Wireless-Tag
 * @brief Application CSI frame callbacks, JPEG conversion and UVC delivery.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "camera_uvc_csi.h"
#include "wt_bsp.h"

#if WT_BSP_USB_DEVICE_UVC_ENABLED && WT_BSP_CSI_ENABLED

#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_video_device.h"
#include "esp_video_ioctl.h"
#include "linux/videodev2.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* ==================== [Defines] =========================================== */

#define CAMERA_UVC_CSI_TIMEOUT_MS 2000
#define CAMERA_UVC_CSI_JPEG_QUALITY 80
#define CAMERA_UVC_CSI_WORKER_STOP_MS 5000
#define CAMERA_UVC_CSI_FRAME_READY_BIT (1UL << 0)
#define CAMERA_UVC_CSI_WORKER_STOP_BIT (1UL << 1)

/* ==================== [Typedefs] ========================================== */

typedef struct {
    wt_bsp_csi_t csi;
    wt_bsp_usb_device_uvc_t uvc;
    int codec_fd;
    uint8_t *codec_capture_buffer;
    uint8_t *usb_buffer;
    uint8_t *latest_jpeg;
    uint8_t *frame_buffer;
    uint8_t *raw_buffer;
    size_t raw_capacity;
    size_t raw_length;
    size_t capacity;
    size_t latest_length;
    int64_t latest_timestamp_us;
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
    esp_err_t frame_error;
    uvc_fb_t frame;
    SemaphoreHandle_t lifecycle_lock;
    SemaphoreHandle_t frame_lock;
    SemaphoreHandle_t frame_ready;
    SemaphoreHandle_t worker_done;
    TaskHandle_t worker;
    atomic_bool raw_busy;
    atomic_bool raw_ready;
    bool worker_stop_requested;
    atomic_bool streaming;
    bool source_started;
    bool codec_output_streaming;
    bool codec_capture_streaming;
    bool initialized;
} camera_uvc_csi_context_t;

/* ==================== [Static Prototypes] ================================= */

static esp_err_t camera_uvc_csi_stream_start(uvc_format_t format, int width, int height,
                                           int rate, void *context);
static void camera_uvc_csi_stream_stop(void *context);
static uvc_fb_t *camera_uvc_csi_get_frame(void *context);
static void camera_uvc_csi_return_frame(uvc_fb_t *frame, void *context);
static void camera_uvc_csi_consume_frame(uint8_t *buffer, uint32_t width, uint32_t height,
                                        size_t length, void *context);
static void camera_uvc_csi_encode_task(void *context);
static esp_err_t camera_uvc_csi_open_codec(camera_uvc_csi_context_t *bridge);
static void camera_uvc_csi_close_codec(camera_uvc_csi_context_t *bridge);
static esp_err_t camera_uvc_csi_stop_source(camera_uvc_csi_context_t *bridge);
static void camera_uvc_csi_release_buffers(camera_uvc_csi_context_t *bridge);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "camera_uvc_csi";
static camera_uvc_csi_context_t s_bridge = {.codec_fd = -1};
static StaticSemaphore_t s_lifecycle_lock_storage;
static StaticSemaphore_t s_frame_lock_storage;
static StaticSemaphore_t s_frame_ready_storage;
static StaticSemaphore_t s_worker_done_storage;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t camera_uvc_csi_start(void)
{
    esp_err_t ret = ESP_OK;
    if (s_bridge.initialized) {
        return ESP_OK;
    }
    s_bridge.csi = wt_bsp_get_csi();
    s_bridge.uvc = wt_bsp_get_usb_device_uvc();
    if (s_bridge.csi == NULL || s_bridge.uvc == NULL) {
        ESP_LOGE(TAG, "CSI or USB UVC feature is unavailable");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_bridge.lifecycle_lock == NULL) {
        s_bridge.lifecycle_lock = xSemaphoreCreateMutexStatic(&s_lifecycle_lock_storage);
        s_bridge.frame_lock = xSemaphoreCreateMutexStatic(&s_frame_lock_storage);
        s_bridge.frame_ready = xSemaphoreCreateBinaryStatic(&s_frame_ready_storage);
        s_bridge.worker_done = xSemaphoreCreateBinaryStatic(&s_worker_done_storage);
    }
    if (s_bridge.lifecycle_lock == NULL || s_bridge.frame_lock == NULL || s_bridge.frame_ready == NULL || s_bridge.worker_done == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_bridge.capacity = (size_t)CONFIG_UVC_CAM1_FRAMESIZE_WIDTH * CONFIG_UVC_CAM1_FRAMESIZE_HEIGT;
    s_bridge.raw_capacity = s_bridge.capacity * 3;
    // JPEG USERPTR buffers must satisfy the external-memory cache alignment.
    s_bridge.raw_buffer = heap_caps_malloc(s_bridge.raw_capacity,
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT | MALLOC_CAP_CACHE_ALIGNED);
    s_bridge.usb_buffer = malloc(s_bridge.capacity);
    s_bridge.latest_jpeg = malloc(s_bridge.capacity);
    s_bridge.frame_buffer = malloc(s_bridge.capacity);
    if (s_bridge.usb_buffer == NULL || s_bridge.latest_jpeg == NULL || s_bridge.frame_buffer == NULL || s_bridge.raw_buffer == NULL) {
        camera_uvc_csi_release_buffers(&s_bridge);
        return ESP_ERR_NO_MEM;
    }
    uvc_device_config_t stream = {
        .uvc_buffer = s_bridge.usb_buffer,
        .uvc_buffer_size = s_bridge.capacity,
        .start_cb = camera_uvc_csi_stream_start,
        .stop_cb = camera_uvc_csi_stream_stop,
        .fb_get_cb = camera_uvc_csi_get_frame,
        .fb_return_cb = camera_uvc_csi_return_frame,
        .cb_ctx = &s_bridge,
    };
    ret = wt_bsp_usb_device_uvc_start(s_bridge.uvc, &stream);
    if (ret != ESP_OK) {
        camera_uvc_csi_release_buffers(&s_bridge);
        return ret;
    }
    s_bridge.initialized = true;
    return ESP_OK;
}

esp_err_t camera_uvc_csi_stop(void)
{
    if (!s_bridge.initialized) {
        return ESP_OK;
    }
    atomic_store(&s_bridge.streaming, false);
    esp_err_t ret = wt_bsp_usb_device_uvc_stop(s_bridge.uvc);
    if (ret != ESP_OK) {
        return ret;
    }
    xSemaphoreTake(s_bridge.lifecycle_lock, portMAX_DELAY);
    ret = camera_uvc_csi_stop_source(&s_bridge);
    if (ret == ESP_OK) {
        xSemaphoreTake(s_bridge.frame_lock, portMAX_DELAY);
        camera_uvc_csi_release_buffers(&s_bridge);
        s_bridge.csi = NULL;
        s_bridge.uvc = NULL;
        s_bridge.initialized = false;
        xSemaphoreGive(s_bridge.frame_lock);
    }
    xSemaphoreGive(s_bridge.lifecycle_lock);
    return ret;
}

bool camera_uvc_csi_is_streaming(void)
{
    return atomic_load(&s_bridge.streaming);
}

/* ==================== [Static Functions] ================================== */

static esp_err_t camera_uvc_csi_stream_start(uvc_format_t format, int width, int height,
                                           int rate, void *context)
{
    camera_uvc_csi_context_t *bridge = context;
    if (bridge == NULL || format != UVC_FORMAT_JPEG || width != CONFIG_UVC_CAM1_FRAMESIZE_WIDTH ||
            height != CONFIG_UVC_CAM1_FRAMESIZE_HEIGT || rate != CONFIG_UVC_CAM1_FRAMERATE) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    xSemaphoreTake(bridge->lifecycle_lock, portMAX_DELAY);
    esp_err_t ret = camera_uvc_csi_stop_source(bridge);
    if (ret != ESP_OK) {
        xSemaphoreGive(bridge->lifecycle_lock);
        return ret;
    }
    xSemaphoreTake(bridge->frame_lock, portMAX_DELAY);
    bridge->width = width;
    bridge->height = height;
    bridge->latest_length = 0;
    bridge->frame_error = ESP_OK;
    while (xSemaphoreTake(bridge->frame_ready, 0) == pdTRUE) {
    }
    xSemaphoreGive(bridge->frame_lock);
    const uint32_t input_formats[] = {
        V4L2_PIX_FMT_RGB565, V4L2_PIX_FMT_UYVY, V4L2_PIX_FMT_RGB24, V4L2_PIX_FMT_GREY,
    };
    ret = ESP_ERR_NOT_SUPPORTED;
    for (size_t i = 0; i < sizeof(input_formats) / sizeof(input_formats[0]); i++) {
        ret = wt_bsp_csi_set_pixel_format(bridge->csi, input_formats[i]);
        if (ret == ESP_OK) {
            // CSI owns capture setup and buffers; the application only consumes callbacks.
            ret = wt_bsp_csi_start(bridge->csi, camera_uvc_csi_consume_frame, bridge);
            bridge->source_started = ret == ESP_OK;
        }
        if (ret != ESP_OK) {
            continue;
        }
        bridge->pixel_format = input_formats[i];
        xSemaphoreTake(bridge->frame_lock, portMAX_DELAY);
        ret = camera_uvc_csi_open_codec(bridge);
        xSemaphoreGive(bridge->frame_lock);
        if (ret == ESP_OK) {
            break;
        }
        esp_err_t cleanup = camera_uvc_csi_stop_source(bridge);
        if (cleanup != ESP_OK) {
            xSemaphoreGive(bridge->lifecycle_lock);
            return cleanup;
        }
    }
    if (ret == ESP_OK) {
        while (xSemaphoreTake(bridge->worker_done, 0) == pdTRUE) {
        }
        atomic_store(&bridge->raw_busy, false);
        atomic_store(&bridge->raw_ready, false);
        bridge->worker_stop_requested = false;
        if (xTaskCreate(camera_uvc_csi_encode_task, "uvc_jpeg", 6144, bridge, 5,
                        &bridge->worker) != pdPASS) {
            bridge->worker = NULL;
            ret = ESP_ERR_NO_MEM;
        }
    }
    if (ret == ESP_OK) {
        atomic_store(&bridge->streaming, true);
    }
    if (ret == ESP_OK) {
        if (xSemaphoreTake(bridge->frame_ready, pdMS_TO_TICKS(CAMERA_UVC_CSI_TIMEOUT_MS)) != pdTRUE) {
            ret = ESP_ERR_TIMEOUT;
        } else {
            xSemaphoreTake(bridge->frame_lock, portMAX_DELAY);
            ret = atomic_load(&bridge->streaming) ? bridge->frame_error : ESP_ERR_NOT_SUPPORTED;
            xSemaphoreGive(bridge->frame_lock);
            xSemaphoreGive(bridge->frame_ready);
        }
    }
    if (ret != ESP_OK) {
        esp_err_t cleanup = camera_uvc_csi_stop_source(bridge);
        if (cleanup != ESP_OK) {
            ESP_LOGE(TAG, "CSI cleanup failed: %s", esp_err_to_name(cleanup));
        }
    }
    xSemaphoreGive(bridge->lifecycle_lock);
    return ret;
}

static void camera_uvc_csi_stream_stop(void *context)
{
    camera_uvc_csi_context_t *bridge = context;
    if (bridge == NULL) {
        return;
    }
    xSemaphoreTake(bridge->lifecycle_lock, portMAX_DELAY);
    esp_err_t ret = camera_uvc_csi_stop_source(bridge);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop application CSI source: %s", esp_err_to_name(ret));
    }
    xSemaphoreGive(bridge->lifecycle_lock);
}

static esp_err_t camera_uvc_csi_stop_source(camera_uvc_csi_context_t *bridge)
{
    atomic_store(&bridge->streaming, false);
    if (bridge->source_started) {
        esp_err_t ret = wt_bsp_csi_stop(bridge->csi);
        if (ret != ESP_OK) {
            return ret;
        }
        bridge->source_started = false;
    }
    if (bridge->worker != NULL) {
        if (!bridge->worker_stop_requested) {
            bridge->worker_stop_requested = true;
            xTaskNotify(bridge->worker, CAMERA_UVC_CSI_WORKER_STOP_BIT, eSetBits);
        }
        if (xSemaphoreTake(bridge->worker_done,
                            pdMS_TO_TICKS(CAMERA_UVC_CSI_WORKER_STOP_MS)) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
        bridge->worker = NULL;
    }
    xSemaphoreTake(bridge->frame_lock, portMAX_DELAY);
    camera_uvc_csi_close_codec(bridge);
    bridge->latest_length = 0;
    xSemaphoreGive(bridge->frame_lock);
    return ESP_OK;
}

static esp_err_t camera_uvc_csi_open_codec(camera_uvc_csi_context_t *bridge)
{
    bridge->codec_fd = open(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDONLY);
    if (bridge->codec_fd < 0) {
        return ESP_ERR_NOT_FOUND;
    }
    struct timeval timeout = {.tv_sec = CAMERA_UVC_CSI_TIMEOUT_MS / 1000};
    struct v4l2_ext_control quality = {
        .id = V4L2_CID_JPEG_COMPRESSION_QUALITY,
        .value = CAMERA_UVC_CSI_JPEG_QUALITY,
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CID_JPEG_CLASS,
        .count = 1,
        .controls = &quality,
    };
    if (ioctl(bridge->codec_fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) != 0) {
        goto fail;
    }
    if (ioctl(bridge->codec_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "Failed to set JPEG compression quality");
    }
    struct v4l2_format format = {
        .type = V4L2_BUF_TYPE_VIDEO_OUTPUT,
        .fmt.pix.width = bridge->width,
        .fmt.pix.height = bridge->height,
        .fmt.pix.pixelformat = bridge->pixel_format,
    };
    if (ioctl(bridge->codec_fd, VIDIOC_S_FMT, &format) != 0) {
        goto fail;
    }
    struct v4l2_requestbuffers buffers = {
        .count = 1,
        .type = V4L2_BUF_TYPE_VIDEO_OUTPUT,
        .memory = V4L2_MEMORY_USERPTR,
    };
    if (ioctl(bridge->codec_fd, VIDIOC_REQBUFS, &buffers) != 0) {
        goto fail;
    }
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    if (ioctl(bridge->codec_fd, VIDIOC_S_FMT, &format) != 0) {
        goto fail;
    }
    buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffers.memory = V4L2_MEMORY_MMAP;
    if (ioctl(bridge->codec_fd, VIDIOC_REQBUFS, &buffers) != 0) {
        goto fail;
    }
    struct v4l2_buffer buffer = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
        .index = 0,
    };
    if (ioctl(bridge->codec_fd, VIDIOC_QUERYBUF, &buffer) != 0) {
        goto fail;
    }
    bridge->codec_capture_buffer = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                                         MAP_SHARED, bridge->codec_fd, buffer.m.offset);
    if (bridge->codec_capture_buffer == MAP_FAILED) {
        bridge->codec_capture_buffer = NULL;
        goto fail;
    }
    if (ioctl(bridge->codec_fd, VIDIOC_QBUF, &buffer) != 0) {
        goto fail;
    }
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(bridge->codec_fd, VIDIOC_STREAMON, &type) != 0) {
        goto fail;
    }
    bridge->codec_capture_streaming = true;
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(bridge->codec_fd, VIDIOC_STREAMON, &type) != 0) {
        goto fail;
    }
    bridge->codec_output_streaming = true;
    return ESP_OK;

fail:
    camera_uvc_csi_close_codec(bridge);
    return ESP_FAIL;
}

static void camera_uvc_csi_close_codec(camera_uvc_csi_context_t *bridge)
{
    if (bridge->codec_fd >= 0) {
        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        if (bridge->codec_output_streaming) {
            ioctl(bridge->codec_fd, VIDIOC_STREAMOFF, &type);
        }
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (bridge->codec_capture_streaming) {
            ioctl(bridge->codec_fd, VIDIOC_STREAMOFF, &type);
        }
        close(bridge->codec_fd);
        bridge->codec_fd = -1;
    }
    bridge->codec_capture_buffer = NULL;
    bridge->codec_output_streaming = false;
    bridge->codec_capture_streaming = false;
}

static void camera_uvc_csi_consume_frame(uint8_t *buffer, uint32_t width, uint32_t height,
                                        size_t length, void *context)
{
    camera_uvc_csi_context_t *bridge = context;
    if (bridge == NULL || !atomic_load(&bridge->streaming)) {
        return;
    }
    if (buffer == NULL || width != bridge->width || height != bridge->height ||
            length == 0 || length > bridge->raw_capacity) {
        atomic_store(&bridge->streaming, false);
        xSemaphoreGive(bridge->frame_ready);
        return;
    }
    bool expected = false;
    if (!atomic_compare_exchange_strong(&bridge->raw_busy, &expected, true)) {
        return; // The encoder is busy; keep capture callbacks bounded by dropping this frame.
    }
    memcpy(bridge->raw_buffer, buffer, length);
    bridge->raw_length = length;
    atomic_store_explicit(&bridge->raw_ready, true, memory_order_release);
    xTaskNotify(bridge->worker, CAMERA_UVC_CSI_FRAME_READY_BIT, eSetBits);
}

static void camera_uvc_csi_encode_task(void *context)
{
    camera_uvc_csi_context_t *bridge = context;
    for (;;) {
        uint32_t notifications = 0;
        xTaskNotifyWait(0, UINT32_MAX, &notifications, portMAX_DELAY);
        if (notifications & CAMERA_UVC_CSI_WORKER_STOP_BIT) {
            break;
        }
        if (!atomic_exchange_explicit(&bridge->raw_ready, false, memory_order_acquire)) {
            continue;
        }
        if (!atomic_load(&bridge->streaming)) {
            atomic_store(&bridge->raw_busy, false);
            continue;
        }
        xSemaphoreTake(bridge->frame_lock, portMAX_DELAY);
        bridge->frame_error = ESP_FAIL;
        struct v4l2_buffer input = {
            .type = V4L2_BUF_TYPE_VIDEO_OUTPUT,
            .memory = V4L2_MEMORY_USERPTR,
            .index = 0,
            .m.userptr = (unsigned long)bridge->raw_buffer,
            .length = bridge->raw_length,
        };
        struct v4l2_buffer output = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };
        if (ioctl(bridge->codec_fd, VIDIOC_QBUF, &input) != 0 ||
                ioctl(bridge->codec_fd, VIDIOC_DQBUF, &output) != 0 ||
                ioctl(bridge->codec_fd, VIDIOC_DQBUF, &input) != 0) {
            ESP_LOGE(TAG, "Failed to encode CSI frame (errno=%d)", errno);
            camera_uvc_csi_close_codec(bridge);
            atomic_store(&bridge->streaming, false);
        } else {
            if (output.bytesused == 0 || output.bytesused > bridge->capacity) {
                bridge->frame_error = ESP_ERR_INVALID_SIZE;
            } else {
                memcpy(bridge->latest_jpeg, bridge->codec_capture_buffer, output.bytesused);
                bridge->latest_length = output.bytesused;
                bridge->latest_timestamp_us = esp_timer_get_time();
                bridge->frame_error = ESP_OK;
            }
            if (ioctl(bridge->codec_fd, VIDIOC_QBUF, &output) != 0) {
                bridge->frame_error = ESP_FAIL;
                camera_uvc_csi_close_codec(bridge);
                atomic_store(&bridge->streaming, false);
            }
        }
        atomic_store(&bridge->raw_busy, false);
        xSemaphoreGive(bridge->frame_ready);
        xSemaphoreGive(bridge->frame_lock);
    }
    atomic_store(&bridge->raw_busy, false);
    xSemaphoreGive(bridge->worker_done);
    vTaskDelete(NULL);
}

static uvc_fb_t *camera_uvc_csi_get_frame(void *context)
{
    camera_uvc_csi_context_t *bridge = context;
    if (bridge == NULL || !atomic_load(&bridge->streaming) ||
            xSemaphoreTake(bridge->frame_ready, pdMS_TO_TICKS(CAMERA_UVC_CSI_TIMEOUT_MS)) != pdTRUE) {
        return NULL;
    }
    xSemaphoreTake(bridge->frame_lock, portMAX_DELAY);
    if (bridge->frame_error != ESP_OK || bridge->latest_length == 0 ||
            !atomic_load(&bridge->streaming)) {
        xSemaphoreGive(bridge->frame_lock);
        return NULL;
    }
    memcpy(bridge->frame_buffer, bridge->latest_jpeg, bridge->latest_length);
    bridge->frame.buf = bridge->frame_buffer;
    bridge->frame.len = bridge->latest_length;
    bridge->frame.width = bridge->width;
    bridge->frame.height = bridge->height;
    bridge->frame.format = UVC_FORMAT_JPEG;
    bridge->frame.timestamp.tv_sec = bridge->latest_timestamp_us / 1000000;
    bridge->frame.timestamp.tv_usec = bridge->latest_timestamp_us % 1000000;
    xSemaphoreGive(bridge->frame_lock);
    return &bridge->frame;
}

static void camera_uvc_csi_return_frame(uvc_fb_t *frame, void *context)
{
    // The USB snapshot buffer remains unchanged until the next get-frame call.
    (void)frame;
    (void)context;
}

static void camera_uvc_csi_release_buffers(camera_uvc_csi_context_t *bridge)
{
    free(bridge->raw_buffer);
    free(bridge->usb_buffer);
    free(bridge->latest_jpeg);
    free(bridge->frame_buffer);
    bridge->raw_buffer = NULL;
    bridge->raw_capacity = 0;
    bridge->raw_length = 0;
    bridge->usb_buffer = NULL;
    bridge->latest_jpeg = NULL;
    bridge->frame_buffer = NULL;
    bridge->capacity = 0;
    bridge->latest_length = 0;
}

#endif // WT_BSP_USB_DEVICE_UVC_ENABLED && WT_BSP_CSI_ENABLED
