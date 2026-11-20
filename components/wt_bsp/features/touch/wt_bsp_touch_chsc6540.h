/**
 * @file wt_bsp_touch_chsc6540.h
 * @author Wireless-Tag
 * @brief CHSC6540 polling controller interface.
 * @version 0.1
 * @date 2026-11-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __WT_BSP_TOUCH_CHSC6540_H__
#define __WT_BSP_TOUCH_CHSC6540_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"

#if WT_BSP_TOUCH_ENABLED

#include "driver/i2c_master.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Probe the controller at its 7-bit I2C address.
 * @param[in] bus Borrowed board I2C bus.
 * @return ESP_OK on ACK, otherwise the I2C probe error.
 */
esp_err_t wt_bsp_touch_chsc6540_probe(i2c_master_bus_handle_t bus);

/**
 * @brief Create a polling controller on the shared I2C bus.
 * @param[in] bus Borrowed board bus which must outlive the controller.
 * @param[in] config Screen dimensions and touch transforms, copied on success.
 * @param[out] out Controller owned by the caller until esp_lcd_touch_del() succeeds.
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_NO_MEM or an I2C driver error.
 */
esp_err_t wt_bsp_touch_chsc6540_new(i2c_master_bus_handle_t bus, const esp_lcd_touch_config_t *config,
                                    esp_lcd_touch_handle_t *out);

/**
 * @brief Read and cache the first contact, clearing stale data on failure.
 * @param[in] handle Borrowed initialized CHSC6540 controller.
 * @return ESP_OK for a valid sample, ESP_ERR_INVALID_ARG for a missing handle,
 *         ESP_ERR_INVALID_RESPONSE for malformed data, or the I2C read error.
 */
esp_err_t wt_bsp_touch_chsc6540_read(esp_lcd_touch_handle_t handle);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_TOUCH_ENABLED

#endif // __WT_BSP_TOUCH_CHSC6540_H__
