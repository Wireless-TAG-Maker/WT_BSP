/**
 * @file factory_compact_controls.h
 * @author Wireless-Tag
 * @brief Factory controls for the 320x480 portrait display.
 * @version 0.1
 * @date 2026-11-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __FACTORY_COMPACT_CONTROLS_H__
#define __FACTORY_COMPACT_CONTROLS_H__

/* ==================== [Includes] ========================================== */

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Typedefs] ========================================== */

/** @brief Borrowed widgets owned by the active LVGL screen. */
typedef struct {
    lv_obj_t *panel;        /*!< Controls below the camera, hidden in fullscreen. */
    lv_obj_t *red;          /*!< Red channel slider. */
    lv_obj_t *green;        /*!< Green channel slider. */
    lv_obj_t *blue;         /*!< Blue channel slider. */
    lv_obj_t *swatch;       /*!< Current RGB LED color. */
    lv_obj_t *sd_status;    /*!< SD mount result and capacity. */
    lv_obj_t *wifi_status;  /*!< Wi-Fi connection status, NULL without Wi-Fi. */
    lv_obj_t *ap_name;      /*!< Access point name, NULL without Wi-Fi. */
    lv_obj_t *clock;        /*!< Local date and time, NULL without Wi-Fi. */
} factory_compact_controls_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Fill the space below a portrait camera preview with factory controls.
 * @param[in] screen Active screen with a column layout and a sized camera preview.
 * @param[in] with_wifi Add the network and clock rows for a Wi-Fi host.
 * @param[in] slider_cb Callback for changes to any RGB channel.
 * @param[in] mount_cb Callback for the SD mount button.
 * @note Create and use these widgets while holding the display lock.
 * @return Borrowed widget handles valid until the screen is deleted.
 */
factory_compact_controls_t factory_compact_controls_create(lv_obj_t *screen, bool with_wifi,
                                                           lv_event_cb_t slider_cb, lv_event_cb_t mount_cb);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __FACTORY_COMPACT_CONTROLS_H__
