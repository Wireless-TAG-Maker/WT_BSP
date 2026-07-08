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

#include "wt_bsp_dsi_port.h"

#if WT_BSP_DSI_ENABLED

#include <string.h>
#include <unistd.h>
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
#define WT_BSP_DSI_BACKLIGHT_PWM_FREQ_HZ        2000U
#define WT_BSP_DSI_BACKLIGHT_DUTY_RES           LEDC_TIMER_10_BIT
#define WT_BSP_DSI_BACKLIGHT_DUTY_MAX           1023U

#define ALIGN_UP(num, align)    (((num) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_DOWN(num, align)  ((num) & ~((align) - 1))

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
static esp_err_t wt_bsp_dsi_enable_dma2d(wt_bsp_dsi_t dsi);
static esp_err_t wt_bsp_dsi_check_ready(wt_bsp_dsi_t dsi);
static esp_err_t wt_bsp_dsi_send_display_command(wt_bsp_dsi_t dsi, bool on);
static esp_err_t wt_bsp_dsi_cleanup(wt_bsp_dsi_t dsi);
static void wt_bsp_dsi_get_default_lvgl_config(wt_bsp_dsi_t dsi, wt_bsp_dsi_lvgl_config_t *config);
static void wt_bsp_dsi_lvgl_rounder_cb(lv_event_t *e);
static lcd_color_format_t wt_bsp_dsi_get_lcd_color_format(wt_bsp_dsi_color_format_t color_format);
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
    dsi->is_initialized = false;

    /* Initialize backlight (non-critical) */
    ret = wt_bsp_dsi_init_backlight(dsi);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Backlight initialization failed: %s", esp_err_to_name(ret));
        /* Continue without backlight */
    }

    /* Enable DSI PHY power (non-critical) */
    ret = wt_bsp_dsi_enable_phy_power(dsi);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "DSI PHY power enable failed: %s", esp_err_to_name(ret));
        goto err;
    }

    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = dsi->info.dsi_lane_num,
        /* Let ESP-IDF select the default for the configured ESP32-P4 revision. */
        .phy_clk_src = 0,
        .lane_bit_rate_mbps = dsi->info.lane_bit_rate_mbps,
    };
    ret = esp_lcd_new_dsi_bus(&bus_config, &dsi->mipi_dsi_bus);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "DSI bus creation failed: %s", esp_err_to_name(ret));
        goto err;
    }

    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ret = esp_lcd_new_panel_io_dbi(dsi->mipi_dsi_bus, &dbi_config, &dsi->io);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Panel IO creation failed: %s", esp_err_to_name(ret));
        goto err;
    }

    ret = wt_bsp_dsi_new_panel(dsi);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LCD panel creation failed: %s", esp_err_to_name(ret));
        goto err;
    }

    ret = wt_bsp_dsi_enable_dma2d(dsi);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LCD panel DMA2D enable failed: %s", esp_err_to_name(ret));
    }

    ret = esp_lcd_panel_reset(dsi->panel);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LCD panel reset failed: %s", esp_err_to_name(ret));
        goto err;
    }

    ret = esp_lcd_panel_init(dsi->panel);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "LCD panel init failed: %s", esp_err_to_name(ret));
        goto err;
    }

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
        return ESP_ERR_INVALID_ARG;
    }
    return wt_bsp_dsi_cleanup(dsi);
}

esp_err_t wt_bsp_dsi_display_on(wt_bsp_dsi_t dsi)
{
    if (dsi == NULL || dsi->panel == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return wt_bsp_dsi_send_display_command(dsi, true);
}

esp_err_t wt_bsp_dsi_display_off(wt_bsp_dsi_t dsi)
{
    if (dsi == NULL || dsi->panel == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return wt_bsp_dsi_send_display_command(dsi, false);
}

esp_err_t wt_bsp_dsi_set_brightness(wt_bsp_dsi_t dsi, uint8_t brightness)
{
    if (dsi == NULL || !dsi->brightness_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (brightness > 100U) brightness = 100U;
    uint32_t duty = (WT_BSP_DSI_BACKLIGHT_DUTY_MAX * brightness) / 100U;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)dsi->info.ledc_channel, duty);
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)dsi->info.ledc_channel);
}

