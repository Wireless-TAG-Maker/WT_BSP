/**
 * @file board_usb_device_uvc.c
 * @author Wireless-Tag
 * @brief ESP32-P4 CSI 摄像头到 USB Device UVC 的共享板级实现。
 * @version 0.1
 * @date 2026-07-28
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "board_usb_device_uvc.h"

#include "sdkconfig.h"

#if CONFIG_WT_BSP_ENABLE_USB_DEVICE_UVC && CONFIG_WT_BSP_ENABLE_CSI

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_video_device.h"
#include "esp_video_ioctl.h"
#include "linux/videodev2.h"
#include "usb_device_uvc.h"
#if defined(CONFIG_WT_BSP_BOARD_WT9932P4X_TINY) && CONFIG_WT_BSP_BOARD_WT9932P4X_TINY
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#endif

/* ==================== [Defines] =========================================== */

#define BOARD_USB_UVC_BUFFER_COUNT 2
#define BOARD_USB_UVC_DQBUF_TIMEOUT_MS 2000
#define BOARD_USB_UVC_JPEG_QUALITY 80
#define BOARD_USB_UVC_INTERFACE_STRING_INDEX 4

#if defined(CONFIG_WT_BSP_BOARD_WT9932P4X_TINY) && CONFIG_WT_BSP_BOARD_WT9932P4X_TINY
#define BOARD_USB_UVC_STATUS_LED_MAX_BRIGHTNESS 5
#define BOARD_USB_UVC_STATUS_LED_STREAMING_BRIGHTNESS 1
#define BOARD_USB_UVC_STATUS_LED_POLL_MS 50
#define BOARD_USB_UVC_STATUS_LED_BLINK_MS 500
#define BOARD_USB_UVC_STATUS_LED_BREATH_UPDATE_MS 2
#define BOARD_USB_UVC_STATUS_LED_BREATH_INHALE_MS 1500
#define BOARD_USB_UVC_STATUS_LED_BREATH_PEAK_MS 200
#define BOARD_USB_UVC_STATUS_LED_BREATH_EXHALE_MS 2100
#define BOARD_USB_UVC_STATUS_LED_BREATH_REST_MS 1000
#define BOARD_USB_UVC_STATUS_LED_BREATH_CYCLE_MS (BOARD_USB_UVC_STATUS_LED_BREATH_INHALE_MS + \
                                                   BOARD_USB_UVC_STATUS_LED_BREATH_PEAK_MS + \
                                                   BOARD_USB_UVC_STATUS_LED_BREATH_EXHALE_MS + \
                                                   BOARD_USB_UVC_STATUS_LED_BREATH_REST_MS)
#define BOARD_USB_UVC_STATUS_LED_TASK_STACK_SIZE 2048
#define BOARD_USB_UVC_STATUS_LED_TASK_PRIORITY 2
#endif

/* ==================== [Typedefs] ========================================== */

typedef struct {
    int capture_fd;
    int codec_fd;
    uint8_t *capture_buffer[BOARD_USB_UVC_BUFFER_COUNT];
    uint8_t *codec_capture_buffer;
    uint8_t *uvc_buffer;
    uvc_fb_t frame;
    uint32_t frame_width;
    uint32_t frame_height;
    bool capture_streaming;
    bool codec_output_streaming;
    bool codec_capture_streaming;
    bool initialized;
} board_usb_uvc_t;

#if defined(CONFIG_WT_BSP_BOARD_WT9932P4X_TINY) && CONFIG_WT_BSP_BOARD_WT9932P4X_TINY
typedef enum {
    BOARD_USB_UVC_STATUS_LED_IDLE = 1,
    BOARD_USB_UVC_STATUS_LED_STREAMING,
    BOARD_USB_UVC_STATUS_LED_STOP,
} board_usb_uvc_status_led_mode_t;

typedef enum {
    BOARD_USB_UVC_LED_WAITING_FOR_HOST = 0,
    BOARD_USB_UVC_LED_MOUNTED,
    BOARD_USB_UVC_LED_STREAMING,
} board_usb_uvc_led_state_t;
#endif

/* ==================== [Static Prototypes] ================================= */

