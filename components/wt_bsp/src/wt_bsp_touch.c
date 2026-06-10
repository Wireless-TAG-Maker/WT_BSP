/**
 * @file wt_bsp_touch.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-06-11
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_touch.h"

#if WT_BSP_TOUCH_ENABLE_IS_ENABLED

#include <string.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lcd_touch_st7123.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_touch";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_touch_init(wt_bsp_touch_t touch, const wt_bsp_touch_info_t *info)
{
    if (touch == NULL || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(touch, 0, sizeof(*touch));
    touch->info = *info;

    // Create I2C bus
    i2c_master_bus_handle_t i2c_handle = NULL;
    i2c_master_bus_config_t i2c_config = {
        .i2c_port = info->i2c_port,
        .sda_io_num = info->sda_pin,
        .scl_io_num = info->scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&i2c_config, &i2c_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new I2C master bus failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
    tp_io_config.scl_speed_hz = 400000;

    ret = esp_lcd_new_panel_io_i2c_v2(i2c_handle, &tp_io_config, &tp_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new panel IO failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = info->width,
        .y_max = info->height,
        .rst_gpio_num = info->rst_pin,
        .int_gpio_num = info->int_pin,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    ret = esp_lcd_touch_new_i2c_st7123(tp_io_handle, &tp_cfg, &touch->handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new touch device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    touch->is_initialized = true;
    ESP_LOGI(TAG, "Touch initialized: %ux%u", info->width, info->height);

    return ESP_OK;
}

esp_err_t wt_bsp_touch_deinit(wt_bsp_touch_t touch)
{
    if (touch == NULL || !touch->is_initialized) {
        return ESP_OK;
    }

    if (touch->handle) {
        esp_lcd_touch_del(touch->handle);
    }
    touch->is_initialized = false;

    return ESP_OK;
}

esp_err_t wt_bsp_touch_read(wt_bsp_touch_t touch)
{
    if (touch == NULL || !touch->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_lcd_touch_read_data(touch->handle);
}

bool wt_bsp_touch_get_coordinates(wt_bsp_touch_t touch, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    if (touch == NULL || !touch->is_initialized) {
        return false;
    }
    return esp_lcd_touch_get_coordinates(touch->handle, x, y, strength, point_num, max_point_num);
}

esp_lcd_touch_handle_t wt_bsp_touch_get_handle(wt_bsp_touch_t touch)
{
    return (touch == NULL) ? NULL : touch->handle;
}

#endif // WT_BSP_TOUCH_ENABLE_IS_ENABLED