esp_err_t wt_bsp_dsi_get_resolution(wt_bsp_dsi_t dsi, uint16_t *width, uint16_t *height)
{
    if (dsi == NULL) return ESP_ERR_INVALID_ARG;
    if (width) *width = dsi->info.width;
    if (height) *height = dsi->info.height;
    return ESP_OK;
}

esp_lcd_panel_handle_t wt_bsp_dsi_get_panel_handle(wt_bsp_dsi_t dsi)
{
    return (dsi == NULL) ? NULL : dsi->panel;
}

lv_display_t *wt_bsp_dsi_lvgl_start(wt_bsp_dsi_t dsi, const wt_bsp_dsi_lvgl_config_t *config)
{
    wt_bsp_dsi_lvgl_config_t resolved = {0};

    if (wt_bsp_dsi_check_ready(dsi) != ESP_OK) return NULL;
    if (dsi->lvgl_display != NULL) return dsi->lvgl_display;

    if (config == NULL) {
        wt_bsp_dsi_get_default_lvgl_config(dsi, &resolved);
    } else {
        resolved = *config;
        if (resolved.buffer_size == 0U) {
            resolved.buffer_size = (uint32_t)dsi->info.width * WT_BSP_DSI_LVGL_DRAW_BUFF_LINES_DEFAULT;
        }
    }

    if (!s_lvgl_port_ready) {
        ESP_RETURN_ON_FALSE(lvgl_port_init(&resolved.lvgl_port_cfg) == ESP_OK, NULL, TAG, "LVGL port init failed");
        dsi->lvgl_port_initialized = true;
        s_lvgl_port_ready = true;
    }

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = dsi->io,
        .panel_handle = dsi->panel,
        .buffer_size = resolved.buffer_size,
        .double_buffer = resolved.double_buffer,
        .hres = dsi->info.width,
        .vres = dsi->info.height,
        .rotation = { .mirror_x = false, .mirror_y = false, .swap_xy = false },
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB888,
#endif
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = false,
            .full_refresh = resolved.flags.avoid_tearing,
        },
    };
    const lvgl_port_display_dsi_cfg_t dsi_cfg = {
        .flags = { .avoid_tearing = resolved.flags.avoid_tearing },
    };

    dsi->lvgl_display = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
    if (dsi->lvgl_display) {
        lv_display_add_event_cb(dsi->lvgl_display, wt_bsp_dsi_lvgl_rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    }

    return dsi->lvgl_display;
}

lv_display_t *wt_bsp_dsi_get_lvgl_display(wt_bsp_dsi_t dsi)
{
    return (dsi == NULL) ? NULL : dsi->lvgl_display;
}

bool wt_bsp_dsi_lvgl_lock(uint32_t timeout_ms)
{
    return s_lvgl_port_ready ? lvgl_port_lock(timeout_ms) : false;
}

void wt_bsp_dsi_lvgl_unlock(void)
{
    if (s_lvgl_port_ready) lvgl_port_unlock();
}

/* ==================== [Static Functions] ================================== */