static esp_err_t board_usb_uvc_open_devices(board_usb_uvc_t *uvc);
static esp_err_t board_usb_uvc_set_dqbuf_timeout(int fd, const char *device_name);
static esp_err_t board_usb_uvc_start_cb(uvc_format_t format, int width, int height, int rate, void *cb_ctx);
static void board_usb_uvc_stop_cb(void *cb_ctx);
static uvc_fb_t *board_usb_uvc_frame_get_cb(void *cb_ctx);
static void board_usb_uvc_frame_return_cb(uvc_fb_t *frame, void *cb_ctx);
static void board_usb_uvc_close_devices(board_usb_uvc_t *uvc);
#if defined(CONFIG_WT_BSP_BOARD_WT9932P4X_TINY) && CONFIG_WT_BSP_BOARD_WT9932P4X_TINY
static void board_usb_uvc_status_led_task(void *arg);
static void board_usb_uvc_status_led_notify(board_usb_uvc_status_led_mode_t mode);
static void board_usb_uvc_status_led_stop(void);
static uint16_t board_usb_uvc_status_led_breath_brightness_q8(uint32_t phase_ms);
static uint32_t board_usb_uvc_status_led_smoothstep_q16(uint32_t elapsed_ms, uint32_t duration_ms);
#endif

/* ==================== [Static Variables] ================================== */

static const char *TAG = "board_usb_uvc";

static board_usb_uvc_t s_usb_uvc = {
    .capture_fd = -1,
    .codec_fd = -1,
};

#if defined(CONFIG_WT_BSP_BOARD_WT9932P4X_TINY) && CONFIG_WT_BSP_BOARD_WT9932P4X_TINY
static TaskHandle_t s_status_led_task;
#endif

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t board_usb_device_uvc_init(void)
{
    esp_err_t ret = ESP_OK;

    if (s_usb_uvc.initialized) {
        ESP_LOGW(TAG, "USB Device UVC is already initialized");
        return ESP_OK;
    }

    ret = board_usb_uvc_open_devices(&s_usb_uvc);
    if (ret != ESP_OK) {
        board_usb_uvc_close_devices(&s_usb_uvc);
        return ret;
    }

    size_t uvc_buffer_size =
        (size_t)CONFIG_UVC_CAM1_FRAMESIZE_WIDTH * CONFIG_UVC_CAM1_FRAMESIZE_HEIGT;
    s_usb_uvc.uvc_buffer = malloc(uvc_buffer_size);
    if (s_usb_uvc.uvc_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate USB UVC buffer: %u bytes", (unsigned int)uvc_buffer_size);
        board_usb_uvc_close_devices(&s_usb_uvc);
        return ESP_ERR_NO_MEM;
    }

    uvc_device_config_t config = {
        .uvc_buffer = s_usb_uvc.uvc_buffer,
        .uvc_buffer_size = uvc_buffer_size,
        .start_cb = board_usb_uvc_start_cb,
        .fb_get_cb = board_usb_uvc_frame_get_cb,
        .fb_return_cb = board_usb_uvc_frame_return_cb,
        .stop_cb = board_usb_uvc_stop_cb,
        .cb_ctx = &s_usb_uvc,
    };

    ret = uvc_device_config(0, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure USB Device UVC: %s", esp_err_to_name(ret));
        board_usb_uvc_close_devices(&s_usb_uvc);
        return ret;
    }

    // usb_device_uvc uses string descriptor index 4 as the camera name shown by Windows.
    extern const char *string_desc_arr[];
    string_desc_arr[BOARD_USB_UVC_INTERFACE_STRING_INDEX] = CONFIG_TUSB_PRODUCT;

    ret = uvc_device_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize USB Device UVC: %s", esp_err_to_name(ret));
        board_usb_uvc_close_devices(&s_usb_uvc);
        return ret;
    }

    s_usb_uvc.initialized = true;
    ESP_LOGI(TAG,
             "USB Device UVC ready: MJPEG %dx%d@%dfps",
             CONFIG_UVC_CAM1_FRAMESIZE_WIDTH,
             CONFIG_UVC_CAM1_FRAMESIZE_HEIGT,
             CONFIG_UVC_CAM1_FRAMERATE);

    return ESP_OK;
}

