/**
 * @file board_gc2145.h
 * @author Wireless-Tag
 * @brief GC2145 retained sensor profile lifecycle.
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

#ifndef __BOARD_GC2145_H__
#define __BOARD_GC2145_H__

/* ==================== [Includes] ========================================== */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Configure GC2145 UYVY from the driver's 1600x1200 RGB565_BE/20 MHz/13 fps
 * default. Call once after opening DVP, before allocating buffers or streaming.
 * The caller must already supply a 20 MHz XCLK. The format and register copy
 * remain allocated for the device lifetime, including failures after submission.
 * @param[in] capture_fd Open DVP video descriptor.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for an invalid descriptor,
 *         ESP_ERR_INVALID_STATE for a sensor/format mismatch or repeated setup,
 *         ESP_ERR_NO_MEM on allocation failure, or ESP_FAIL on an ioctl failure.
 */
esp_err_t board_gc2145_configure_uxga(int capture_fd);

/** @brief Free the retained register table only after the sensor has been destroyed. */
void board_gc2145_release_format(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __BOARD_GC2145_H__