static esp_err_t wt_bsp_dsi_resolve_info(wt_bsp_dsi_info_t *resolved, const wt_bsp_dsi_info_t *info)
{
    *resolved = *info;
    if (resolved->dsi_lane_num == 0U) resolved->dsi_lane_num = WT_BSP_DSI_DEFAULT_LANE_NUM;
    if (resolved->lane_bit_rate_mbps == 0U) resolved->lane_bit_rate_mbps = WT_BSP_DSI_DEFAULT_LANE_BITRATE_MBPS;
    if (resolved->dpi_frame_buffer_num == 0U) resolved->dpi_frame_buffer_num = WT_BSP_DSI_DEFAULT_DPI_FB_NUM;
    if (resolved->phy_ldo_channel == 0) resolved->phy_ldo_channel = WT_BSP_DSI_DEFAULT_PHY_LDO_CHANNEL;
    if (resolved->phy_ldo_voltage_mv == 0) resolved->phy_ldo_voltage_mv = WT_BSP_DSI_DEFAULT_PHY_LDO_VOLTAGE_MV;
    if (resolved->ledc_channel < 0) resolved->ledc_channel = WT_BSP_DSI_DEFAULT_LEDC_CHANNEL;
    if (resolved->ledc_timer < 0) resolved->ledc_timer = WT_BSP_DSI_DEFAULT_LEDC_TIMER;

    if (resolved->panel_type == WT_BSP_DSI_PANEL_AUTO) {
        if (resolved->width == 480U && resolved->height == 640U) resolved->panel_type = WT_BSP_DSI_PANEL_ST7102_480_640;
        else if (resolved->width == 800U && resolved->height == 1280U) resolved->panel_type = WT_BSP_DSI_PANEL_ILI9881C_800_1280;
        else resolved->panel_type = WT_BSP_DSI_PANEL_EK79007_1024_600;
    }
    if (resolved->width == 0U || resolved->height == 0U) {
        if (resolved->panel_type == WT_BSP_DSI_PANEL_ST7102_480_640) { resolved->width = 480; resolved->height = 640; }
        else if (resolved->panel_type == WT_BSP_DSI_PANEL_ILI9881C_800_1280) { resolved->width = 800; resolved->height = 1280; }
        else { resolved->width = 1024; resolved->height = 600; }
    }
    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_validate_info(const wt_bsp_dsi_info_t *info)
{
    if (info->dsi_lane_num != 2U) return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_init_backlight(wt_bsp_dsi_t dsi)
{
    if (dsi->info.backlight_gpio_num < 0) return ESP_OK;
    ledc_timer_config_t t_cfg = { .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = WT_BSP_DSI_BACKLIGHT_DUTY_RES, .timer_num = (ledc_timer_t)dsi->info.ledc_timer, .freq_hz = WT_BSP_DSI_BACKLIGHT_PWM_FREQ_HZ, .clk_cfg = LEDC_AUTO_CLK };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&t_cfg), TAG, "LEDC timer failed");
    ledc_channel_config_t c_cfg = { .gpio_num = dsi->info.backlight_gpio_num, .speed_mode = LEDC_LOW_SPEED_MODE, .channel = (ledc_channel_t)dsi->info.ledc_channel, .timer_sel = (ledc_timer_t)dsi->info.ledc_timer, .duty = 0, .hpoint = 0 };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&c_cfg), TAG, "LEDC channel failed");
    dsi->brightness_initialized = true;
    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_enable_phy_power(wt_bsp_dsi_t dsi)
{
    if (dsi->info.phy_ldo_channel < 0) return ESP_OK;
    esp_ldo_channel_config_t ldo = { .chan_id = dsi->info.phy_ldo_channel, .voltage_mv = dsi->info.phy_ldo_voltage_mv };
    return esp_ldo_acquire_channel(&ldo, &dsi->phy_pwr_chan);
}

static esp_err_t wt_bsp_dsi_new_panel(wt_bsp_dsi_t dsi)
{
    if (dsi->info.panel_type == WT_BSP_DSI_PANEL_ST7102_480_640) return wt_bsp_dsi_new_st7102_panel(dsi);
    if (dsi->info.panel_type == WT_BSP_DSI_PANEL_ILI9881C_800_1280) return wt_bsp_dsi_new_ili9881c_panel(dsi);
    return wt_bsp_dsi_new_ek79007_panel(dsi);
}

