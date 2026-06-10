/**
 * @file wt_bsp_dsi.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-05-20
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_internal.h"

#if WT_BSP_DSI_ENABLE_IS_ENABLED

#include <string.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_dev.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_ili9881c.h"
#include "esp_lcd_st7102.h"

/* ==================== [Defines] =========================================== */

#define WT_BSP_DSI_DEFAULT_LANE_NUM             2U
#define WT_BSP_DSI_DEFAULT_LANE_BITRATE_MBPS    1000U
#define WT_BSP_DSI_DEFAULT_DPI_FB_NUM           1U
#define WT_BSP_DSI_DEFAULT_PHY_LDO_CHANNEL      3
#define WT_BSP_DSI_DEFAULT_PHY_LDO_VOLTAGE_MV   2500
#define WT_BSP_DSI_DEFAULT_LEDC_CHANNEL         1
#define WT_BSP_DSI_DEFAULT_LEDC_TIMER           1
#define WT_BSP_DSI_BACKLIGHT_PWM_FREQ_HZ        5000U
#define WT_BSP_DSI_BACKLIGHT_DUTY_RES           LEDC_TIMER_10_BIT
#define WT_BSP_DSI_BACKLIGHT_DUTY_MAX           1023U

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static esp_err_t wt_bsp_dsi_resolve_info(wt_bsp_dsi_info_t *resolved, const wt_bsp_dsi_info_t *info);
static esp_err_t wt_bsp_dsi_validate_info(const wt_bsp_dsi_info_t *info);
static esp_err_t wt_bsp_dsi_init_backlight(wt_bsp_dsi_t dsi);
static esp_err_t wt_bsp_dsi_enable_phy_power(wt_bsp_dsi_t dsi);
static esp_err_t wt_bsp_dsi_new_panel(wt_bsp_dsi_t dsi);
static esp_err_t wt_bsp_dsi_new_ek79007_panel(wt_bsp_dsi_t dsi);
static esp_err_t wt_bsp_dsi_new_ili9881c_panel(wt_bsp_dsi_t dsi);
static esp_err_t wt_bsp_dsi_new_st7102_panel(wt_bsp_dsi_t dsi);
static esp_err_t wt_bsp_dsi_check_ready(wt_bsp_dsi_t dsi);
static esp_err_t wt_bsp_dsi_send_display_command(wt_bsp_dsi_t dsi, bool on);
static esp_err_t wt_bsp_dsi_cleanup(wt_bsp_dsi_t dsi);
static void wt_bsp_dsi_get_default_lvgl_config(wt_bsp_dsi_t dsi, wt_bsp_dsi_lvgl_config_t *config);
static lcd_color_rgb_pixel_format_t wt_bsp_dsi_get_lcd_pixel_format(wt_bsp_dsi_color_format_t color_format);
static uint32_t wt_bsp_dsi_get_color_format_bits_per_pixel(wt_bsp_dsi_color_format_t color_format);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_dsi";
static bool s_lvgl_port_ready = false;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_dsi_init(wt_bsp_dsi_t dsi, const wt_bsp_dsi_info_t *info)
{
    esp_err_t ret = ESP_OK;
    wt_bsp_dsi_info_t resolved = {0};

    if (dsi == NULL || info == NULL) {
        ESP_LOGE(TAG, "Invalid argument: dsi=%p, info=%p", dsi, info);
        return ESP_ERR_INVALID_ARG;
    }

    ret = wt_bsp_dsi_resolve_info(&resolved, info);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = wt_bsp_dsi_validate_info(&resolved);
    if (ret != ESP_OK) {
        return ret;
    }

    memset(dsi, 0, sizeof(*dsi));
    dsi->info = resolved;

    ESP_GOTO_ON_ERROR(wt_bsp_dsi_init_backlight(dsi), err, TAG, "init backlight failed");
    ESP_GOTO_ON_ERROR(wt_bsp_dsi_enable_phy_power(dsi), err, TAG, "enable DSI PHY power failed");

    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = dsi->info.dsi_lane_num,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = dsi->info.lane_bit_rate_mbps,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_dsi_bus(&bus_config, &dsi->mipi_dsi_bus), err, TAG, "new DSI bus failed");

    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_dbi(dsi->mipi_dsi_bus, &dbi_config, &dsi->io), err, TAG, "new panel IO failed");
    ESP_GOTO_ON_ERROR(wt_bsp_dsi_new_panel(dsi), err, TAG, "new LCD panel failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(dsi->panel), err, TAG, "LCD panel reset failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(dsi->panel), err, TAG, "LCD panel init failed");

    dsi->is_initialized = true;
    ESP_LOGI(TAG, "DSI display initialized: %ux%u, panel=%d, lanes=%u",
             dsi->info.width,
             dsi->info.height,
             dsi->info.panel_type,
             dsi->info.dsi_lane_num);

    return ESP_OK;

