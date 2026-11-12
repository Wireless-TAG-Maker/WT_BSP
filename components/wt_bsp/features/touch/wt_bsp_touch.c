/**
 * @file wt_bsp_touch.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP touch controller and LVGL integration.
 * @version 0.1
 * @date 2026-11-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_touch_port.h"

#if WT_BSP_TOUCH_ENABLED

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_touch_st7123.h"
#include "esp_lvgl_port.h"

#if defined(CONFIG_WT_BSP_DSI_LCD_ADAPTER_WLK7002_MIPI_15P) || defined(CONFIG_WT_BSP_DSI_LCD_ADAPTER_WTDKP4_LCD_ADAPTER)
#include "esp_lcd_touch_gt911.h"
#define WT_BSP_TOUCH_GT911_ENABLED 1
#else
#define WT_BSP_TOUCH_GT911_ENABLED 0
#endif

/* ==================== [Defines] =========================================== */

#define WT_BSP_TOUCH_PROBE_TIMEOUT_MS 200
#define WT_BSP_TOUCH_GT911_PRODUCT_ID_REG 0x8140
#define WT_BSP_TOUCH_GT911_CONFIG_REG 0x8047

/* ==================== [Typedefs] ========================================== */

#if WT_BSP_TOUCH_GT911_ENABLED
/**
 * @brief GT911 driver configuration and immutable coordinate scaling context.
 */
typedef struct {
    esp_lcd_touch_io_gt911_config_t config;
    uint16_t raw_width;
    uint16_t raw_height;
    uint16_t width;
    uint16_t height;
} wt_bsp_touch_gt911_data_t;
#endif

/* ==================== [Static Prototypes] ================================= */

static wt_bsp_touch_model_t wt_bsp_touch_resolve_model(wt_bsp_touch_model_t model);
#if WT_BSP_TOUCH_GT911_ENABLED
static esp_err_t wt_bsp_touch_probe_gt911(i2c_master_bus_handle_t bus, uint8_t *address);
static esp_err_t wt_bsp_touch_init_gt911(wt_bsp_touch_t touch, uint8_t address, esp_lcd_touch_config_t *config);
static void wt_bsp_touch_scale_gt911(esp_lcd_touch_handle_t handle, uint16_t *x, uint16_t *y,
                                   uint16_t *strength, uint8_t *point_num, uint8_t max_point_num);
