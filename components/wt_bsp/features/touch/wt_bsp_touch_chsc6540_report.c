/**
 * @file wt_bsp_touch_chsc6540_report.c
 * @author Wireless-Tag
 * @brief CHSC6540 report decoder.
 * @version 0.1
 * @date 2026-11-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_touch_chsc6540_report.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

bool wt_bsp_touch_chsc6540_decode_report(const uint8_t *report, size_t size,
                                         const wt_bsp_touch_chsc6540_mapping_t *mapping,
                                         wt_bsp_touch_chsc6540_sample_t *sample)
{
    if (sample == NULL) {
        return false;
    }
    *sample = (wt_bsp_touch_chsc6540_sample_t) {0};
    if (report == NULL || mapping == NULL || size < WT_BSP_TOUCH_CHSC6540_REPORT_SIZE ||
        mapping->width == 0 || mapping->height == 0) {
        return false;
    }

    uint8_t count = report[2] & 0x0f;
    if (count == 0) {
        return true;
    }
    if (count > 2) {
        return false;
    }

    uint8_t event = report[3] >> 6;
    if (event == 1) {
        return true;
    }
    if (event == 3) {
        return false;
    }

    uint16_t x = ((uint16_t)(report[3] & 0x0f) << 8) | report[4];
    uint16_t y = ((uint16_t)(report[5] & 0x0f) << 8) | report[6];
    if (mapping->swap_xy) {
        uint16_t tmp = x;
        x = y;
        y = tmp;
    }
    if (x >= mapping->width || y >= mapping->height) {
        return false;
    }
    sample->x = mapping->mirror_x ? mapping->width - 1 - x : x;
    sample->y = mapping->mirror_y ? mapping->height - 1 - y : y;
    sample->pressed = true;
    return true;
}

/* ==================== [Static Functions] ================================== */