err:
    (void)wt_bsp_dsi_cleanup(dsi);
    return ret;
}

esp_err_t wt_bsp_dsi_deinit(wt_bsp_dsi_t dsi)
{
    if (dsi == NULL) {
        ESP_LOGE(TAG, "DSI is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    return wt_bsp_dsi_cleanup(dsi);
}

esp_err_t wt_bsp_dsi_display_on(wt_bsp_dsi_t dsi)
{
    if (dsi == NULL || dsi->panel == NULL) {
        ESP_LOGE(TAG, "Invalid DSI object");
        return ESP_ERR_INVALID_ARG;
    }

    return wt_bsp_dsi_send_display_command(dsi, true);
}

esp_err_t wt_bsp_dsi_display_off(wt_bsp_dsi_t dsi)
{
    if (dsi == NULL || dsi->panel == NULL) {
        ESP_LOGE(TAG, "Invalid DSI object");
        return ESP_ERR_INVALID_ARG;
    }

    return wt_bsp_dsi_send_display_command(dsi, false);
}

esp_err_t wt_bsp_dsi_set_brightness(wt_bsp_dsi_t dsi, uint8_t brightness)
{
    if (dsi == NULL) {
        ESP_LOGE(TAG, "DSI is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (dsi->info.backlight_gpio_num < 0) {
        return ESP_OK;
    }
    if (!dsi->brightness_initialized) {
        ESP_LOGE(TAG, "Backlight is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (brightness > 100U) {
        brightness = 100U;
    }

    uint32_t duty_cycle = (WT_BSP_DSI_BACKLIGHT_DUTY_MAX * brightness) / 100U;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE,
                                      (ledc_channel_t)dsi->info.ledc_channel,
                                      duty_cycle),
                        TAG,
                        "set backlight duty failed");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)dsi->info.ledc_channel);
}

esp_err_t wt_bsp_dsi_get_resolution(wt_bsp_dsi_t dsi, uint16_t *width, uint16_t *height)
{
    if (dsi == NULL) {
        ESP_LOGE(TAG, "DSI is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (width != NULL) {
        *width = dsi->info.width;
    }
    if (height != NULL) {
        *height = dsi->info.height;
    }

    return ESP_OK;
}

esp_lcd_panel_handle_t wt_bsp_dsi_get_panel_handle(wt_bsp_dsi_t dsi)
{
    return (dsi == NULL) ? NULL : dsi->panel;
}

lv_display_t *wt_bsp_dsi_lvgl_start(wt_bsp_dsi_t dsi, const wt_bsp_dsi_lvgl_config_t *config)
{
    wt_bsp_dsi_lvgl_config_t resolved = {0};

    if (wt_bsp_dsi_check_ready(dsi) != ESP_OK) {
        return NULL;
    }
    if (dsi->lvgl_display != NULL) {
        return dsi->lvgl_display;
    }

    if (config == NULL) {
        wt_bsp_dsi_get_default_lvgl_config(dsi, &resolved);
    } else {
        resolved = *config;
        if (resolved.buffer_size == 0U) {
            resolved.buffer_size = (uint32_t)dsi->info.width * WT_BSP_DSI_LVGL_DRAW_BUFF_LINES_DEFAULT;
        }
    }

    if (!s_lvgl_port_ready) {
        esp_err_t ret = lvgl_port_init(&resolved.lvgl_port_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LVGL port init failed: %s", esp_err_to_name(ret));
            return NULL;
        }
        dsi->lvgl_port_initialized = true;
        s_lvgl_port_ready = true;
    }

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = dsi->io,
        .panel_handle = dsi->panel,
        .control_handle = NULL,
        .buffer_size = resolved.buffer_size,
        .double_buffer = resolved.double_buffer,
        .hres = dsi->info.width,
        .vres = dsi->info.height,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = resolved.flags.buff_dma,
            .buff_spiram = resolved.flags.buff_spiram,
            .sw_rotate = resolved.flags.sw_rotate,
        },
    };
    const lvgl_port_display_dsi_cfg_t dsi_cfg = {
        .flags = {
            .avoid_tearing = resolved.flags.avoid_tearing,
        },
    };

    dsi->lvgl_display = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
    if (dsi->lvgl_display == NULL) {
        ESP_LOGE(TAG, "LVGL DSI display add failed");
    }

    return dsi->lvgl_display;
}