esp_err_t board_usb_device_uvc_deinit(void)
{
    esp_err_t ret = ESP_OK;

#if defined(CONFIG_WT_BSP_BOARD_WT9932P4X_TINY) && CONFIG_WT_BSP_BOARD_WT9932P4X_TINY
    board_usb_uvc_status_led_stop();
#endif

    if (!s_usb_uvc.initialized) {
        return ESP_OK;
    }

    ret = uvc_device_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize USB Device UVC: %s", esp_err_to_name(ret));
    }

    board_usb_uvc_close_devices(&s_usb_uvc);
    s_usb_uvc.initialized = false;

    return ret;
}

#if defined(CONFIG_WT_BSP_BOARD_WT9932P4X_TINY) && CONFIG_WT_BSP_BOARD_WT9932P4X_TINY
esp_err_t board_usb_device_uvc_status_led_start(wt_bsp_rgb_t rgb)
{
    if (rgb == NULL) {
        ESP_LOGE(TAG, "P4X status LED object is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (s_status_led_task != NULL) {
        ESP_LOGW(TAG, "P4X status LED task is already running");
        return ESP_OK;
    }

    BaseType_t result = xTaskCreate(board_usb_uvc_status_led_task,
                                    "p4x_uvc_led",
                                    BOARD_USB_UVC_STATUS_LED_TASK_STACK_SIZE,
                                    rgb,
                                    BOARD_USB_UVC_STATUS_LED_TASK_PRIORITY,
                                    &s_status_led_task);
    if (result != pdPASS) {
        s_status_led_task = NULL;
        ESP_LOGE(TAG, "Failed to create P4X status LED task");
        return ESP_ERR_NO_MEM;
    }

    board_usb_uvc_status_led_notify(s_usb_uvc.capture_streaming ?
                                    BOARD_USB_UVC_STATUS_LED_STREAMING :
                                    BOARD_USB_UVC_STATUS_LED_IDLE);
    ESP_LOGI(TAG, "P4X status LED ready: blink=%d, breathe=0..%d/4.8s, solid=%d",
             BOARD_USB_UVC_STATUS_LED_MAX_BRIGHTNESS,
             BOARD_USB_UVC_STATUS_LED_MAX_BRIGHTNESS,
             BOARD_USB_UVC_STATUS_LED_STREAMING_BRIGHTNESS);
    return ESP_OK;
}
#endif

/* ==================== [Static Functions] ================================== */

static esp_err_t board_usb_uvc_open_devices(board_usb_uvc_t *uvc)
{
    esp_err_t ret = ESP_OK;
    struct v4l2_ext_control control = {
        .id = V4L2_CID_JPEG_COMPRESSION_QUALITY,
        .value = BOARD_USB_UVC_JPEG_QUALITY,
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CID_JPEG_CLASS,
        .count = 1,
        .controls = &control,
    };

    uvc->capture_fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
    if (uvc->capture_fd < 0) {
        ESP_LOGE(TAG, "Failed to open CSI device %s", ESP_VIDEO_MIPI_CSI_DEVICE_NAME);
        return ESP_ERR_NOT_FOUND;
    }

    uvc->codec_fd = open(ESP_VIDEO_JPEG_DEVICE_NAME, O_RDONLY);
    if (uvc->codec_fd < 0) {
        ESP_LOGE(TAG, "Failed to open JPEG device %s", ESP_VIDEO_JPEG_DEVICE_NAME);
        return ESP_ERR_NOT_FOUND;
    }

    ret = board_usb_uvc_set_dqbuf_timeout(uvc->capture_fd, "CSI");
    if (ret != ESP_OK) {
        return ret;
    }

    ret = board_usb_uvc_set_dqbuf_timeout(uvc->codec_fd, "JPEG");
    if (ret != ESP_OK) {
        return ret;
    }

    if (ioctl(uvc->codec_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "Failed to set JPEG compression quality");
    }

    return ESP_OK;
}

static esp_err_t board_usb_uvc_set_dqbuf_timeout(int fd, const char *device_name)
{
    struct timeval timeout = {
        .tv_sec = BOARD_USB_UVC_DQBUF_TIMEOUT_MS / 1000,
        .tv_usec = (BOARD_USB_UVC_DQBUF_TIMEOUT_MS % 1000) * 1000,
    };

    if (ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) != 0) {
        ESP_LOGE(TAG, "Failed to set %s frame dequeue timeout", device_name);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t board_usb_uvc_start_cb(uvc_format_t format, int width, int height, int rate, void *cb_ctx)
{
    board_usb_uvc_t *uvc = (board_usb_uvc_t *)cb_ctx;
    struct v4l2_buffer buffer;
    struct v4l2_requestbuffers request;
    struct v4l2_format video_format;
    uint32_t capture_format = 0;
    int type;

    if (uvc == NULL || format != UVC_FORMAT_JPEG) {
        ESP_LOGE(TAG, "Unsupported UVC stream format");
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (width != CONFIG_UVC_CAM1_FRAMESIZE_WIDTH ||
            height != CONFIG_UVC_CAM1_FRAMESIZE_HEIGT ||
            rate != CONFIG_UVC_CAM1_FRAMERATE) {
        ESP_LOGE(TAG, "Unsupported UVC stream mode: %dx%d@%dfps", width, height, rate);
        return ESP_ERR_NOT_SUPPORTED;
    }

    const uint32_t jpeg_input_formats[] = {
        V4L2_PIX_FMT_RGB565,
        V4L2_PIX_FMT_UYVY,
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_GREY,
    };

    for (uint32_t index = 0; capture_format == 0; index++) {
        struct v4l2_fmtdesc format_desc = {
            .index = index,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        };

        if (ioctl(uvc->capture_fd, VIDIOC_ENUM_FMT, &format_desc) != 0) {
            break;
        }

        for (size_t i = 0; i < sizeof(jpeg_input_formats) / sizeof(jpeg_input_formats[0]); i++) {
            if (jpeg_input_formats[i] == format_desc.pixelformat) {
                capture_format = jpeg_input_formats[i];
                break;
            }
        }
    }

    if (capture_format == 0) {
        ESP_LOGE(TAG, "CSI output format is not supported by the JPEG encoder");
        return ESP_ERR_NOT_SUPPORTED;
    }

    memset(&video_format, 0, sizeof(video_format));
    video_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_format.fmt.pix.width = width;
    video_format.fmt.pix.height = height;
    video_format.fmt.pix.pixelformat = capture_format;
    if (ioctl(uvc->capture_fd, VIDIOC_S_FMT, &video_format) != 0) {
        ESP_LOGE(TAG, "Failed to configure CSI capture format");
        return ESP_FAIL;
    }
    capture_format = video_format.fmt.pix.pixelformat;
    ESP_LOGI(TAG,
             "CSI/JPEG input format: %c%c%c%c",
             (char)(capture_format & 0xff),
             (char)((capture_format >> 8) & 0xff),
             (char)((capture_format >> 16) & 0xff),
             (char)((capture_format >> 24) & 0xff));

    memset(&request, 0, sizeof(request));
    request.count = BOARD_USB_UVC_BUFFER_COUNT;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;
    if (ioctl(uvc->capture_fd, VIDIOC_REQBUFS, &request) != 0) {
        ESP_LOGE(TAG, "Failed to request CSI buffers");
        return ESP_FAIL;
    }

    for (uint32_t i = 0; i < BOARD_USB_UVC_BUFFER_COUNT; i++) {
        memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        if (ioctl(uvc->capture_fd, VIDIOC_QUERYBUF, &buffer) != 0) {
            ESP_LOGE(TAG, "Failed to query CSI buffer");
            return ESP_FAIL;
        }

        uvc->capture_buffer[i] = mmap(NULL,
                                      buffer.length,
                                      PROT_READ | PROT_WRITE,
                                      MAP_SHARED,
                                      uvc->capture_fd,
                                      buffer.m.offset);
        if (uvc->capture_buffer[i] == MAP_FAILED) {
            uvc->capture_buffer[i] = NULL;
            ESP_LOGE(TAG, "Failed to map CSI buffer");
            return ESP_FAIL;
        }

        if (ioctl(uvc->capture_fd, VIDIOC_QBUF, &buffer) != 0) {
            ESP_LOGE(TAG, "Failed to queue CSI buffer");
            return ESP_FAIL;
        }
    }

    memset(&video_format, 0, sizeof(video_format));
    video_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    video_format.fmt.pix.width = width;
    video_format.fmt.pix.height = height;
    video_format.fmt.pix.pixelformat = capture_format;
    if (ioctl(uvc->codec_fd, VIDIOC_S_FMT, &video_format) != 0) {
        ESP_LOGE(TAG, "Failed to configure JPEG input format");
        return ESP_FAIL;
    }

    memset(&request, 0, sizeof(request));
    request.count = 1;
    request.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    request.memory = V4L2_MEMORY_USERPTR;
    if (ioctl(uvc->codec_fd, VIDIOC_REQBUFS, &request) != 0) {
        ESP_LOGE(TAG, "Failed to request JPEG input buffer");
        return ESP_FAIL;
    }

    memset(&video_format, 0, sizeof(video_format));
    video_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    video_format.fmt.pix.width = width;
    video_format.fmt.pix.height = height;
    video_format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
    if (ioctl(uvc->codec_fd, VIDIOC_S_FMT, &video_format) != 0) {
        ESP_LOGE(TAG, "Failed to configure JPEG output format");
        return ESP_FAIL;
    }

    memset(&request, 0, sizeof(request));
    request.count = 1;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;
    if (ioctl(uvc->codec_fd, VIDIOC_REQBUFS, &request) != 0) {
        ESP_LOGE(TAG, "Failed to request JPEG output buffer");
        return ESP_FAIL;
    }

    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = 0;
    if (ioctl(uvc->codec_fd, VIDIOC_QUERYBUF, &buffer) != 0) {
        ESP_LOGE(TAG, "Failed to query JPEG output buffer");
        return ESP_FAIL;
    }

    uvc->codec_capture_buffer = mmap(NULL,
                                     buffer.length,
                                     PROT_READ | PROT_WRITE,
                                     MAP_SHARED,
                                     uvc->codec_fd,
                                     buffer.m.offset);
    if (uvc->codec_capture_buffer == MAP_FAILED) {
        uvc->codec_capture_buffer = NULL;
        ESP_LOGE(TAG, "Failed to map JPEG output buffer");
        return ESP_FAIL;
    }

    if (ioctl(uvc->codec_fd, VIDIOC_QBUF, &buffer) != 0) {
        ESP_LOGE(TAG, "Failed to queue JPEG output buffer");
        return ESP_FAIL;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(uvc->codec_fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "Failed to start JPEG output stream");
        return ESP_FAIL;
    }
    uvc->codec_capture_streaming = true;

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl(uvc->codec_fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "Failed to start JPEG input stream");
        board_usb_uvc_stop_cb(uvc);
        return ESP_FAIL;
    }
    uvc->codec_output_streaming = true;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(uvc->capture_fd, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "Failed to start CSI stream");
        board_usb_uvc_stop_cb(uvc);
        return ESP_FAIL;
    }
    uvc->capture_streaming = true;
    uvc->frame_width = width;
    uvc->frame_height = height;

#if defined(CONFIG_WT_BSP_BOARD_WT9932P4X_TINY) && CONFIG_WT_BSP_BOARD_WT9932P4X_TINY
    board_usb_uvc_status_led_notify(BOARD_USB_UVC_STATUS_LED_STREAMING);
#endif

    ESP_LOGI(TAG, "UVC streaming started: %dx%d@%dfps", width, height, rate);
    return ESP_OK;
}

static void board_usb_uvc_stop_cb(void *cb_ctx)
{
    board_usb_uvc_t *uvc = (board_usb_uvc_t *)cb_ctx;
    int type;

    if (uvc == NULL) {
        return;
    }

    if (uvc->capture_streaming) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(uvc->capture_fd, VIDIOC_STREAMOFF, &type);
        uvc->capture_streaming = false;
    }

    if (uvc->codec_output_streaming) {
        type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        ioctl(uvc->codec_fd, VIDIOC_STREAMOFF, &type);
        uvc->codec_output_streaming = false;
    }

    if (uvc->codec_capture_streaming) {
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(uvc->codec_fd, VIDIOC_STREAMOFF, &type);
        uvc->codec_capture_streaming = false;
    }

    uvc->frame_width = 0;
    uvc->frame_height = 0;
#if defined(CONFIG_WT_BSP_BOARD_WT9932P4X_TINY) && CONFIG_WT_BSP_BOARD_WT9932P4X_TINY
    board_usb_uvc_status_led_notify(BOARD_USB_UVC_STATUS_LED_IDLE);
#endif
}

