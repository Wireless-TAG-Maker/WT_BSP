/**
 * @file camera_uvc.c
 * @author Wireless-Tag
 * @brief Application-owned camera source selection and USB frame delivery.
 * @version 0.1
 * @date 2026-11-04
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "camera_uvc.h"
#include "wt_bsp.h"

#if WT_BSP_USB_DEVICE_UVC_ENABLED && WT_BSP_CSI_ENABLED
#include "camera_uvc_csi.h"
#endif

#if WT_BSP_USB_DEVICE_UVC_ENABLED && WT_BSP_CAMERA_ENABLED && !WT_BSP_CSI_ENABLED
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

/* ==================== [Defines] =========================================== */
/* ==================== [Typedefs] ========================================== */

typedef struct {
    wt_bsp_camera_t camera;
    wt_bsp_usb_device_uvc_t uvc;
    uint8_t *usb_buffer;
    uint8_t *jpeg_buffer;
    size_t capacity;
    uvc_fb_t frame;
    atomic_bool streaming;
    atomic_bool frame_ready;
    bool started;
} camera_uvc_context_t;

/* ==================== [Static Prototypes] ================================= */

static esp_err_t camera_uvc_stream_start(uvc_format_t format, int width, int height,
                                       int rate, void *context);
static void camera_uvc_stream_stop(void *context);
static uvc_fb_t *camera_uvc_get_frame(void *context);
static void camera_uvc_return_frame(uvc_fb_t *frame, void *context);
static esp_err_t camera_uvc_copy_jpeg(const wt_bsp_camera_frame_t *frame, void *context);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "camera_uvc";
static camera_uvc_context_t s_bridge;

/* ==================== [Macros] ============================================ */

#endif

/* ==================== [Global Functions] ================================== */

