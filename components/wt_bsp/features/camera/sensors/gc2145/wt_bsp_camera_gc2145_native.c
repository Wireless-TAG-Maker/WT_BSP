/*
 * SPDX-FileCopyrightText: 2026 Wireless-Tag
 * SPDX-License-Identifier: Apache-2.0
 */

#include "wt_bsp_camera_interface.h"

#if WT_BSP_CAMERA_ENABLED && (CONFIG_WT_BSP_CAMERA_SENSOR_GC2145_VGA || CONFIG_WT_BSP_CAMERA_SENSOR_GC2145_720P)

#include <stdlib.h>

#include "driver/jpeg_encode.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "wt_bsp_gc2145_capture.h"

typedef struct {
    SemaphoreHandle_t mutex;
    wt_bsp_gc2145_capture_t *capture;
    ppa_client_handle_t ppa;
    jpeg_encoder_handle_t encoder;
    uint8_t *rgb565;
    size_t rgb565_capacity;
    uint8_t *jpeg;
    size_t jpeg_capacity;
    bool direct_uyvy;
    bool prepared;
    bool streaming;
    bool frame_borrowed;
    wt_bsp_camera_status_t health;
} gc2145_native_context_t;

static const char *TAG = "gc2145_native";

static esp_err_t gc2145_native_prepare_locked(wt_bsp_camera_t camera)
{
    gc2145_native_context_t *ctx = camera->backend;
    const wt_bsp_camera_info_t *info = &camera->info;
    if (!ctx->direct_uyvy && ctx->rgb565 == NULL) {
        const jpeg_encode_memory_alloc_cfg_t memory = {
            .buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER,
        };
        ctx->rgb565 = jpeg_alloc_encoder_mem(info->format.width * info->format.height * 2,
                                            &memory, &ctx->rgb565_capacity);
        ESP_RETURN_ON_FALSE(ctx->rgb565, ESP_ERR_NO_MEM, TAG, "allocate RGB565 buffer");
    }
    if (ctx->jpeg == NULL) {
        const jpeg_encode_memory_alloc_cfg_t memory = {
            .buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER,
        };
        ctx->jpeg = jpeg_alloc_encoder_mem(info->jpeg_buffer_capacity, &memory, &ctx->jpeg_capacity);
        ESP_RETURN_ON_FALSE(ctx->jpeg, ESP_ERR_NO_MEM, TAG, "allocate JPEG buffer");
    }
    if (!ctx->direct_uyvy && ctx->ppa == NULL) {
        const ppa_client_config_t config = {
            .oper_type = PPA_OPERATION_SRM,
        };
        ESP_RETURN_ON_ERROR(ppa_register_client(&config, &ctx->ppa), TAG, "create PPA client");
    }
    if (ctx->encoder == NULL) {
        const jpeg_encode_engine_cfg_t config = {
            .timeout_ms = info->jpeg_encode_timeout_ms,
        };
        ESP_RETURN_ON_ERROR(jpeg_new_encoder_engine(&config, &ctx->encoder), TAG, "create JPEG engine");
    }
    if (!ctx->prepared) {
        ESP_RETURN_ON_ERROR(wt_bsp_gc2145_capture_stop(&ctx->capture), TAG, "finish DVP cleanup");
        ESP_RETURN_ON_ERROR(wt_bsp_gc2145_capture_prepare(&ctx->capture, info), TAG, "prepare DVP capture");
        ctx->prepared = true;
    }
    return ESP_OK;
}

static esp_err_t gc2145_native_prepare(wt_bsp_camera_t camera, wt_bsp_camera_format_t *format)
{
    gc2145_native_context_t *ctx = camera->backend;
    xSemaphoreTakeRecursive(ctx->mutex, portMAX_DELAY);
    esp_err_t ret = ctx->frame_borrowed ? ESP_ERR_INVALID_STATE : gc2145_native_prepare_locked(camera);
    if (ret == ESP_OK) {
        *format = camera->info.format;
    }
    if (!ctx->frame_borrowed) {
        ctx->health.ready = ret == ESP_OK;
        ctx->health.last_error = ret;
    }
    xSemaphoreGiveRecursive(ctx->mutex);
    return ret;
}

