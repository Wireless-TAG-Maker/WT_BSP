/**
 * @file board.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-05-13
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "board.h"

#include "wt_bsp_board.h"
#include "wt_bsp_button.h"
#include "wt_bsp_rgb.h"
#include "wt_bsp_lcd.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"

#if CONFIG_WT_BSP_LCD_ILI9881C
#include "esp_lcd_ili9881c.h"
#elif CONFIG_WT_BSP_LCD_EK79007
#include "esp_lcd_ek79007.h"
#endif

/* ==================== [Defines] =========================================== */

#define BOARD_VERSION_MAJOR 0
#define BOARD_VERSION_MINOR 1

#define BOARD_NAME "WT9932P4-TINY"

#define BOARD_BUTTON_GPIO_NUM 35
#define BOARD_BUTTON_ACTIVE_LEVEL WT_BSP_BUTTON_ACTIVE_LOW

#define BOARD_RGB_GPIO_NUM 51
#define BOARD_RGB_MODEL WT_BSP_RGB_MODEL_WS2812
#define BOARD_RGB_FORMAT WT_BSP_RGB_FORMAT_GRB
#define BOARD_RGB_INVERT_OUT false

#if CONFIG_WT_BSP_LCD_ILI9881C
#define BOARD_MIPI_DSI_DPI_CLK_MHZ  80
#define BOARD_MIPI_DSI_LCD_H_RES    800
#define BOARD_MIPI_DSI_LCD_V_RES    1280
#define BOARD_MIPI_DSI_LCD_HSYNC    40
#define BOARD_MIPI_DSI_LCD_HBP      140
#define BOARD_MIPI_DSI_LCD_HFP      40
#define BOARD_MIPI_DSI_LCD_VSYNC    4
#define BOARD_MIPI_DSI_LCD_VBP      16
#define BOARD_MIPI_DSI_LCD_VFP      16
#elif CONFIG_WT_BSP_LCD_EK79007
#define BOARD_MIPI_DSI_DPI_CLK_MHZ  48
#define BOARD_MIPI_DSI_LCD_H_RES    1024
#define BOARD_MIPI_DSI_LCD_V_RES    600
#define BOARD_MIPI_DSI_LCD_HSYNC    10
#define BOARD_MIPI_DSI_LCD_HBP      120
#define BOARD_MIPI_DSI_LCD_HFP      120
#define BOARD_MIPI_DSI_LCD_VSYNC    1
#define BOARD_MIPI_DSI_LCD_VBP      20
#define BOARD_MIPI_DSI_LCD_VFP      10
#endif

#define BOARD_MIPI_DSI_LANE_NUM          2
#define BOARD_MIPI_DSI_LANE_BITRATE_MBPS 1000

#define BOARD_MIPI_DSI_PHY_PWR_LDO_CHAN       3
#define BOARD_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV 2500
#define BOARD_LCD_BK_LIGHT_ON_LEVEL           1
#define BOARD_PIN_NUM_BK_LIGHT                26
#define BOARD_PIN_NUM_LCD_RST                 27

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static esp_err_t board_init(void);
static esp_err_t board_deinit(void);
static wt_bsp_board_t board_get_board(void);
static wt_bsp_button_t board_get_button(void);
static wt_bsp_rgb_t board_get_rgb(void);
static wt_bsp_lcd_t board_get_lcd(void);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "board";

static bool s_board_is_init = false;

static wt_bsp_interface_t s_bsp_interface = {
    .init = board_init,
    .deinit = board_deinit,
    .get_board = board_get_board,
    .get_button = board_get_button,
    .get_rgb = board_get_rgb,
    .get_lcd = board_get_lcd,
};

static wt_bsp_board_obj_t s_bsp_board = {0};
static wt_bsp_button_obj_t s_bsp_button = {0};
static wt_bsp_rgb_obj_t s_bsp_rgb = {0};
static wt_bsp_lcd_t s_bsp_lcd = NULL;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

wt_bsp_interface_t *board_get_bsp_interface(void)
{
    return &s_bsp_interface;
}

/* ==================== [Static Functions] ================================== */