static uvc_fb_t *board_usb_uvc_frame_get_cb(void *cb_ctx)
{
    board_usb_uvc_t *uvc = (board_usb_uvc_t *)cb_ctx;
    struct v4l2_buffer capture_buffer;
    struct v4l2_buffer codec_input_buffer;
    struct v4l2_buffer codec_output_buffer;
    int64_t timestamp_us;

    if (uvc == NULL || !uvc->capture_streaming) {
        return NULL;
    }

    memset(&capture_buffer, 0, sizeof(capture_buffer));
    capture_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    capture_buffer.memory = V4L2_MEMORY_MMAP;
    if (ioctl(uvc->capture_fd, VIDIOC_DQBUF, &capture_buffer) != 0) {
        ESP_LOGE(TAG,
                 "CSI frame dequeue failed or timed out after %d ms",
                 BOARD_USB_UVC_DQBUF_TIMEOUT_MS);
        return NULL;
    }

    if (capture_buffer.index >= BOARD_USB_UVC_BUFFER_COUNT) {
        ESP_LOGE(TAG, "CSI returned an invalid buffer index");
        board_usb_uvc_stop_cb(uvc);
        return NULL;
    }

    memset(&codec_input_buffer, 0, sizeof(codec_input_buffer));
    codec_input_buffer.index = 0;
    codec_input_buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    codec_input_buffer.memory = V4L2_MEMORY_USERPTR;
    codec_input_buffer.m.userptr = (unsigned long)uvc->capture_buffer[capture_buffer.index];
    codec_input_buffer.length = capture_buffer.bytesused;
    if (ioctl(uvc->codec_fd, VIDIOC_QBUF, &codec_input_buffer) != 0) {
        ioctl(uvc->capture_fd, VIDIOC_QBUF, &capture_buffer);
        ESP_LOGE(TAG, "Failed to queue JPEG input frame");
        return NULL;
    }

    memset(&codec_output_buffer, 0, sizeof(codec_output_buffer));
    codec_output_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    codec_output_buffer.memory = V4L2_MEMORY_MMAP;
    if (ioctl(uvc->codec_fd, VIDIOC_DQBUF, &codec_output_buffer) != 0) {
        ESP_LOGE(TAG,
                 "JPEG frame dequeue failed or timed out after %d ms",
                 BOARD_USB_UVC_DQBUF_TIMEOUT_MS);
        board_usb_uvc_stop_cb(uvc);
        return NULL;
    }

    if (ioctl(uvc->capture_fd, VIDIOC_QBUF, &capture_buffer) != 0) {
        ESP_LOGE(TAG, "Failed to recycle CSI input buffer");
        board_usb_uvc_stop_cb(uvc);
        return NULL;
    }

    if (ioctl(uvc->codec_fd, VIDIOC_DQBUF, &codec_input_buffer) != 0) {
        ESP_LOGE(TAG, "Failed to recycle JPEG input buffer");
        board_usb_uvc_stop_cb(uvc);
        return NULL;
    }

    if (codec_output_buffer.bytesused == 0) {
        board_usb_uvc_frame_return_cb(NULL, uvc);
        ESP_LOGW(TAG, "JPEG encoder returned an empty frame");
        return NULL;
    }

    timestamp_us = esp_timer_get_time();
    uvc->frame.buf = uvc->codec_capture_buffer;
    uvc->frame.len = codec_output_buffer.bytesused;
    uvc->frame.width = uvc->frame_width;
    uvc->frame.height = uvc->frame_height;
    uvc->frame.format = UVC_FORMAT_JPEG;
    uvc->frame.timestamp.tv_sec = timestamp_us / 1000000;
    uvc->frame.timestamp.tv_usec = timestamp_us % 1000000;

    return &uvc->frame;
}

