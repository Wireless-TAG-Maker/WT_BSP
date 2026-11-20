/**
 * @file wt_bsp_touch_chsc6540.c
 * @author Wireless-Tag
 * @brief CHSC6540 polling and esp_lcd_touch integration.
 * @version 0.1
 * @date 2026-11-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_touch_chsc6540.h"
#include "wt_bsp_touch_chsc6540_report.h"

#include "esp_check.h"

#include <stdlib.h>

#if WT_BSP_TOUCH_ENABLED

/* ==================== [Defines] =========================================== */

#define WT_BSP_TOUCH_CHSC6540_ADDRESS 0x2e

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static bool wt_bsp_touch_chsc6540_get_xy(esp_lcd_touch_handle_t handle, uint16_t *x, uint16_t *y,
                                         uint16_t *strength, uint8_t *points, uint8_t max_points);
static esp_err_t wt_bsp_touch_chsc6540_poll(esp_lcd_touch_handle_t handle);
static esp_err_t wt_bsp_touch_chsc6540_del(esp_lcd_touch_handle_t handle);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_touch_chsc6540";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_touch_chsc6540_probe(i2c_master_bus_handle_t bus)
{
    return i2c_master_probe(bus, WT_BSP_TOUCH_CHSC6540_ADDRESS, 100);
}

esp_err_t wt_bsp_touch_chsc6540_read(esp_lcd_touch_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "Missing touch handle");
    uint8_t report[WT_BSP_TOUCH_CHSC6540_REPORT_SIZE];
    wt_bsp_touch_chsc6540_sample_t sample = {0};
    const wt_bsp_touch_chsc6540_mapping_t mapping = {
        .width = handle->config.x_max + 1,
        .height = handle->config.y_max + 1,
    };
    const uint8_t reg = 0;
    esp_err_t ret = i2c_master_transmit_receive(handle->config.driver_data, &reg, 1,
                                               report, sizeof(report), 20);
    if (ret == ESP_OK && !wt_bsp_touch_chsc6540_decode_report(report, sizeof(report), &mapping, &sample)) {
        ret = ESP_ERR_INVALID_RESPONSE;
    }
    portENTER_CRITICAL(&handle->data.lock);
    handle->data.points = sample.pressed ? 1 : 0;
    handle->data.coords[0].x = sample.x;
    handle->data.coords[0].y = sample.y;
    portEXIT_CRITICAL(&handle->data.lock);
    return ret;
}

esp_err_t wt_bsp_touch_chsc6540_new(i2c_master_bus_handle_t bus, const esp_lcd_touch_config_t *config,
                                    esp_lcd_touch_handle_t *out)
{
    ESP_RETURN_ON_FALSE(bus && config && out && config->x_max && config->y_max,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid touch configuration");
    *out = NULL;
    esp_lcd_touch_handle_t handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }
    handle->config = *config;
    /* esp_lcd_touch mirrors about the inclusive maximum coordinate. */
    handle->config.x_max--;
    handle->config.y_max--;
    handle->config.int_gpio_num = GPIO_NUM_NC;
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = WT_BSP_TOUCH_CHSC6540_ADDRESS,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t device;
    esp_err_t ret = i2c_master_bus_add_device(bus, &device_config, &device);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }
    handle->config.driver_data = device;
    handle->read_data = wt_bsp_touch_chsc6540_poll;
    handle->get_xy = wt_bsp_touch_chsc6540_get_xy;
    handle->del = wt_bsp_touch_chsc6540_del;
    handle->data.lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    *out = handle;
    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static bool wt_bsp_touch_chsc6540_get_xy(esp_lcd_touch_handle_t handle, uint16_t *x, uint16_t *y,
                                         uint16_t *strength, uint8_t *points, uint8_t max_points)
{
    portENTER_CRITICAL(&handle->data.lock);
    *points = max_points ? handle->data.points : 0;
    if (*points) {
        x[0] = handle->data.coords[0].x;
        y[0] = handle->data.coords[0].y;
        if (strength) {
            strength[0] = 1;
        }
    }
    handle->data.points = 0;
    portEXIT_CRITICAL(&handle->data.lock);
    return *points != 0;
}

/* esp_lvgl_port aborts on read errors. Report a released pointer for a bad
 * sample while the public BSP read API still returns the underlying error. */
static esp_err_t wt_bsp_touch_chsc6540_poll(esp_lcd_touch_handle_t handle)
{
    wt_bsp_touch_chsc6540_read(handle);
    return ESP_OK;
}

static esp_err_t wt_bsp_touch_chsc6540_del(esp_lcd_touch_handle_t handle)
{
    esp_err_t ret = i2c_master_bus_rm_device(handle->config.driver_data);
    if (ret == ESP_OK) {
        free(handle);
    }
    return ret;
}

#endif // WT_BSP_TOUCH_ENABLED