lv_display_t *wt_bsp_dsi_get_lvgl_display(wt_bsp_dsi_t dsi)
{
    return (dsi == NULL) ? NULL : dsi->lvgl_display;
}

bool wt_bsp_dsi_lvgl_lock(uint32_t timeout_ms)
{
    if (!s_lvgl_port_ready) {
        return false;
    }
    return lvgl_port_lock(timeout_ms);
}

void wt_bsp_dsi_lvgl_unlock(void)
{
    if (!s_lvgl_port_ready) {
        return;
    }
    lvgl_port_unlock();
}

/* ==================== [Static Functions] ================================== */

static esp_err_t wt_bsp_dsi_resolve_info(wt_bsp_dsi_info_t *resolved, const wt_bsp_dsi_info_t *info)
{
    *resolved = *info;

    if (resolved->dsi_lane_num == 0U) {
        resolved->dsi_lane_num = WT_BSP_DSI_DEFAULT_LANE_NUM;
    }
    if (resolved->lane_bit_rate_mbps == 0U) {
        resolved->lane_bit_rate_mbps = WT_BSP_DSI_DEFAULT_LANE_BITRATE_MBPS;
    }
    if (resolved->dpi_frame_buffer_num == 0U) {
        resolved->dpi_frame_buffer_num = WT_BSP_DSI_DEFAULT_DPI_FB_NUM;
    }
    if (resolved->phy_ldo_channel == 0) {
        resolved->phy_ldo_channel = WT_BSP_DSI_DEFAULT_PHY_LDO_CHANNEL;
    }
    if (resolved->phy_ldo_voltage_mv == 0) {
        resolved->phy_ldo_voltage_mv = WT_BSP_DSI_DEFAULT_PHY_LDO_VOLTAGE_MV;
    }
    if (resolved->ledc_channel < 0) {
        resolved->ledc_channel = WT_BSP_DSI_DEFAULT_LEDC_CHANNEL;
    }
    if (resolved->ledc_timer < 0) {
        resolved->ledc_timer = WT_BSP_DSI_DEFAULT_LEDC_TIMER;
    }

    if (resolved->panel_type == WT_BSP_DSI_PANEL_AUTO) {
        if ((resolved->width == 0U && resolved->height == 0U) ||
            (resolved->width == 1024U && resolved->height == 600U)) {
            resolved->panel_type = WT_BSP_DSI_PANEL_EK79007_1024_600;
        } else if (resolved->width == 800U && resolved->height == 1280U) {
            resolved->panel_type = WT_BSP_DSI_PANEL_ILI9881C_800_1280;
        } else if (resolved->width == 480U && resolved->height == 640U) {
            resolved->panel_type = WT_BSP_DSI_PANEL_ST7102_480_640;
        } else {
            ESP_LOGE(TAG, "Unsupported DSI panel resolution: %ux%u", resolved->width, resolved->height);
            return ESP_ERR_INVALID_ARG;
        }
    }

    if (resolved->panel_type == WT_BSP_DSI_PANEL_ST7102_480_640 && info->color_format == WT_BSP_DSI_COLOR_FORMAT_RGB565) {
        resolved->color_format = WT_BSP_DSI_COLOR_FORMAT_RGB888;
    }

    if (resolved->width == 0U || resolved->height == 0U) {
        if (resolved->panel_type == WT_BSP_DSI_PANEL_EK79007_1024_600) {
            resolved->width = 1024U;
            resolved->height = 600U;
        } else if (resolved->panel_type == WT_BSP_DSI_PANEL_ILI9881C_800_1280) {
            resolved->width = 800U;
            resolved->height = 1280U;
        } else if (resolved->panel_type == WT_BSP_DSI_PANEL_ST7102_480_640) {
            resolved->width = 480U;
            resolved->height = 640U;
        }
    }

    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_validate_info(const wt_bsp_dsi_info_t *info)
{
    if (info->backlight_gpio_num >= 0 && !GPIO_IS_VALID_OUTPUT_GPIO(info->backlight_gpio_num)) {
        ESP_LOGE(TAG, "Invalid backlight GPIO: %d", info->backlight_gpio_num);
        return ESP_ERR_INVALID_ARG;
    }
    if (info->reset_gpio_num >= 0 && !GPIO_IS_VALID_OUTPUT_GPIO(info->reset_gpio_num)) {
        ESP_LOGE(TAG, "Invalid reset GPIO: %d", info->reset_gpio_num);
        return ESP_ERR_INVALID_ARG;
    }
    if (info->dsi_lane_num != 2U) {
        ESP_LOGE(TAG, "Unsupported DSI lane count: %u", info->dsi_lane_num);
        return ESP_ERR_INVALID_ARG;
    }
    if (info->color_format > WT_BSP_DSI_COLOR_FORMAT_RGB888) {
        ESP_LOGE(TAG, "Unsupported color format: %d", info->color_format);
        return ESP_ERR_INVALID_ARG;
    }
    if (info->dpi_frame_buffer_num == 0U) {
        ESP_LOGE(TAG, "Invalid DPI frame buffer count");
        return ESP_ERR_INVALID_ARG;
    }

    switch (info->panel_type) {
    case WT_BSP_DSI_PANEL_EK79007_1024_600:
        if (info->width != 1024U || info->height != 600U) {
            ESP_LOGE(TAG, "EK79007 requires 1024x600, got %ux%u", info->width, info->height);
            return ESP_ERR_INVALID_ARG;
        }
        break;
    case WT_BSP_DSI_PANEL_ILI9881C_800_1280:
        if (info->width != 800U || info->height != 1280U) {
            ESP_LOGE(TAG, "ILI9881C requires 800x1280, got %ux%u", info->width, info->height);
            return ESP_ERR_INVALID_ARG;
        }
        break;
    case WT_BSP_DSI_PANEL_ST7102_480_640:
        if (info->width != 480U || info->height != 640U) {
            ESP_LOGE(TAG, "ST7102 requires 480x640, got %ux%u", info->width, info->height);
            return ESP_ERR_INVALID_ARG;
        }
        break;
    default:
        ESP_LOGE(TAG, "Unsupported DSI panel type: %d", info->panel_type);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_init_backlight(wt_bsp_dsi_t dsi)
{
    if (dsi->info.backlight_gpio_num < 0) {
        return ESP_OK;
    }

    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = WT_BSP_DSI_BACKLIGHT_DUTY_RES,
        .timer_num = (ledc_timer_t)dsi->info.ledc_timer,
        .freq_hz = WT_BSP_DSI_BACKLIGHT_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_config), TAG, "configure LEDC timer failed");

    ledc_channel_config_t channel_config = {
        .gpio_num = dsi->info.backlight_gpio_num,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)dsi->info.ledc_channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)dsi->info.ledc_timer,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel_config), TAG, "configure LEDC channel failed");

    dsi->brightness_initialized = true;
    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_enable_phy_power(wt_bsp_dsi_t dsi)
{
    if (dsi->info.phy_ldo_channel < 0) {
        return ESP_OK;
    }

    esp_ldo_channel_config_t ldo_config = {
        .chan_id = dsi->info.phy_ldo_channel,
        .voltage_mv = dsi->info.phy_ldo_voltage_mv,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_config, &dsi->phy_pwr_chan),
                        TAG,
                        "acquire DSI PHY LDO failed");
    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_new_panel(wt_bsp_dsi_t dsi)
{
    switch (dsi->info.panel_type) {
    case WT_BSP_DSI_PANEL_EK79007_1024_600:
        return wt_bsp_dsi_new_ek79007_panel(dsi);
    case WT_BSP_DSI_PANEL_ILI9881C_800_1280:
        return wt_bsp_dsi_new_ili9881c_panel(dsi);
    case WT_BSP_DSI_PANEL_ST7102_480_640:
        return wt_bsp_dsi_new_st7102_panel(dsi);
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

static esp_err_t wt_bsp_dsi_new_ek79007_panel(wt_bsp_dsi_t dsi)
{
    esp_lcd_dpi_panel_config_t dpi_config = EK79007_1024_600_PANEL_60HZ_CONFIG(
            wt_bsp_dsi_get_lcd_pixel_format(dsi->info.color_format));
    dpi_config.num_fbs = dsi->info.dpi_frame_buffer_num;

    ek79007_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = dsi->mipi_dsi_bus,
            .dpi_config = &dpi_config,
            .lane_num = dsi->info.dsi_lane_num,
        },
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = dsi->info.reset_gpio_num,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = wt_bsp_dsi_get_color_format_bits_per_pixel(dsi->info.color_format),
        .vendor_config = &vendor_config,
    };

    return esp_lcd_new_panel_ek79007(dsi->io, &panel_config, &dsi->panel);
}