static void board_usb_uvc_frame_return_cb(uvc_fb_t *frame, void *cb_ctx)
{
    board_usb_uvc_t *uvc = (board_usb_uvc_t *)cb_ctx;
    struct v4l2_buffer codec_output_buffer;

    (void)frame;

    if (uvc == NULL || !uvc->codec_capture_streaming) {
        return;
    }

    memset(&codec_output_buffer, 0, sizeof(codec_output_buffer));
    codec_output_buffer.index = 0;
    codec_output_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    codec_output_buffer.memory = V4L2_MEMORY_MMAP;
    if (ioctl(uvc->codec_fd, VIDIOC_QBUF, &codec_output_buffer) != 0) {
        ESP_LOGE(TAG, "Failed to recycle JPEG output buffer");
    }
}

#if defined(CONFIG_WT_BSP_BOARD_WT9932P4X_TINY) && CONFIG_WT_BSP_BOARD_WT9932P4X_TINY
static void board_usb_uvc_status_led_task(void *arg)
{
    wt_bsp_rgb_t rgb = (wt_bsp_rgb_t)arg;
    board_usb_uvc_led_state_t state = BOARD_USB_UVC_LED_WAITING_FOR_HOST;
    board_usb_uvc_led_state_t previous_state = (board_usb_uvc_led_state_t)-1;
    bool streaming = s_usb_uvc.capture_streaming;
    bool blink_on = true;
    uint16_t breath_dither_accumulator = 0;
    int last_brightness = -1;
    TickType_t last_blink = xTaskGetTickCount();
    TickType_t breath_cycle_start = last_blink;

    while (true) {
        uint32_t notification = 0;
        TickType_t wait_ticks = pdMS_TO_TICKS(previous_state == BOARD_USB_UVC_LED_MOUNTED ?
                                              BOARD_USB_UVC_STATUS_LED_BREATH_UPDATE_MS :
                                              BOARD_USB_UVC_STATUS_LED_POLL_MS);
        if (xTaskNotifyWait(0, UINT32_MAX, &notification,
                            wait_ticks) == pdTRUE) {
            if (notification == BOARD_USB_UVC_STATUS_LED_STOP) {
                break;
            }
            streaming = notification == BOARD_USB_UVC_STATUS_LED_STREAMING;
        }

        if (streaming) {
            state = BOARD_USB_UVC_LED_STREAMING;
        } else if (tud_mounted()) {
            state = BOARD_USB_UVC_LED_MOUNTED;
        } else {
            state = BOARD_USB_UVC_LED_WAITING_FOR_HOST;
        }

        TickType_t now = xTaskGetTickCount();
        if (state != previous_state) {
            previous_state = state;
            last_brightness = -1;
            blink_on = true;
            breath_dither_accumulator = 0;
            last_blink = now;
            breath_cycle_start = now;
            ESP_LOGI(TAG, "P4X status LED -> %s",
                     state == BOARD_USB_UVC_LED_STREAMING ? "UVC streaming (green solid 1)" :
                     state == BOARD_USB_UVC_LED_MOUNTED ? "USB mounted (natural green breathe 0..5, 4.8 s cycle)" :
                     "waiting for USB host (green blink 5)");
        }

        uint8_t brightness;
        if (state == BOARD_USB_UVC_LED_STREAMING) {
            brightness = BOARD_USB_UVC_STATUS_LED_STREAMING_BRIGHTNESS;
        } else if (state == BOARD_USB_UVC_LED_MOUNTED) {
            uint32_t phase_ms = pdTICKS_TO_MS(now - breath_cycle_start) %
                                BOARD_USB_UVC_STATUS_LED_BREATH_CYCLE_MS;
            uint16_t brightness_q8 = board_usb_uvc_status_led_breath_brightness_q8(phase_ms);
            brightness = brightness_q8 >> 8;
            breath_dither_accumulator += brightness_q8 & 0xff;
            if (breath_dither_accumulator >= 256) {
                brightness++;
                breath_dither_accumulator -= 256;
            }
            if (brightness > BOARD_USB_UVC_STATUS_LED_MAX_BRIGHTNESS) {
                brightness = BOARD_USB_UVC_STATUS_LED_MAX_BRIGHTNESS;
            }
        } else {
            if (now - last_blink >= pdMS_TO_TICKS(BOARD_USB_UVC_STATUS_LED_BLINK_MS)) {
                blink_on = !blink_on;
                last_blink = now;
            }
            brightness = blink_on ? BOARD_USB_UVC_STATUS_LED_MAX_BRIGHTNESS : 0;
        }

        if (brightness != last_brightness) {
            esp_err_t ret = wt_bsp_rgb_set_color(rgb, (wt_bsp_rgb_color_t) {
                .r = 0,
                .g = brightness,
                .b = 0,
            });
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to update P4X status LED: %s", esp_err_to_name(ret));
                break;
            }
            last_brightness = brightness;
        }
    }

    wt_bsp_rgb_clear(rgb);
    s_status_led_task = NULL;
    vTaskDelete(NULL);
}

