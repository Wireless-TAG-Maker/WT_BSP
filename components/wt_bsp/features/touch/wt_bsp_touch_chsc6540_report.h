/**
 * @file wt_bsp_touch_chsc6540_report.h
 * @author Wireless-Tag
 * @brief CHSC6540 report decoder.
 * @version 0.1
 * @date 2026-11-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __WT_BSP_TOUCH_CHSC6540_REPORT_H__
#define __WT_BSP_TOUCH_CHSC6540_REPORT_H__

/* ==================== [Includes] ========================================== */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

#define WT_BSP_TOUCH_CHSC6540_REPORT_SIZE 7

/* ==================== [Typedefs] ========================================== */

/** @brief Final screen dimensions and transforms applied in the given order. */
typedef struct {
    uint16_t width;  /*!< Final horizontal pixel count. */
    uint16_t height;  /*!< Final vertical pixel count. */
    bool swap_xy;  /*!< Exchange the raw axes before checking bounds. */
    bool mirror_x;  /*!< Mirror the horizontal axis. */
    bool mirror_y;  /*!< Mirror the vertical axis. */
} wt_bsp_touch_chsc6540_mapping_t;

/** @brief One pointer sample; coordinates are meaningful only when pressed. */
typedef struct {
    bool pressed;  /*!< True while the first contact is pressed. */
    uint16_t x;  /*!< Horizontal pixel coordinate. */
    uint16_t y;  /*!< Vertical pixel coordinate. */
} wt_bsp_touch_chsc6540_sample_t;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Decode registers 0x00..0x06 into a single LVGL-compatible pointer.
 * @param[in] report Report bytes, read beginning at register 0x00.
 * @param[in] size Available byte count.
 * @param[in] mapping Final screen size, then swap/mirror settings.
 * @param[out] sample Cleared on malformed input; otherwise receives the sample.
 * @return true for a valid press/release, false for invalid arguments or data.
 *
 * The chip reports up to two contacts; this single-pointer interface uses the
 * first. Reserved event codes and out-of-screen coordinates are rejected.
 */
bool wt_bsp_touch_chsc6540_decode_report(const uint8_t *report, size_t size,
                                         const wt_bsp_touch_chsc6540_mapping_t *mapping,
                                         wt_bsp_touch_chsc6540_sample_t *sample);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __WT_BSP_TOUCH_CHSC6540_REPORT_H__