static esp_err_t wt_bsp_dsi_new_ili9881c_panel(wt_bsp_dsi_t dsi)
{
    esp_lcd_dpi_panel_config_t dpi_config = ILI9881C_800_1280_PANEL_60HZ_DPI_CONFIG(
            wt_bsp_dsi_get_lcd_pixel_format(dsi->info.color_format));
    dpi_config.num_fbs = dsi->info.dpi_frame_buffer_num;

    ili9881c_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = dsi->mipi_dsi_bus,
            .dpi_config = &dpi_config,
            .lane_num = dsi->info.dsi_lane_num,
        },
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = dsi->info.reset_gpio_num,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = wt_bsp_dsi_get_color_format_bits_per_pixel(dsi->info.color_format),
        .vendor_config = &vendor_config,
    };

    return esp_lcd_new_panel_ili9881c(dsi->io, &panel_config, &dsi->panel);
}

static const st7102_lcd_init_cmd_t s_st7102_vendor_specific_init[] = {
//  {cmd, { data }, data_size, delay_ms}
    {0x28, (uint8_t []){0x00}, 0, 0},
    {0x10, (uint8_t []){0x00}, 0, 0},
    {0x99, (uint8_t []){0x71,0x02,0xa2}, 3, 0},
    {0x99, (uint8_t []){0x71,0x02,0xa3}, 3, 0},
    {0x99, (uint8_t []){0x71,0x02,0xa4}, 3, 0},
   	{0xA4, (uint8_t []){0x31}, 1, 0},
    {0xB0, (uint8_t []){0x22,0x43,0x1E,0x43,0x2F,0x57,0x57}, 7, 0},
    {0xB7, (uint8_t []){0x7D,0x7D}, 2, 0},
    {0xBF, (uint8_t []){0x7A,0x7A}, 2, 0},
    {0xC8, (uint8_t []){0x00,0x00,0x13,0x23,0x3E,0x00,0x6A,0x03,0xB0,0x06,0x11,0x0F,0x07,0x85,0x03,0x21,0xD5,0x01,0x18,0x00,0x22,0x56,0x0F,0x98,0x0A,0x32,0xF8,0x0D,0x48,0x0F,0xF3,0x80,0x0F,0xAC,0xC1,0x03,0xC4}, 37, 0},
    {0xC9, (uint8_t []){0x00,0x00,0x13,0x23,0x3E,0x00,0x6A,0x03,0xB0,0x06,0x11,0x0F,0x07,0x85,0x03,0x21,0xD5,0x01,0x18,0x00,0x22,0x56,0x0F,0x98,0x0A,0x32,0xF8,0x0D,0x48,0x0F,0xF3,0x80,0x0F,0xAC,0xC1,0x03,0xC4}, 37, 0},
    {0xD7, (uint8_t []){0x10,0x0C,0x02,0x19,0x40,0x40}, 6, 0},
    {0xA3, (uint8_t []){0x40,0x03,0x80,0xCF,0x44,0x00,0x00,0x00,0x02,0x05,0x6F,0x6F,0x00,0x1A,0x00,0x45,0x05,0x00,0x00,0x00,0x00,0x46,0x00,0x00,0x02,0x20,0x52,0x00,0x05,0x00,0x00,0xFF}, 32, 0},
    {0xA6, (uint8_t []){0x02,0x00,0x24,0x55,0x35,0x00,0x38,0x00,0x97,0x97,0x00,0x24,0x55,0x36,0x00,0x37,0x00,0x97,0x97,0x02,0xAC,0x51,0x3A,0x00,0x00,0x00,0x97,0x97,0x00,0xAC,0x21,0x00,0x0B,0x00,0x00,0x97,0x97,0x00,0x00,0x06,0x00,0x00,0x00,0x00}, 44, 0},
    {0xA7, (uint8_t []){0x19,0x19,0x00,0x64,0x40,0x07,0x16,0x40,0x00,0x04,0x03,0x97,0x97,0x00,0x64,0x40,0x25,0x34,0x00,0x00,0x02,0x01,0x97,0x97,0x00,0x64,0x40,0x4B,0x5A,0x00,0x00,0x02,0x01,0x97,0x97,0x00,0x24,0x40,0x69,0x78,0x00,0x00,0x00,0x00,0x97,0x97,0x00,0x44}, 48, 0},
    {0xAC, (uint8_t []){0x11,0x08,0x13,0x0A,0x18,0x1A,0x1B,0x00,0x06,0x03,0x19,0x1B,0x1B,0x1B,0x18,0x1B,0x10,0x09,0x12,0x0B,0x18,0x1A,0x1B,0x02,0x06,0x01,0x19,0x1B,0x1B,0x1B,0x18,0x1B,0xFF,0x67,0xFF,0x67,0x00}, 37, 0},
    {0xAD, (uint8_t []){0xCC,0x40,0x46,0x11,0x04,0x6F,0x6F}, 7, 0},
    {0xE8, (uint8_t []){0x30,0x07,0x00,0xB3,0xB3,0x9C,0x00,0xE2,0x04,0x00,0x00,0x00,0x00,0xEF}, 14, 0},
    {0x75, (uint8_t []){0x03,0x04}, 2, 0},
    {0xE7, (uint8_t []){0x8B,0x3C,0x00,0x0C,0xF0,0x5D,0x00,0x5D,0x00,0x5D,0x00,0x5D,0x00,0xFF,0x00,0x08,0x7B,0x00,0x00,0xC8,0x6A,0x5A,0x08,0x1A,0x3C,0x00,0x71,0x01,0x8C,0x01,0x7F,0xF0,0x22}, 33, 0},
    {0xE9, (uint8_t []){0x3C,0x7F,0x08,0x0D,0x1A,0x7A,0x22,0x1A,0x33}, 9, 0},
    {0x99, (uint8_t []){0x71,0x02,0x00}, 3, 0},  
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0x29, (uint8_t []){0x00}, 0, 20},
	{0x35, (uint8_t []){0x00}, 1, 0},
    {0x36, (uint8_t []){0x00}, 1, 0},
};