static uint16_t board_usb_uvc_status_led_breath_brightness_q8(uint32_t phase_ms)
{
    uint32_t curve_q16;

    if (phase_ms < BOARD_USB_UVC_STATUS_LED_BREATH_INHALE_MS) {
        curve_q16 = board_usb_uvc_status_led_smoothstep_q16(
                        phase_ms, BOARD_USB_UVC_STATUS_LED_BREATH_INHALE_MS);
    } else if (phase_ms < BOARD_USB_UVC_STATUS_LED_BREATH_INHALE_MS +
                          BOARD_USB_UVC_STATUS_LED_BREATH_PEAK_MS) {
        curve_q16 = UINT16_MAX;
    } else if (phase_ms < BOARD_USB_UVC_STATUS_LED_BREATH_INHALE_MS +
                          BOARD_USB_UVC_STATUS_LED_BREATH_PEAK_MS +
                          BOARD_USB_UVC_STATUS_LED_BREATH_EXHALE_MS) {
        uint32_t exhale_ms = phase_ms - BOARD_USB_UVC_STATUS_LED_BREATH_INHALE_MS -
                             BOARD_USB_UVC_STATUS_LED_BREATH_PEAK_MS;
        curve_q16 = UINT16_MAX - board_usb_uvc_status_led_smoothstep_q16(
                                      exhale_ms, BOARD_USB_UVC_STATUS_LED_BREATH_EXHALE_MS);
    } else {
        curve_q16 = 0;
    }

    return (uint16_t)(((uint32_t)BOARD_USB_UVC_STATUS_LED_MAX_BRIGHTNESS * 256U * curve_q16 +
                       (UINT16_MAX / 2)) / UINT16_MAX);
}