esp_err_t camera_uvc_start(void)
{
#if WT_BSP_USB_DEVICE_UVC_ENABLED && WT_BSP_CSI_ENABLED
    return camera_uvc_csi_start();
#elif WT_BSP_USB_DEVICE_UVC_ENABLED && WT_BSP_CAMERA_ENABLED
    esp_err_t ret = ESP_OK;

    if (s_bridge.started) {
        return ESP_OK;
    }
    s_bridge.camera = wt_bsp_get_camera();
    s_bridge.uvc = wt_bsp_get_usb_device_uvc();
    if (s_bridge.camera == NULL || s_bridge.uvc == NULL) {
        ESP_LOGE(TAG, "Camera or USB UVC feature is unavailable");
        return ESP_ERR_INVALID_STATE;
    }

#if CONFIG_WT_CAMERA_UVC_S31_GC2145_720P_PROFILE
    /* Bound both MJPEG staging buffers to the PSRAM budget used by the 720p profile. */
    s_bridge.capacity = 512 * 1024;
#else
    s_bridge.capacity = (size_t)CONFIG_UVC_CAM1_FRAMESIZE_WIDTH * CONFIG_UVC_CAM1_FRAMESIZE_HEIGT;
#endif
    s_bridge.usb_buffer = heap_caps_malloc(s_bridge.capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_bridge.jpeg_buffer = heap_caps_malloc(s_bridge.capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_bridge.usb_buffer == NULL || s_bridge.jpeg_buffer == NULL) {
        free(s_bridge.usb_buffer);
        free(s_bridge.jpeg_buffer);
        s_bridge.usb_buffer = NULL;
        s_bridge.jpeg_buffer = NULL;
        s_bridge.capacity = 0;
        return ESP_ERR_NO_MEM;
    }

    uvc_device_config_t stream = {
        .uvc_buffer = s_bridge.usb_buffer,
        .uvc_buffer_size = s_bridge.capacity,
        .start_cb = camera_uvc_stream_start,
        .stop_cb = camera_uvc_stream_stop,
        .fb_get_cb = camera_uvc_get_frame,
        .fb_return_cb = camera_uvc_return_frame,
        .cb_ctx = &s_bridge,
    };
    ret = wt_bsp_usb_device_uvc_start(s_bridge.uvc, &stream);
    if (ret != ESP_OK) {
        free(s_bridge.usb_buffer);
        free(s_bridge.jpeg_buffer);
        s_bridge.usb_buffer = NULL;
        s_bridge.jpeg_buffer = NULL;
        s_bridge.capacity = 0;
        return ret;
    }
    s_bridge.started = true;
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t camera_uvc_stop(void)
{
#if WT_BSP_USB_DEVICE_UVC_ENABLED && WT_BSP_CSI_ENABLED
    return camera_uvc_csi_stop();
#elif WT_BSP_USB_DEVICE_UVC_ENABLED && WT_BSP_CAMERA_ENABLED
    if (!s_bridge.started) {
        return ESP_OK;
    }
    atomic_store(&s_bridge.streaming, false);
    esp_err_t ret = wt_bsp_usb_device_uvc_stop(s_bridge.uvc);
    if (ret != ESP_OK) {
        return ret;
    }
    atomic_store(&s_bridge.streaming, false);
    free(s_bridge.usb_buffer);
    free(s_bridge.jpeg_buffer);
    s_bridge.usb_buffer = NULL;
    s_bridge.jpeg_buffer = NULL;
    s_bridge.capacity = 0;
    s_bridge.frame_ready = false;
    s_bridge.started = false;
    s_bridge.camera = NULL;
    s_bridge.uvc = NULL;
    return ESP_OK;
#else
    return ESP_OK;
#endif
}

bool camera_uvc_is_streaming(void)
{
#if WT_BSP_USB_DEVICE_UVC_ENABLED && WT_BSP_CSI_ENABLED
    return camera_uvc_csi_is_streaming();
#elif WT_BSP_USB_DEVICE_UVC_ENABLED && WT_BSP_CAMERA_ENABLED
    return atomic_load(&s_bridge.streaming);
#else
    return false;
#endif
}

/* ==================== [Static Functions] ================================== */

#if WT_BSP_USB_DEVICE_UVC_ENABLED && WT_BSP_CAMERA_ENABLED && !WT_BSP_CSI_ENABLED

static esp_err_t camera_uvc_stream_start(uvc_format_t format, int width, int height,
                                       int rate, void *context)
{
    camera_uvc_context_t *bridge = context;
    wt_bsp_camera_format_t capture_format;
    if (bridge == NULL || format != UVC_FORMAT_JPEG || width <= 0 || height <= 0 || rate <= 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_err_t ret = wt_bsp_camera_prepare(bridge->camera, &capture_format);
    if (ret != ESP_OK) {
        return ret;
    }
    if ((uint32_t)width != capture_format.width || (uint32_t)height != capture_format.height || (uint32_t)rate != capture_format.fps) {
        ESP_LOGE(TAG, "USB mode does not match the selected camera format");
        return ESP_ERR_NOT_SUPPORTED;
    }
    bridge->frame_ready = false;
    atomic_store(&bridge->streaming, true);
    return ESP_OK;
}

static void camera_uvc_stream_stop(void *context)
{
    camera_uvc_context_t *bridge = context;
    if (bridge != NULL) {
        atomic_store(&bridge->streaming, false);
        bridge->frame_ready = false;
    }
}

static esp_err_t camera_uvc_copy_jpeg(const wt_bsp_camera_frame_t *frame, void *context)
{
    camera_uvc_context_t *bridge = context;
    if (frame == NULL || bridge == NULL || frame->data == NULL || frame->length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (frame->length > bridge->capacity) {
        ESP_LOGE(TAG, "JPEG frame exceeds the application bridge buffer");
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(bridge->jpeg_buffer, frame->data, frame->length);
    bridge->frame.buf = bridge->jpeg_buffer;
    bridge->frame.len = frame->length;
    bridge->frame.width = frame->width;
    bridge->frame.height = frame->height;
    bridge->frame.format = UVC_FORMAT_JPEG;
    bridge->frame.timestamp.tv_sec = frame->timestamp_us / 1000000;
    bridge->frame.timestamp.tv_usec = frame->timestamp_us % 1000000;
    bridge->frame_ready = true;
    return ESP_OK;
}

static uvc_fb_t *camera_uvc_get_frame(void *context)
{
    camera_uvc_context_t *bridge = context;
    if (bridge == NULL || !atomic_load(&bridge->streaming)) {
        return NULL;
    }
    bridge->frame_ready = false;
    esp_err_t ret = wt_bsp_camera_capture(bridge->camera, camera_uvc_copy_jpeg, bridge);
    if (ret != ESP_OK || !atomic_load(&bridge->frame_ready) || !atomic_load(&bridge->streaming)) {
        ESP_LOGW(TAG, "Camera frame unavailable: %s", esp_err_to_name(ret));
        return NULL;
    }
    return &bridge->frame;
}

static void camera_uvc_return_frame(uvc_fb_t *frame, void *context)
{
    camera_uvc_context_t *bridge = context;
    (void)frame;
    if (bridge != NULL) {
        bridge->frame_ready = false;
    }
}

#endif