static esp_err_t wt_bsp_dsi_new_ek79007_panel(wt_bsp_dsi_t dsi)
{
    esp_lcd_dpi_panel_config_t dpi = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 52,
        .in_color_format = wt_bsp_dsi_get_lcd_color_format(dsi->info.color_format),
        .out_color_format = wt_bsp_dsi_get_lcd_color_format(dsi->info.color_format),
        .num_fbs = dsi->info.dpi_frame_buffer_num,
        .video_timing = {
            .h_size = 1024,
            .v_size = 600,
            .hsync_pulse_width = 10,
            .hsync_back_porch = 160,
            .hsync_front_porch = 160,
            .vsync_pulse_width = 1,
            .vsync_back_porch = 23,
            .vsync_front_porch = 12,
        },
    };
    ek79007_vendor_config_t vendor = { .mipi_config = { .dsi_bus = dsi->mipi_dsi_bus, .dpi_config = &dpi, .lane_num = dsi->info.dsi_lane_num } };
    esp_lcd_panel_dev_config_t dev = { .reset_gpio_num = dsi->info.reset_gpio_num, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, .bits_per_pixel = wt_bsp_dsi_get_color_format_bits_per_pixel(dsi->info.color_format), .vendor_config = &vendor };
    return esp_lcd_new_panel_ek79007(dsi->io, &dev, &dsi->panel);
}

static esp_err_t wt_bsp_dsi_new_ili9881c_panel(wt_bsp_dsi_t dsi)
{
    esp_lcd_dpi_panel_config_t dpi = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 80,
        .in_color_format = wt_bsp_dsi_get_lcd_color_format(dsi->info.color_format),
        .out_color_format = wt_bsp_dsi_get_lcd_color_format(dsi->info.color_format),
        .num_fbs = dsi->info.dpi_frame_buffer_num,
        .video_timing = {
            .h_size = 800,
            .v_size = 1280,
            .hsync_back_porch = 140,
            .hsync_pulse_width = 40,
            .hsync_front_porch = 40,
            .vsync_back_porch = 16,
            .vsync_pulse_width = 4,
            .vsync_front_porch = 16,
        },
    };
    ili9881c_vendor_config_t vendor = { .mipi_config = { .dsi_bus = dsi->mipi_dsi_bus, .dpi_config = &dpi, .lane_num = dsi->info.dsi_lane_num } };
    esp_lcd_panel_dev_config_t dev = { .reset_gpio_num = dsi->info.reset_gpio_num, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, .bits_per_pixel = wt_bsp_dsi_get_color_format_bits_per_pixel(dsi->info.color_format), .vendor_config = &vendor };
    return esp_lcd_new_panel_ili9881c(dsi->io, &dev, &dsi->panel);
}

static const st7102_lcd_init_cmd_t s_st7102_vendor_specific_init[] = {
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
    esp_lcd_dpi_panel_config_t dpi = {
        .virtual_channel = 0, .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT, .dpi_clock_freq_mhz = 24,
        .in_color_format = wt_bsp_dsi_get_lcd_color_format(dsi->info.color_format),
        .out_color_format = wt_bsp_dsi_get_lcd_color_format(dsi->info.color_format),
        .num_fbs = dsi->info.dpi_frame_buffer_num,
        .video_timing = { .h_size = 480, .v_size = 640, .hsync_back_porch = 40, .hsync_pulse_width = 2, .hsync_front_porch = 40, .vsync_back_porch = 10, .vsync_pulse_width = 2, .vsync_front_porch = 145 },
    };
    st7102_vendor_config_t vendor = { .init_cmds = s_st7102_vendor_specific_init, .init_cmds_size = sizeof(s_st7102_vendor_specific_init) / sizeof(st7102_lcd_init_cmd_t), .mipi_config = { .dsi_bus = dsi->mipi_dsi_bus, .dpi_config = &dpi }, .flags.use_mipi_interface = 1 };
    esp_lcd_panel_dev_config_t dev = { .reset_gpio_num = dsi->info.reset_gpio_num, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, .bits_per_pixel = wt_bsp_dsi_get_color_format_bits_per_pixel(dsi->info.color_format), .vendor_config = &vendor };
    return esp_lcd_new_panel_st7102(dsi->io, &dev, &dsi->panel);
}

