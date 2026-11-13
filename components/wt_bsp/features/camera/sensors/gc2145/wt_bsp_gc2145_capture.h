/*
 * SPDX-FileCopyrightText: 2026 Wireless-Tag
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "wt_bsp_camera_port.h"

#define WT_BSP_GC2145_VGA_WIDTH      640
#define WT_BSP_GC2145_VGA_HEIGHT     480
#define WT_BSP_GC2145_VGA_XCLK_HZ    25000000U
#define WT_BSP_GC2145_720P_WIDTH     1280
#define WT_BSP_GC2145_720P_HEIGHT    720
#define WT_BSP_GC2145_720P_XCLK_HZ   24000000U

typedef struct wt_bsp_gc2145_capture wt_bsp_gc2145_capture_t;

typedef struct {
    uint8_t *data;        /*!< DMA-owned RGB565-BE (VGA) or UYVY (720p). Read only. */
    size_t size;          /*!< Bytes in the complete raw frame. */
    int64_t timestamp_us; /*!< Monotonic frame completion time. */
    uint32_t index;       /*!< Buffer ownership token used by release(). */
} wt_bsp_gc2145_frame_t;

/* Serialize all calls. Prepare expects a NULL instance and leaves the sensor
 * stopped. Failed cleanup retains the instance. Release any frame before stop. */
esp_err_t wt_bsp_gc2145_capture_prepare(wt_bsp_gc2145_capture_t **capture, const wt_bsp_camera_info_t *info);
esp_err_t wt_bsp_gc2145_capture_start(wt_bsp_gc2145_capture_t *capture);
esp_err_t wt_bsp_gc2145_capture_stop(wt_bsp_gc2145_capture_t **capture);
esp_err_t wt_bsp_gc2145_capture_acquire(wt_bsp_gc2145_capture_t *capture, wt_bsp_gc2145_frame_t *frame);
void wt_bsp_gc2145_capture_release(wt_bsp_gc2145_capture_t *capture, const wt_bsp_gc2145_frame_t *frame);
