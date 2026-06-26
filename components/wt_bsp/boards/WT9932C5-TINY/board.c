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

#include "esp_log.h"

/* ==================== [Defines] =========================================== */

#define BOARD_VERSION_MAJOR 0
#define BOARD_VERSION_MINOR 1

#define BOARD_NAME "WT9932C5-TINY"

#define BOARD_BUTTON_GPIO_NUM 28
#define BOARD_BUTTON_ACTIVE_LEVEL WT_BSP_BUTTON_ACTIVE_LOW

#define BOARD_RGB_GPIO_NUM 6
#define BOARD_RGB_MODEL WT_BSP_RGB_MODEL_WS2812
#define BOARD_RGB_FORMAT WT_BSP_RGB_FORMAT_GRB
#define BOARD_RGB_INVERT_OUT false

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static esp_err_t board_init(void);
static esp_err_t board_deinit(void);
static wt_bsp_board_t board_get_board(void);
static wt_bsp_button_t board_get_button(void);
static wt_bsp_rgb_t board_get_rgb(void);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "board";

static bool s_board_is_init = false;

static wt_bsp_interface_t s_bsp_interface = {
    .init = board_init,
    .deinit = board_deinit,
    .get_board = board_get_board,
    .get_button = board_get_button,
    .get_rgb = board_get_rgb,
};

static wt_bsp_board_obj_t s_bsp_board = {0};
static wt_bsp_button_obj_t s_bsp_button = {0};
static wt_bsp_rgb_obj_t s_bsp_rgb = {0};

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

    ret = wt_bsp_button_init(&s_bsp_button, &(wt_bsp_button_info_t) {
        .gpio_num = BOARD_BUTTON_GPIO_NUM,
        .active_level = BOARD_BUTTON_ACTIVE_LEVEL,
    });
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize button: %s", esp_err_to_name(ret));
        return ret;
    }

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

    ret = wt_bsp_rgb_deinit(&s_bsp_rgb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize RGB: %s", esp_err_to_name(ret));
    }

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