static esp_err_t wt_bsp_dsi_enable_dma2d(wt_bsp_dsi_t dsi)
{
    if (dsi == NULL || dsi->panel == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_lcd_dpi_panel_enable_dma2d(dsi->panel);
}

static esp_err_t wt_bsp_dsi_check_ready(wt_bsp_dsi_t dsi)
{
    if (dsi == NULL || !dsi->is_initialized || dsi->panel == NULL) return ESP_ERR_INVALID_STATE;
    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_send_display_command(wt_bsp_dsi_t dsi, bool on)
{
    esp_err_t ret = esp_lcd_panel_disp_on_off(dsi->panel, on);
    if (ret == ESP_ERR_NOT_SUPPORTED && dsi->io != NULL) ret = esp_lcd_panel_io_tx_param(dsi->io, on ? LCD_CMD_DISPON : LCD_CMD_DISPOFF, NULL, 0);
    return ret;
}

static esp_err_t wt_bsp_dsi_cleanup(wt_bsp_dsi_t dsi)
{
    if (dsi->lvgl_display) {
        if (lvgl_port_lock(0)) { lvgl_port_remove_disp(dsi->lvgl_display); lvgl_port_unlock(); }
        dsi->lvgl_display = NULL;
    }
    if (dsi->lvgl_port_initialized) { lvgl_port_deinit(); dsi->lvgl_port_initialized = false; s_lvgl_port_ready = false; }
    if (dsi->brightness_initialized) { ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)dsi->info.ledc_channel, 0); dsi->brightness_initialized = false; }
    if (dsi->panel) { esp_lcd_panel_del(dsi->panel); dsi->panel = NULL; }
    if (dsi->io) { esp_lcd_panel_io_del(dsi->io); dsi->io = NULL; }
    if (dsi->mipi_dsi_bus) { esp_lcd_del_dsi_bus(dsi->mipi_dsi_bus); dsi->mipi_dsi_bus = NULL; }
    if (dsi->phy_pwr_chan) { esp_ldo_release_channel(dsi->phy_pwr_chan); dsi->phy_pwr_chan = NULL; }
    dsi->is_initialized = false;
    return ESP_OK;
}

static void wt_bsp_dsi_get_default_lvgl_config(wt_bsp_dsi_t dsi, wt_bsp_dsi_lvgl_config_t *config)
{
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = 16384;
    *config = (wt_bsp_dsi_lvgl_config_t) { .lvgl_port_cfg = port_cfg, .buffer_size = (uint32_t)dsi->info.width * WT_BSP_DSI_LVGL_DRAW_BUFF_LINES_DEFAULT, .double_buffer = false, .flags = { .buff_dma = true, .buff_spiram = true, .sw_rotate = false, .avoid_tearing = false } };
}

static void wt_bsp_dsi_lvgl_rounder_cb(lv_event_t *e)
{
    lv_area_t *area = lv_event_get_param(e);
    area->x1 = ALIGN_DOWN(area->x1, 16);
    area->x2 = ALIGN_UP(area->x2, 16) - 1;
}

static lcd_color_format_t wt_bsp_dsi_get_lcd_color_format(wt_bsp_dsi_color_format_t color_format)
{
    return (color_format == WT_BSP_DSI_COLOR_FORMAT_RGB888) ? LCD_COLOR_FMT_RGB888 : LCD_COLOR_FMT_RGB565;
}

static uint32_t wt_bsp_dsi_get_color_format_bits_per_pixel(wt_bsp_dsi_color_format_t color_format)
{
    return (color_format == WT_BSP_DSI_COLOR_FORMAT_RGB888) ? 24U : 16U;
}

#endif // WT_BSP_DSI_ENABLED