#endif

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_touch";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_touch_get_default_info(wt_bsp_touch_info_t *info)
{
    if (info == NULL) {
        ESP_LOGE(TAG, "Touch info is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(info, 0, sizeof(*info));
    info->scl_pin = -1;
    info->sda_pin = -1;
    info->rst_pin = -1;
    info->int_pin = -1;
    info->model = wt_bsp_touch_resolve_model(WT_BSP_TOUCH_MODEL_AUTO);
    info->width = 480;
    info->height = 640;
#if WT_BSP_TOUCH_GT911_ENABLED
    info->width = 1024;
    info->height = 600;
#if defined(CONFIG_WT_BSP_DSI_LCD_ADAPTER_WTDKP4_LCD_ADAPTER)
    info->mirror_x = true;
    info->mirror_y = true;
#endif
#endif
    return ESP_OK;
}

esp_err_t wt_bsp_touch_probe(i2c_master_bus_handle_t bus, uint8_t legacy_address)
{
    if (bus == NULL) {
        ESP_LOGE(TAG, "Touch probe bus is NULL");
        return ESP_ERR_INVALID_ARG;
    }

#if WT_BSP_TOUCH_GT911_ENABLED
    uint8_t address = 0;
    (void)legacy_address;
    return wt_bsp_touch_probe_gt911(bus, &address);
#else
    if (legacy_address == 0 || legacy_address > 0x7f) {
        ESP_LOGE(TAG, "Invalid touch probe address: 0x%02x", legacy_address);
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_master_probe(bus, legacy_address, WT_BSP_TOUCH_PROBE_TIMEOUT_MS);
#endif
}

esp_err_t wt_bsp_touch_init(wt_bsp_touch_t touch, const wt_bsp_touch_info_t *info)
{
    esp_err_t ret = ESP_OK;

    if (touch == NULL || info == NULL || info->width == 0 || info->height == 0) {
        ESP_LOGE(TAG, "Invalid touch configuration");
        return ESP_ERR_INVALID_ARG;
    }
    if (touch->is_initialized) {
        ESP_LOGW(TAG, "Touch is already initialized");
        return ESP_OK;
    }
    if (touch->handle != NULL || touch->io != NULL || touch->i2c_bus != NULL ||
        touch->driver_data != NULL || touch->lvgl_indev != NULL) {
        ESP_LOGE(TAG, "Touch cleanup is incomplete");
        return ESP_ERR_INVALID_STATE;
    }

    touch->info = *info;
    touch->info.model = wt_bsp_touch_resolve_model(info->model);
    if (touch->info.model != WT_BSP_TOUCH_MODEL_ST7123 && touch->info.model != WT_BSP_TOUCH_MODEL_GT911) {
        ESP_LOGE(TAG, "Unsupported touch model: %d", touch->info.model);
        return ESP_ERR_NOT_SUPPORTED;
    }
#if !WT_BSP_TOUCH_GT911_ENABLED
    if (touch->info.model == WT_BSP_TOUCH_MODEL_GT911) {
        ESP_LOGE(TAG, "GT911 is not enabled for the selected display adapter");
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif
    if (touch->info.model == WT_BSP_TOUCH_MODEL_GT911 && (info->width < 2 || info->height < 2)) {
        ESP_LOGE(TAG, "Invalid GT911 display dimensions");
        return ESP_ERR_INVALID_ARG;
    }

    touch->i2c_bus = (i2c_master_bus_handle_t)info->i2c_bus_handle;
    if (touch->i2c_bus == NULL) {
        const i2c_master_bus_config_t bus_config = {
            .i2c_port = info->i2c_port,
            .sda_io_num = info->sda_pin,
            .scl_io_num = info->scl_pin,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .flags.enable_internal_pullup = true,
        };
        ret = i2c_new_master_bus(&bus_config, &touch->i2c_bus);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Create touch I2C bus failed: %s", esp_err_to_name(ret));
            goto err;
        }
        touch->owns_i2c_bus = true;
    }

    uint8_t address = 0;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
#if WT_BSP_TOUCH_GT911_ENABLED
    if (touch->info.model == WT_BSP_TOUCH_MODEL_GT911) {
        ret = wt_bsp_touch_probe_gt911(touch->i2c_bus, &address);
        if (ret != ESP_OK) {
            goto err;
        }
        io_config = (esp_lcd_panel_io_i2c_config_t)ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        io_config.dev_addr = address;
    }
#else
    (void)address;
#endif
    ret = esp_lcd_new_panel_io_i2c(touch->i2c_bus, &io_config, &touch->io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create touch panel IO failed: %s", esp_err_to_name(ret));
        goto err;
    }

    esp_lcd_touch_config_t config = {
        .x_max = info->width,
        .y_max = info->height,
        .rst_gpio_num = info->rst_pin,
        .int_gpio_num = info->int_pin,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = info->swap_xy,
            .mirror_x = info->mirror_x,
            .mirror_y = info->mirror_y,
        },
    };
#if WT_BSP_TOUCH_GT911_ENABLED
    if (touch->info.model == WT_BSP_TOUCH_MODEL_GT911) {
        ret = wt_bsp_touch_init_gt911(touch, address, &config);
    } else
#endif
    {
        ret = esp_lcd_touch_new_i2c_st7123(touch->io, &config, &touch->handle);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create touch controller failed: %s", esp_err_to_name(ret));
        goto err;
    }

    touch->is_initialized = true;
    ESP_LOGI(TAG, "Touch initialized: %ux%u", info->width, info->height);
    return ESP_OK;

err:
    {
        esp_err_t cleanup_ret = wt_bsp_touch_deinit(touch);
        if (cleanup_ret != ESP_OK) {
            ESP_LOGE(TAG, "Touch cleanup failed: %s", esp_err_to_name(cleanup_ret));
        }
    }
    return ret;
}

esp_err_t wt_bsp_touch_deinit(wt_bsp_touch_t touch)
{
    esp_err_t ret = ESP_OK;

    if (touch == NULL) {
        ESP_LOGE(TAG, "Touch is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (touch->lvgl_indev != NULL) {
        ret = lvgl_port_remove_touch(touch->lvgl_indev);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Remove LVGL touch failed: %s", esp_err_to_name(ret));
            return ret;
        }
        touch->lvgl_indev = NULL;
    }
    touch->is_initialized = false;
    if (touch->handle != NULL) {
        ret = esp_lcd_touch_del(touch->handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Delete touch controller failed: %s", esp_err_to_name(ret));
            return ret;
        }
        touch->handle = NULL;
    }
    if (touch->io != NULL) {
        ret = esp_lcd_panel_io_del(touch->io);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Delete touch panel IO failed: %s", esp_err_to_name(ret));
            return ret;
        }
        touch->io = NULL;
    }
    if (touch->owns_i2c_bus && touch->i2c_bus != NULL) {
        ret = i2c_del_master_bus(touch->i2c_bus);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Delete touch I2C bus failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    touch->i2c_bus = NULL;
    touch->owns_i2c_bus = false;
    free(touch->driver_data);
    touch->driver_data = NULL;
    memset(&touch->info, 0, sizeof(touch->info));
    return ESP_OK;
}

esp_err_t wt_bsp_touch_read(wt_bsp_touch_t touch)
{
    if (touch == NULL) {
        ESP_LOGE(TAG, "Touch is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!touch->is_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_lcd_touch_read_data(touch->handle);
}

bool wt_bsp_touch_get_coordinates(wt_bsp_touch_t touch, uint16_t *x, uint16_t *y,
                                  uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    if (point_num != NULL) {
        *point_num = 0;
    }
    if (touch == NULL || x == NULL || y == NULL || point_num == NULL || max_point_num == 0) {
        ESP_LOGE(TAG, "Invalid touch coordinate arguments");
        return false;
    }
    if (!touch->is_initialized || wt_bsp_touch_read(touch) != ESP_OK) {
        return false;
    }
    return esp_lcd_touch_get_coordinates(touch->handle, x, y, strength, point_num, max_point_num);
}

esp_lcd_touch_handle_t wt_bsp_touch_get_handle(wt_bsp_touch_t touch)
{
    return (touch != NULL && touch->is_initialized) ? touch->handle : NULL;
}

lv_indev_t *wt_bsp_touch_lvgl_start(wt_bsp_touch_t touch, lv_display_t *disp)
{
    if (touch == NULL || !touch->is_initialized || disp == NULL) {
        return NULL;
    }
    if (touch->lvgl_indev != NULL) {
        return touch->lvgl_indev;
    }

    const lvgl_port_touch_cfg_t config = {
        .disp = disp,
        .handle = touch->handle,
    };
    touch->lvgl_indev = lvgl_port_add_touch(&config);
    return touch->lvgl_indev;
}

/* ==================== [Static Functions] ================================== */

static wt_bsp_touch_model_t wt_bsp_touch_resolve_model(wt_bsp_touch_model_t model)
{
    if (model != WT_BSP_TOUCH_MODEL_AUTO) {
        return model;
    }
#if WT_BSP_TOUCH_GT911_ENABLED
    return WT_BSP_TOUCH_MODEL_GT911;
#else
    return WT_BSP_TOUCH_MODEL_ST7123;
#endif
}

#if WT_BSP_TOUCH_GT911_ENABLED
static esp_err_t wt_bsp_touch_probe_gt911(i2c_master_bus_handle_t bus, uint8_t *address)
{
    const uint8_t addresses[] = {ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP};

    for (size_t i = 0; i < sizeof(addresses) / sizeof(addresses[0]); ++i) {
        esp_err_t ret = i2c_master_probe(bus, addresses[i], WT_BSP_TOUCH_PROBE_TIMEOUT_MS);
        if (ret == ESP_OK) {
            *address = addresses[i];
            ESP_LOGI(TAG, "GT911 detected at 0x%02x", *address);
            return ESP_OK;
        }
        if (ret != ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "GT911 probe failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    ESP_LOGW(TAG, "GT911 not detected at 0x5D or 0x14");
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t wt_bsp_touch_init_gt911(wt_bsp_touch_t touch, uint8_t address, esp_lcd_touch_config_t *config)
{
    wt_bsp_touch_gt911_data_t *data = calloc(1, sizeof(*data));
    if (data == NULL) {
        return ESP_ERR_NO_MEM;
    }
    touch->driver_data = data;
    data->config.dev_addr = address;
    data->width = touch->info.width;
    data->height = touch->info.height;

    uint8_t product[3];
    esp_err_t ret = esp_lcd_panel_io_rx_param(touch->io, WT_BSP_TOUCH_GT911_PRODUCT_ID_REG, product, sizeof(product));
    if (ret != ESP_OK) {
        return ret;
    }
    if (memcmp(product, "911", sizeof(product)) != 0) {
        ESP_LOGE(TAG, "Unexpected GT911 product ID: %02x %02x %02x", product[0], product[1], product[2]);
        return ESP_ERR_NOT_SUPPORTED;
    }
    uint8_t factory[5];
    ret = esp_lcd_panel_io_rx_param(touch->io, WT_BSP_TOUCH_GT911_CONFIG_REG, factory, sizeof(factory));
    if (ret != ESP_OK) {
        return ret;
    }
    data->raw_width = (uint16_t)factory[1] | ((uint16_t)factory[2] << 8);
    data->raw_height = (uint16_t)factory[3] | ((uint16_t)factory[4] << 8);
    if (data->raw_width < 2 || data->raw_height < 2) {
        ESP_LOGE(TAG, "Invalid GT911 factory dimensions");
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* The first member is the official driver's address configuration. Keep
     * our scaling context in driver_data; the LVGL port owns config.user_data.
     */
    config->driver_data = &data->config;
    config->process_coordinates = wt_bsp_touch_scale_gt911;
    config->x_max = data->width - 1;
    config->y_max = data->height - 1;
    ret = esp_lcd_touch_new_i2c_gt911(touch->io, config, &touch->handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "GT911 factory size=%ux%u config=%u, display=%ux%u",
                 data->raw_width, data->raw_height, factory[0], data->width, data->height);
    }
    return ret;
}

static void wt_bsp_touch_scale_gt911(esp_lcd_touch_handle_t handle, uint16_t *x, uint16_t *y,
                                   uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    const wt_bsp_touch_gt911_data_t *data = (const wt_bsp_touch_gt911_data_t *)handle->config.driver_data;
    (void)strength;
    (void)max_point_num;

    for (uint8_t i = 0; i < *point_num; ++i) {
        uint16_t raw_x = x[i] < data->raw_width ? x[i] : data->raw_width - 1;
        uint16_t raw_y = y[i] < data->raw_height ? y[i] : data->raw_height - 1;
        x[i] = (uint32_t)raw_x * (data->width - 1) / (data->raw_width - 1);
        y[i] = (uint32_t)raw_y * (data->height - 1) / (data->raw_height - 1);
    }
}
#endif

#endif // WT_BSP_TOUCH_ENABLED