static esp_err_t board_init(void)
{
    esp_err_t ret = ESP_OK;

    if (s_board_is_init) {
        ESP_LOGW(TAG, "Board is already initialized");
        return ESP_OK;
    }

    // Initialize board
    wt_bsp_board_info_t board_info = {
        .name = BOARD_NAME,
        .version_major = BOARD_VERSION_MAJOR,
        .version_minor = BOARD_VERSION_MINOR,
    };

    ret = wt_bsp_board_init(&s_bsp_board, &board_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize board: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize button
    ret = wt_bsp_button_init(&s_bsp_button, &(wt_bsp_button_info_t) {
        .gpio_num = BOARD_BUTTON_GPIO_NUM,
        .active_level = BOARD_BUTTON_ACTIVE_LEVEL,
    });
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize button: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize RGB
    ret = wt_bsp_rgb_init(&s_bsp_rgb, &(wt_bsp_rgb_info_t) {
        .gpio_num = BOARD_RGB_GPIO_NUM,
        .model = BOARD_RGB_MODEL,
        .format = BOARD_RGB_FORMAT,
        .invert_out = BOARD_RGB_INVERT_OUT,
    });
    if (ret != ESP_OK) {
        wt_bsp_button_deinit(&s_bsp_button);
        ESP_LOGE(TAG, "Failed to initialize RGB: %s", esp_err_to_name(ret));
        return ret;
    }

#if WT_BSP_LCD_ENABLE_IS_ENABLED
    // Turn on the power for MIPI DSI PHY
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = BOARD_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = BOARD_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    ret = esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire LDO channel for MIPI DSI PHY: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize backlight
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BOARD_PIN_NUM_BK_LIGHT
    };
    gpio_config(&bk_gpio_config);
    gpio_set_level(BOARD_PIN_NUM_BK_LIGHT, !BOARD_LCD_BK_LIGHT_ON_LEVEL);

    // Create MIPI DSI bus
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = BOARD_MIPI_DSI_LANE_NUM,
        .lane_bit_rate_mbps = BOARD_MIPI_DSI_LANE_BITRATE_MBPS,
    };
    ret = esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create MIPI DSI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Install MIPI DSI LCD control IO
    esp_lcd_panel_io_handle_t mipi_dbi_io;
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ret = esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create MIPI DBI IO: %s", esp_err_to_name(ret));
        return ret;
    }

    // Install MIPI DSI LCD data panel
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = BOARD_MIPI_DSI_DPI_CLK_MHZ,
        .in_color_format = LCD_COLOR_FMT_RGB888,
        .video_timing = {
            .h_size = BOARD_MIPI_DSI_LCD_H_RES,
            .v_size = BOARD_MIPI_DSI_LCD_V_RES,
            .hsync_back_porch = BOARD_MIPI_DSI_LCD_HBP,
            .hsync_pulse_width = BOARD_MIPI_DSI_LCD_HSYNC,
            .hsync_front_porch = BOARD_MIPI_DSI_LCD_HFP,
            .vsync_back_porch = BOARD_MIPI_DSI_LCD_VBP,
            .vsync_pulse_width = BOARD_MIPI_DSI_LCD_VSYNC,
            .vsync_front_porch = BOARD_MIPI_DSI_LCD_VFP,
        },
    };

#if CONFIG_WT_BSP_LCD_ILI9881C
    ili9881c_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
            .lane_num = BOARD_MIPI_DSI_LANE_NUM,
        },
    };
    esp_lcd_panel_dev_config_t lcd_dev_config = {
        .reset_gpio_num = BOARD_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 24,
        .vendor_config = &vendor_config,
    };
    ret = esp_lcd_new_panel_ili9881c(mipi_dbi_io, &lcd_dev_config, &s_bsp_lcd);
#elif CONFIG_WT_BSP_LCD_EK79007
    ek79007_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    esp_lcd_panel_dev_config_t lcd_dev_config = {
        .reset_gpio_num = BOARD_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 24,
        .vendor_config = &vendor_config,
    };
    ret = esp_lcd_new_panel_ek79007(mipi_dbi_io, &lcd_dev_config, &s_bsp_lcd);
#endif

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LCD panel: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_lcd_panel_reset(s_bsp_lcd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset LCD panel: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_lcd_panel_init(s_bsp_lcd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init LCD panel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Turn on backlight
    gpio_set_level(BOARD_PIN_NUM_BK_LIGHT, BOARD_LCD_BK_LIGHT_ON_LEVEL);
#endif

    s_board_is_init = true;

    return ESP_OK;
}

static esp_err_t board_deinit(void)
{
    esp_err_t ret = ESP_OK;
    if (!s_board_is_init) {
        ESP_LOGW(TAG, "Board is not initialized");
        return ESP_OK;
    }

    // Deinitialize RGB
    ret = wt_bsp_rgb_deinit(&s_bsp_rgb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize RGB: %s", esp_err_to_name(ret));
    }

    // Deinitialize button

    ret = wt_bsp_button_deinit(&s_bsp_button);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize button: %s", esp_err_to_name(ret));
    }

    s_board_is_init = false;

    return ESP_OK;
}

static wt_bsp_board_t board_get_board(void)
{
    return &s_bsp_board;
}

static wt_bsp_button_t board_get_button(void)
{
    return &s_bsp_button;
}

static wt_bsp_rgb_t board_get_rgb(void)
{
    return &s_bsp_rgb;
}

static wt_bsp_lcd_t board_get_lcd(void)
{
    return s_bsp_lcd;
}