static uint32_t board_usb_uvc_status_led_smoothstep_q16(uint32_t elapsed_ms, uint32_t duration_ms)
{
    uint32_t x = (uint32_t)(((uint64_t)elapsed_ms * UINT16_MAX) / duration_ms);
    uint32_t x_squared = (uint32_t)(((uint64_t)x * x) / UINT16_MAX);
    return (uint32_t)(((uint64_t)x_squared * (3U * UINT16_MAX - 2U * x)) / UINT16_MAX);
}

static void board_usb_uvc_status_led_notify(board_usb_uvc_status_led_mode_t mode)
{
    if (s_status_led_task != NULL) {
        xTaskNotify(s_status_led_task, mode, eSetValueWithOverwrite);
    }
}

static void board_usb_uvc_status_led_stop(void)
{
    TaskHandle_t task = s_status_led_task;
    if (task == NULL) {
        return;
    }

    xTaskNotify(task, BOARD_USB_UVC_STATUS_LED_STOP, eSetValueWithOverwrite);
    for (int retry = 0; retry < 20 && s_status_led_task != NULL; retry++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (s_status_led_task != NULL) {
        ESP_LOGW(TAG, "P4X status LED task did not stop in time; deleting it");
        vTaskDelete(s_status_led_task);
        s_status_led_task = NULL;
    }
}
#endif

static void board_usb_uvc_close_devices(board_usb_uvc_t *uvc)
{
    board_usb_uvc_stop_cb(uvc);

    if (uvc->codec_fd >= 0) {
        close(uvc->codec_fd);
        uvc->codec_fd = -1;
    }

    if (uvc->capture_fd >= 0) {
        close(uvc->capture_fd);
        uvc->capture_fd = -1;
    }

    free(uvc->uvc_buffer);
    uvc->uvc_buffer = NULL;
    uvc->codec_capture_buffer = NULL;
    memset(uvc->capture_buffer, 0, sizeof(uvc->capture_buffer));
}

#endif // CONFIG_WT_BSP_ENABLE_USB_DEVICE_UVC && CONFIG_WT_BSP_ENABLE_CSI
