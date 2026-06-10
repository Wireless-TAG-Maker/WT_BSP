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
#include "wt_bsp_sdmmc.h"
#include "wt_bsp_dsi.h"
#include "esp_log.h"
#include "driver/gpio.h"

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

#define BOARD_SDMMC_MOUNT_POINT "/sdcard"
#define BOARD_SDMMC_SLOT 0
#define BOARD_SDMMC_WIDTH 4
#define BOARD_SDMMC_CD_GPIO -1
#define BOARD_SDMMC_WP_GPIO -1
#define BOARD_SDMMC_CLK_GPIO 43
#define BOARD_SDMMC_CMD_GPIO 44
#define BOARD_SDMMC_D0_GPIO 39
#define BOARD_SDMMC_D1_GPIO 40
#define BOARD_SDMMC_D2_GPIO 41
#define BOARD_SDMMC_D3_GPIO 42

#define BOARD_DSI_BACKLIGHT_GPIO_NUM 26
#define BOARD_DSI_RESET_GPIO_NUM 27

#if CONFIG_WT_BSP_BOARD_WT9932P4_TINY_2_8CUN
#define BOARD_DSI_WIDTH 480
#define BOARD_DSI_HEIGHT 640
#define BOARD_DSI_PANEL_TYPE WT_BSP_DSI_PANEL_ST7102_480_640
#else
#define BOARD_DSI_WIDTH 1024
#define BOARD_DSI_HEIGHT 600
#define BOARD_DSI_PANEL_TYPE WT_BSP_DSI_PANEL_EK79007_1024_600
#endif

#define BOARD_DSI_LANE_NUM 2
#define BOARD_DSI_COLOR_FORMAT WT_BSP_DSI_COLOR_FORMAT_RGB565
#define BOARD_DSI_DPI_FB_NUM 1
#define BOARD_DSI_LANE_BITRATE_MBPS 1000
#define BOARD_DSI_PHY_LDO_CHANNEL 3
#define BOARD_DSI_PHY_LDO_VOLTAGE_MV 2500
#define BOARD_DSI_LEDC_CHANNEL 1
#define BOARD_DSI_LEDC_TIMER 1

#define BOARD_CSI_SCCB_SCL_PIN 8
#define BOARD_CSI_SCCB_SDA_PIN 7
#define BOARD_CSI_RESET_PIN -1
#define BOARD_CSI_PWDN_PIN -1
#define BOARD_CSI_WIDTH 1024
#define BOARD_CSI_HEIGHT 600
#define BOARD_CSI_FPS 30
#define BOARD_CSI_BUF_COUNT 3

#define BOARD_TOUCH_I2C_PORT 0
#define BOARD_TOUCH_I2C_SDA_PIN 7
#define BOARD_TOUCH_I2C_SCL_PIN 8
#define BOARD_TOUCH_RST_PIN -1
#define BOARD_TOUCH_INT_PIN -1

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static esp_err_t board_init(void);
static esp_err_t board_deinit(void);
static wt_bsp_board_t board_get_board(void);
static wt_bsp_button_t board_get_button(void);
static wt_bsp_rgb_t board_get_rgb(void);
static wt_bsp_sdmmc_t board_get_sdmmc(void);
static wt_bsp_dsi_t board_get_dsi(void);
static wt_bsp_csi_t board_get_csi(void);
static wt_bsp_touch_t board_get_touch(void);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "board";

static bool s_board_is_init = false;

static wt_bsp_interface_t s_bsp_interface = {
    .init = board_init,
    .deinit = board_deinit,
    .get_board = board_get_board,
    .get_button = board_get_button,
    .get_rgb = board_get_rgb,
    .get_sdmmc = board_get_sdmmc,
    .get_dsi = board_get_dsi,
    .get_csi = board_get_csi,
    .get_touch = board_get_touch,
};