static esp_err_t wt_bsp_dsi_new_st7102_panel(wt_bsp_dsi_t dsi)
{
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 24,
        .in_color_format = wt_bsp_dsi_get_lcd_pixel_format(dsi->info.color_format),
        .video_timing = {
            .h_size = 480,
            .v_size = 640,
            .hsync_back_porch = 40,
            .hsync_pulse_width = 2,
            .hsync_front_porch = 40,
            .vsync_back_porch = 10,
            .vsync_pulse_width = 2,
            .vsync_front_porch = 145,
        },
    };
    dpi_config.num_fbs = dsi->info.dpi_frame_buffer_num;

    st7102_vendor_config_t vendor_config = {
        .init_cmds = s_st7102_vendor_specific_init,
        .init_cmds_size = sizeof(s_st7102_vendor_specific_init) / sizeof(st7102_lcd_init_cmd_t),
        .mipi_config = {
            .dsi_bus = dsi->mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
        .flags.use_mipi_interface = 1,
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = dsi->info.reset_gpio_num,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = wt_bsp_dsi_get_color_format_bits_per_pixel(dsi->info.color_format),
        .vendor_config = &vendor_config,
    };

    return esp_lcd_new_panel_st7102(dsi->io, &panel_config, &dsi->panel);
}

static esp_err_t wt_bsp_dsi_check_ready(wt_bsp_dsi_t dsi)
{
    if (dsi == NULL || !dsi->is_initialized || dsi->panel == NULL) {
        ESP_LOGE(TAG, "Invalid DSI object");
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_send_display_command(wt_bsp_dsi_t dsi, bool on)
{
    esp_err_t ret = esp_lcd_panel_disp_on_off(dsi->panel, on);
    if (ret == ESP_ERR_NOT_SUPPORTED && dsi->io != NULL) {
        ret = esp_lcd_panel_io_tx_param(dsi->io, on ? LCD_CMD_DISPON : LCD_CMD_DISPOFF, NULL, 0);
    }
    return ret;
}

static esp_err_t wt_bsp_dsi_cleanup(wt_bsp_dsi_t dsi)
{
    esp_err_t ret = ESP_OK;

    if (dsi->lvgl_display != NULL) {
        if (lvgl_port_lock(0)) {
            esp_err_t disp_ret = lvgl_port_remove_disp(dsi->lvgl_display);
            lvgl_port_unlock();
            if (disp_ret != ESP_OK) {
                ESP_LOGW(TAG, "remove LVGL display failed: %s", esp_err_to_name(disp_ret));
                ret = disp_ret;
            }
        } else {
            ESP_LOGW(TAG, "lock LVGL before display remove failed");
            ret = ESP_ERR_TIMEOUT;
        }
        dsi->lvgl_display = NULL;
    }
    if (dsi->lvgl_port_initialized) {
        esp_err_t lvgl_ret = lvgl_port_deinit();
        if (lvgl_ret != ESP_OK) {
            ESP_LOGW(TAG, "deinit LVGL port failed: %s", esp_err_to_name(lvgl_ret));
            ret = lvgl_ret;
        }
        dsi->lvgl_port_initialized = false;
        s_lvgl_port_ready = false;
    }

    if (dsi->brightness_initialized) {
        esp_err_t stop_ret = ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)dsi->info.ledc_channel, 0);
        if (stop_ret != ESP_OK) {
            ESP_LOGW(TAG, "stop backlight PWM failed: %s", esp_err_to_name(stop_ret));
            ret = stop_ret;
        }
        dsi->brightness_initialized = false;
    }

    if (dsi->panel != NULL) {
        esp_err_t panel_ret = esp_lcd_panel_del(dsi->panel);
        if (panel_ret != ESP_OK) {
            ESP_LOGW(TAG, "delete LCD panel failed: %s", esp_err_to_name(panel_ret));
            ret = panel_ret;
        }
        dsi->panel = NULL;
    }
    if (dsi->io != NULL) {
        esp_err_t io_ret = esp_lcd_panel_io_del(dsi->io);
        if (io_ret != ESP_OK) {
            ESP_LOGW(TAG, "delete panel IO failed: %s", esp_err_to_name(io_ret));
            ret = io_ret;
        }
        dsi->io = NULL;
    }
    if (dsi->mipi_dsi_bus != NULL) {
        esp_err_t bus_ret = esp_lcd_del_dsi_bus(dsi->mipi_dsi_bus);
        if (bus_ret != ESP_OK) {
            ESP_LOGW(TAG, "delete DSI bus failed: %s", esp_err_to_name(bus_ret));
            ret = bus_ret;
        }
        dsi->mipi_dsi_bus = NULL;
    }
    if (dsi->phy_pwr_chan != NULL) {
        esp_err_t ldo_ret = esp_ldo_release_channel(dsi->phy_pwr_chan);
        if (ldo_ret != ESP_OK) {
            ESP_LOGW(TAG, "release DSI PHY LDO failed: %s", esp_err_to_name(ldo_ret));
            ret = ldo_ret;
        }
        dsi->phy_pwr_chan = NULL;
    }

    dsi->is_initialized = false;
    return ret;
}