static esp_err_t gc2145_native_swap_rgb565(const wt_bsp_camera_info_t *info, gc2145_native_context_t *ctx,
                                            const wt_bsp_gc2145_frame_t *raw)
{
    const uint32_t width = info->format.width;
    const uint32_t height = info->format.height;
    const ppa_srm_oper_config_t swap = {
        .in = {
            .buffer = raw->data, .pic_w = width, .pic_h = height,
            .block_w = width, .block_h = height, .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .out = {
            .buffer = ctx->rgb565, .buffer_size = ctx->rgb565_capacity,
            .pic_w = width, .pic_h = height, .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .scale_x = 1.0f,
        .scale_y = 1.0f,
        .byte_swap = true,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };
    /* The sensor emits RGB565 big-endian. PPA writes the JPEG input directly. */
    esp_err_t ret = esp_cache_msync(ctx->rgb565, ctx->rgb565_capacity,
                                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    if (ret == ESP_OK) {
        ret = ppa_do_scale_rotate_mirror(ctx->ppa, &swap);
    }
    wt_bsp_gc2145_capture_release(ctx->capture, raw);
    return ret;
}

static esp_err_t gc2145_native_encode(const wt_bsp_camera_info_t *info, gc2145_native_context_t *ctx,
                                      const wt_bsp_gc2145_frame_t *raw, uint32_t *jpeg_size)
{
    if (!ctx->direct_uyvy) {
        ESP_RETURN_ON_ERROR(gc2145_native_swap_rgb565(info, ctx, raw), TAG, "prepare JPEG RGB565 input");
    }
    esp_err_t ret = esp_cache_msync(ctx->jpeg, ctx->jpeg_capacity,
                                  ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    if (ret == ESP_OK) {
        const jpeg_encode_cfg_t config = {
            .height = info->format.height,
            .width = info->format.width,
            .src_type = ctx->direct_uyvy ? JPEG_ENCODE_IN_FORMAT_YUV422 : JPEG_ENCODE_IN_FORMAT_RGB565,
            .sub_sample = JPEG_DOWN_SAMPLING_YUV422,
            .image_quality = info->jpeg_quality,
        };
        const uint8_t *input = ctx->direct_uyvy ? raw->data : ctx->rgb565;
        ret = jpeg_encoder_process(ctx->encoder, &config, input, raw->size,
                                   ctx->jpeg, ctx->jpeg_capacity, jpeg_size);
    }
    if (ctx->direct_uyvy) {
        /* JPEG DMA borrows the captured UYVY buffer until encoding returns. */
        wt_bsp_gc2145_capture_release(ctx->capture, raw);
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "encode JPEG");
    if (*jpeg_size < 4 || *jpeg_size > ctx->jpeg_capacity ||
            ctx->jpeg[0] != 0xff || ctx->jpeg[1] != 0xd8 ||
            ctx->jpeg[*jpeg_size - 2] != 0xff || ctx->jpeg[*jpeg_size - 1] != 0xd9) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t gc2145_native_capture(wt_bsp_camera_t camera, wt_bsp_camera_frame_cb_t callback,
                                   void *user_data)
{
    gc2145_native_context_t *ctx = camera->backend;
    xSemaphoreTakeRecursive(ctx->mutex, portMAX_DELAY);
    if (ctx->frame_borrowed) {
        xSemaphoreGiveRecursive(ctx->mutex);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = gc2145_native_prepare_locked(camera);
    if (ret == ESP_OK && !ctx->streaming) {
        ret = wt_bsp_gc2145_capture_start(ctx->capture);
        ctx->streaming = ret == ESP_OK;
    }
    wt_bsp_gc2145_frame_t raw;
    if (ret == ESP_OK) {
        ret = wt_bsp_gc2145_capture_acquire(ctx->capture, &raw);
    }
    if (ret != ESP_OK) {
        /* Rebuild the DMA state after a lost capture on the next request. */
        const esp_err_t cleanup = wt_bsp_gc2145_capture_stop(&ctx->capture);
        if (cleanup != ESP_OK) {
            ESP_LOGE(TAG, "Capture cleanup failed: %s", esp_err_to_name(cleanup));
        }
        ctx->prepared = false;
        ctx->streaming = false;
        ctx->health.ready = false;
        ctx->health.last_error = ret;
        xSemaphoreGiveRecursive(ctx->mutex);
        return ret;
    }

    uint32_t jpeg_size = 0;
    ret = gc2145_native_encode(&camera->info, ctx, &raw, &jpeg_size);
    ctx->health.ready = true;
    ctx->health.last_error = ret;
    if (ret == ESP_OK) {
        const wt_bsp_camera_frame_t frame = {
            .data = ctx->jpeg,
            .length = jpeg_size,
            .width = camera->info.format.width,
            .height = camera->info.format.height,
            .timestamp_us = raw.timestamp_us,
        };
        ctx->health.frames++;
        ctx->health.last_frame_us = raw.timestamp_us;
        ctx->frame_borrowed = true;
        ret = callback(&frame, user_data);
        ctx->frame_borrowed = false;
    }
    xSemaphoreGiveRecursive(ctx->mutex);
    return ret;
}

static esp_err_t gc2145_native_get_status(wt_bsp_camera_t camera, wt_bsp_camera_status_t *status)
{
    gc2145_native_context_t *ctx = camera->backend;
    xSemaphoreTakeRecursive(ctx->mutex, portMAX_DELAY);
    *status = ctx->health;
    xSemaphoreGiveRecursive(ctx->mutex);
    return ESP_OK;
}

static esp_err_t gc2145_native_deinit(wt_bsp_camera_t camera)
{
    gc2145_native_context_t *ctx = camera->backend;
    xSemaphoreTakeRecursive(ctx->mutex, portMAX_DELAY);
    if (ctx->frame_borrowed) {
        xSemaphoreGiveRecursive(ctx->mutex);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = wt_bsp_gc2145_capture_stop(&ctx->capture);
    ctx->health.ready = false;
    ctx->prepared = false;
    ctx->streaming = false;
    if (ret == ESP_OK && ctx->encoder != NULL) {
        ret = jpeg_del_encoder_engine(ctx->encoder);
        if (ret == ESP_OK) {
            ctx->encoder = NULL;
        }
    }
    if (ret == ESP_OK && ctx->ppa != NULL) {
        ret = ppa_unregister_client(ctx->ppa);
        if (ret == ESP_OK) {
            ctx->ppa = NULL;
        }
    }
    ctx->health.last_error = ret;
    xSemaphoreGiveRecursive(ctx->mutex);
    if (ret == ESP_OK) {
        free(ctx->jpeg);
        free(ctx->rgb565);
        vSemaphoreDelete(ctx->mutex);
        free(ctx);
        camera->backend = NULL;
    }
    return ret;
}

static esp_err_t gc2145_native_init(wt_bsp_camera_t camera, const wt_bsp_camera_info_t *info,
                                    const wt_bsp_camera_ops_t **ops, bool direct_uyvy)
{
    static const wt_bsp_camera_ops_t driver_ops = {
        .deinit = gc2145_native_deinit,
        .prepare = gc2145_native_prepare,
        .capture = gc2145_native_capture,
        .get_status = gc2145_native_get_status,
    };

    const uint32_t width = direct_uyvy ? WT_BSP_GC2145_720P_WIDTH : WT_BSP_GC2145_VGA_WIDTH;
    const uint32_t height = direct_uyvy ? WT_BSP_GC2145_720P_HEIGHT : WT_BSP_GC2145_VGA_HEIGHT;
    const uint32_t xclk_hz = direct_uyvy ? WT_BSP_GC2145_720P_XCLK_HZ : WT_BSP_GC2145_VGA_XCLK_HZ;
    if (info->format.width != width || info->format.height != height || info->format.fps != 30 ||
            info->dvp.xclk_freq_hz != xclk_hz || info->capture_buffer_count != 2 ||
            info->property_count != 0 || info->dequeue_timeout_ms <= 0 ||
            info->jpeg_buffer_capacity == 0 || info->jpeg_buffer_capacity > UINT32_MAX ||
            info->jpeg_quality < 1 || info->jpeg_quality > 100 || info->jpeg_encode_timeout_ms <= 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    gc2145_native_context_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ctx->mutex = xSemaphoreCreateRecursiveMutex();
    if (ctx->mutex == NULL) {
        free(ctx);
        return ESP_ERR_NO_MEM;
    }
    ctx->direct_uyvy = direct_uyvy;
    ctx->health.last_error = ESP_ERR_INVALID_STATE;
    camera->backend = ctx;
    *ops = &driver_ops;
    return ESP_OK;
}

#if CONFIG_WT_BSP_CAMERA_SENSOR_GC2145_VGA
static esp_err_t gc2145_vga_init(wt_bsp_camera_t camera, const wt_bsp_camera_info_t *info,
                                const wt_bsp_camera_ops_t **ops)
{
    return gc2145_native_init(camera, info, ops, false);
}

WT_BSP_CAMERA_DRIVER_REGISTER(gc2145_vga, "gc2145_vga", gc2145_vga_init);
#endif

#if CONFIG_WT_BSP_CAMERA_SENSOR_GC2145_720P
static esp_err_t gc2145_720p_init(wt_bsp_camera_t camera, const wt_bsp_camera_info_t *info,
                                  const wt_bsp_camera_ops_t **ops)
{
    return gc2145_native_init(camera, info, ops, true);
}

WT_BSP_CAMERA_DRIVER_REGISTER(gc2145_720p, "gc2145_720p", gc2145_720p_init);
#endif

#endif