static wt_bsp_board_obj_t s_bsp_board = {0};
static wt_bsp_button_obj_t s_bsp_button = {0};
static wt_bsp_rgb_obj_t s_bsp_rgb = {0};
static wt_bsp_sdmmc_obj_t s_bsp_sdmmc = {0};
static wt_bsp_dsi_obj_t s_bsp_dsi = {0};
static wt_bsp_csi_obj_t s_bsp_csi = {0};
static wt_bsp_touch_obj_t s_bsp_touch = {0};

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

    // Initialize SDMMC
    ret = wt_bsp_sdmmc_init(&s_bsp_sdmmc, &(wt_bsp_sdmmc_info_t) {
        .mount_point = BOARD_SDMMC_MOUNT_POINT,
        .slot = BOARD_SDMMC_SLOT,
        .width = BOARD_SDMMC_WIDTH,
        .cd_gpio = BOARD_SDMMC_CD_GPIO,
        .wp_gpio = BOARD_SDMMC_WP_GPIO,
        .clk_gpio = BOARD_SDMMC_CLK_GPIO,
        .cmd_gpio = BOARD_SDMMC_CMD_GPIO,
        .d0_gpio = BOARD_SDMMC_D0_GPIO,
        .d1_gpio = BOARD_SDMMC_D1_GPIO,
        .d2_gpio = BOARD_SDMMC_D2_GPIO,
        .d3_gpio = BOARD_SDMMC_D3_GPIO,
    });
    if (ret != ESP_OK) {
        wt_bsp_rgb_deinit(&s_bsp_rgb);
        wt_bsp_button_deinit(&s_bsp_button);
        ESP_LOGE(TAG, "Failed to initialize SDMMC: %s", esp_err_to_name(ret));
        return ret;
    }

    // Automatically mount SDMMC
    ret = wt_bsp_sdmmc_mount(&s_bsp_sdmmc);
    if (ret != ESP_OK) {
        wt_bsp_sdmmc_deinit(&s_bsp_sdmmc);
        wt_bsp_rgb_deinit(&s_bsp_rgb);
        wt_bsp_button_deinit(&s_bsp_button);
        ESP_LOGE(TAG, "Failed to mount SDMMC: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize DSI
    ret = wt_bsp_dsi_init(&s_bsp_dsi, &(wt_bsp_dsi_info_t) {
        .backlight_gpio_num = BOARD_DSI_BACKLIGHT_GPIO_NUM,
        .reset_gpio_num = BOARD_DSI_RESET_GPIO_NUM,
        .width = BOARD_DSI_WIDTH,
        .height = BOARD_DSI_HEIGHT,
        .dsi_lane_num = BOARD_DSI_LANE_NUM,
        .panel_type = BOARD_DSI_PANEL_TYPE,
        .color_format = BOARD_DSI_COLOR_FORMAT,
        .dpi_frame_buffer_num = BOARD_DSI_DPI_FB_NUM,
        .lane_bit_rate_mbps = BOARD_DSI_LANE_BITRATE_MBPS,
        .phy_ldo_channel = BOARD_DSI_PHY_LDO_CHANNEL,
        .phy_ldo_voltage_mv = BOARD_DSI_PHY_LDO_VOLTAGE_MV,
        .ledc_channel = BOARD_DSI_LEDC_CHANNEL,
        .ledc_timer = BOARD_DSI_LEDC_TIMER,
    });
    if (ret != ESP_OK) {
        wt_bsp_sdmmc_deinit(&s_bsp_sdmmc);
        wt_bsp_rgb_deinit(&s_bsp_rgb);
        wt_bsp_button_deinit(&s_bsp_button);
        ESP_LOGE(TAG, "Failed to initialize DSI: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize CSI
    /* 硬件限制：IO0 连接到了摄像头的 PWDN/LDO/RESET 引脚。
     * 正常使用摄像头前，必须将 IO0 拉高以启用摄像头供电及解除复位。 */
    gpio_config_t csi_pwr_conf = {
        .pin_bit_mask = (1ULL << 0),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&csi_pwr_conf);
    gpio_set_level(0, 1);

    ret = wt_bsp_csi_init(&s_bsp_csi, &(wt_bsp_csi_info_t) {
        .sccb_scl_pin = BOARD_CSI_SCCB_SCL_PIN,
        .sccb_sda_pin = BOARD_CSI_SCCB_SDA_PIN,
        .reset_pin = BOARD_CSI_RESET_PIN,
        .pwdn_pin = BOARD_CSI_PWDN_PIN,
        .width = BOARD_CSI_WIDTH,
        .height = BOARD_CSI_HEIGHT,
        .fps = BOARD_CSI_FPS,
        .buffer_count = BOARD_CSI_BUF_COUNT,
    });
    if (ret != ESP_OK) {
        wt_bsp_dsi_deinit(&s_bsp_dsi);
        wt_bsp_sdmmc_deinit(&s_bsp_sdmmc);
        wt_bsp_rgb_deinit(&s_bsp_rgb);
        wt_bsp_button_deinit(&s_bsp_button);
        ESP_LOGE(TAG, "Failed to initialize CSI: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize touch
    ret = wt_bsp_touch_init(&s_bsp_touch, &(wt_bsp_touch_info_t) {
        .i2c_port = BOARD_TOUCH_I2C_PORT,
        .scl_pin = BOARD_TOUCH_I2C_SCL_PIN,
        .sda_pin = BOARD_TOUCH_I2C_SDA_PIN,
        .rst_pin = BOARD_TOUCH_RST_PIN,
        .int_pin = BOARD_TOUCH_INT_PIN,
        .width = BOARD_DSI_WIDTH,
        .height = BOARD_DSI_HEIGHT,
    });
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize touch: %s", esp_err_to_name(ret));
        // Touch failure is not fatal
    }

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

    // Deinitialize touch
    ret = wt_bsp_touch_deinit(&s_bsp_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize touch: %s", esp_err_to_name(ret));
    }

    // Deinitialize CSI
    ret = wt_bsp_csi_deinit(&s_bsp_csi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize CSI: %s", esp_err_to_name(ret));
    }

    // Deinitialize DSI
    ret = wt_bsp_dsi_deinit(&s_bsp_dsi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize DSI: %s", esp_err_to_name(ret));
    }

    // Deinitialize SDMMC
    ret = wt_bsp_sdmmc_deinit(&s_bsp_sdmmc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize SDMMC: %s", esp_err_to_name(ret));
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

static wt_bsp_sdmmc_t board_get_sdmmc(void)
{
    return &s_bsp_sdmmc;
}

static wt_bsp_dsi_t board_get_dsi(void)
{
    return &s_bsp_dsi;
}

static wt_bsp_csi_t board_get_csi(void)
{
    return &s_bsp_csi;
}

static wt_bsp_touch_t board_get_touch(void)
{
    return &s_bsp_touch;
}