static void wt_bsp_dsi_get_default_lvgl_config(wt_bsp_dsi_t dsi, wt_bsp_dsi_lvgl_config_t *config)
{
    *config = (wt_bsp_dsi_lvgl_config_t) {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = (uint32_t)dsi->info.width * WT_BSP_DSI_LVGL_DRAW_BUFF_LINES_DEFAULT,
        .double_buffer = false,
        .flags = {
            .buff_dma = (dsi->info.color_format == WT_BSP_DSI_COLOR_FORMAT_RGB565),
            .buff_spiram = false,
            .sw_rotate = false,
            .avoid_tearing = false,
        },
    };
}

static lcd_color_rgb_pixel_format_t wt_bsp_dsi_get_lcd_pixel_format(wt_bsp_dsi_color_format_t color_format)
{
    return (color_format == WT_BSP_DSI_COLOR_FORMAT_RGB888) ? LCD_COLOR_PIXEL_FORMAT_RGB888 :
           LCD_COLOR_PIXEL_FORMAT_RGB565;
}

static uint32_t wt_bsp_dsi_get_color_format_bits_per_pixel(wt_bsp_dsi_color_format_t color_format)
{
    return (color_format == WT_BSP_DSI_COLOR_FORMAT_RGB888) ? 24U : 16U;
}

#endif // WT_BSP_DSI_ENABLE_IS_ENABLED
