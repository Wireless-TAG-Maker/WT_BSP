/**
 * @file board.c
 * @author Wireless-Tag
 * @brief WT9932S31-TINY resource ownership and lifecycle.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "board.h"

#include "esp_log.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static esp_err_t board_init(void);
static esp_err_t board_deinit(void);
static wt_bsp_board_t board_get_board(void);
static wt_bsp_button_t board_get_button(void);
static wt_bsp_rgb_t board_get_rgb(void);
static wt_bsp_sdmmc_t board_get_sdmmc(void);
static wt_bsp_usb_device_cdc_t board_get_usb_device_cdc(void);
static wt_bsp_camera_t board_get_camera(void);
#if WT_BSP_BUTTON_ENABLED || WT_BSP_RGB_ENABLED || WT_BSP_SDMMC_ENABLED || WT_BSP_USB_DEVICE_CDC_ENABLED || WT_BSP_USB_DEVICE_UVC_ENABLED
static void board_record_cleanup_error(esp_err_t ret, const char *resource, esp_err_t *result);
#endif

/* ==================== [Static Variables] ================================== */

static const char *TAG = "board";
static bool s_board_is_init;
static wt_bsp_board_obj_t s_bsp_board;
#if WT_BSP_BUTTON_ENABLED
static wt_bsp_button_obj_t s_bsp_button;
#endif
#if WT_BSP_RGB_ENABLED
static wt_bsp_rgb_obj_t s_bsp_rgb;
#endif
#if WT_BSP_SDMMC_ENABLED
static wt_bsp_sdmmc_obj_t s_bsp_sdmmc;
#endif
#if WT_BSP_USB_DEVICE_CDC_ENABLED
static wt_bsp_usb_device_cdc_obj_t s_bsp_usb_device_cdc;
#endif
#if WT_BSP_CAMERA_ENABLED
static wt_bsp_camera_obj_t s_bsp_camera;
#endif

static wt_bsp_interface_t s_bsp_interface = {
    .init = board_init,
    .deinit = board_deinit,
    .get_board = board_get_board,
    .get_button = board_get_button,
    .get_rgb = board_get_rgb,
    .get_sdmmc = board_get_sdmmc,
    .get_dsi = NULL,
    .get_csi = NULL,
    .get_touch = NULL,
    .get_usb_device_cdc = board_get_usb_device_cdc,
    .get_camera = board_get_camera,
};

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
    ret = wt_bsp_board_init(&s_bsp_board, &(wt_bsp_board_info_t) {
        .name = "WT9932S31-TINY",
        .version_major = 1,
        .version_minor = 1,
    });
    if (ret != ESP_OK) {
        goto fail;
    }
#if WT_BSP_BUTTON_ENABLED
    ret = wt_bsp_button_init(&s_bsp_button, &(wt_bsp_button_info_t) {
        .gpio_num = BOARD_BUTTON_GPIO_NUM,
        .active_level = WT_BSP_BUTTON_ACTIVE_LOW,
    });
    if (ret != ESP_OK) {
        goto fail;
    }
#endif
#if WT_BSP_RGB_ENABLED
    ret = wt_bsp_rgb_init(&s_bsp_rgb, &(wt_bsp_rgb_info_t) {
        .gpio_num = BOARD_RGB_GPIO_NUM,
        .model = WT_BSP_RGB_MODEL_WS2812,
        .format = WT_BSP_RGB_FORMAT_GRB,
        .invert_out = false,
    });
    if (ret != ESP_OK) {
        goto fail;
    }
#endif
#if WT_BSP_SDMMC_ENABLED
    ret = wt_bsp_sdmmc_init(&s_bsp_sdmmc, &(wt_bsp_sdmmc_info_t) {
        .mount_point = "/sdcard",
        .slot = BOARD_SDMMC_SLOT,
        .width = 4,
        .cd_gpio = -1,
        .wp_gpio = -1,
        .clk_gpio = BOARD_SDMMC_CLK_GPIO,
        .cmd_gpio = BOARD_SDMMC_CMD_GPIO,
        .d0_gpio = BOARD_SDMMC_D0_GPIO,
        .d1_gpio = BOARD_SDMMC_D1_GPIO,
        .d2_gpio = BOARD_SDMMC_D2_GPIO,
        .d3_gpio = BOARD_SDMMC_D3_GPIO,
        .use_on_chip_ldo = true,
        .ldo_chan_id = BOARD_SDMMC_LDO_CHANNEL,
        .ldo_voltage_mv = 1800,
    });
    if (ret != ESP_OK) {
        goto fail;
    }
#endif
#if WT_BSP_USB_DEVICE_CDC_ENABLED
    ret = wt_bsp_usb_device_cdc_init(&s_bsp_usb_device_cdc, &(wt_bsp_usb_device_cdc_info_t) {
        .port = 0,
    });
    if (ret != ESP_OK) {
        goto fail;
    }
#endif
#if WT_BSP_CAMERA_ENABLED
    ret = wt_bsp_camera_init(&s_bsp_camera, &(wt_bsp_camera_info_t) {
        .prepare = board_camera_prepare,
        .capture = board_camera_capture,
        .get_status = board_camera_get_status,
    });
    if (ret != ESP_OK) {
        goto fail;
    }
#endif
#if WT_BSP_USB_DEVICE_UVC_ENABLED
    ret = board_usb_device_uvc_init();
    if (ret != ESP_OK) {
        goto fail;
    }
#endif
    s_board_is_init = true;
    return ESP_OK;

fail:
    ESP_LOGE(TAG, "Board initialization failed: %s", esp_err_to_name(ret));
    board_deinit();
    return ret;
}

static esp_err_t board_deinit(void)
{
    esp_err_t result = ESP_OK;

    /* Cleanup also accepts partially initialized objects. Keep releasing
     * independent resources when a previous cleanup reports an error. */
#if WT_BSP_USB_DEVICE_UVC_ENABLED
    board_record_cleanup_error(board_usb_device_uvc_deinit(), "UVC", &result);
#endif
#if WT_BSP_CAMERA_ENABLED
    board_record_cleanup_error(wt_bsp_camera_deinit(&s_bsp_camera), "camera", &result);
#endif
#if WT_BSP_USB_DEVICE_CDC_ENABLED
    board_record_cleanup_error(wt_bsp_usb_device_cdc_deinit(&s_bsp_usb_device_cdc), "CDC", &result);
#endif
#if WT_BSP_SDMMC_ENABLED
    if (s_bsp_sdmmc.is_initialized) {
        board_record_cleanup_error(wt_bsp_sdmmc_deinit(&s_bsp_sdmmc), "SDMMC", &result);
    }
#endif
#if WT_BSP_RGB_ENABLED
    if (s_bsp_rgb.strip != NULL) {
        board_record_cleanup_error(wt_bsp_rgb_deinit(&s_bsp_rgb), "RGB", &result);
    }
#endif
#if WT_BSP_BUTTON_ENABLED
    if (s_bsp_button.handle != NULL) {
        board_record_cleanup_error(wt_bsp_button_deinit(&s_bsp_button), "button", &result);
    }
#endif
    s_board_is_init = false;
    return result;
}

#if WT_BSP_BUTTON_ENABLED || WT_BSP_RGB_ENABLED || WT_BSP_SDMMC_ENABLED || WT_BSP_USB_DEVICE_CDC_ENABLED || WT_BSP_USB_DEVICE_UVC_ENABLED
static void board_record_cleanup_error(esp_err_t ret, const char *resource, esp_err_t *result)
{
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize %s: %s", resource, esp_err_to_name(ret));
        if (*result == ESP_OK) {
            *result = ret;
        }
    }
}

#endif

static wt_bsp_board_t board_get_board(void)
{
    return s_board_is_init ? &s_bsp_board : NULL;
}

static wt_bsp_camera_t board_get_camera(void)
{
#if WT_BSP_CAMERA_ENABLED
    return s_board_is_init && s_bsp_camera.is_initialized ? &s_bsp_camera : NULL;
#else
    return NULL;
#endif
}

static wt_bsp_button_t board_get_button(void)
{
#if WT_BSP_BUTTON_ENABLED
    return s_board_is_init && s_bsp_button.handle != NULL ? &s_bsp_button : NULL;
#else
    return NULL;
#endif
}

static wt_bsp_rgb_t board_get_rgb(void)
{
#if WT_BSP_RGB_ENABLED
    return s_board_is_init && s_bsp_rgb.strip != NULL ? &s_bsp_rgb : NULL;
#else
    return NULL;
#endif
}

static wt_bsp_sdmmc_t board_get_sdmmc(void)
{
#if WT_BSP_SDMMC_ENABLED
    return s_board_is_init && s_bsp_sdmmc.is_initialized ? &s_bsp_sdmmc : NULL;
#else
    return NULL;
#endif
}

static wt_bsp_usb_device_cdc_t board_get_usb_device_cdc(void)
{
#if WT_BSP_USB_DEVICE_CDC_ENABLED
    return s_board_is_init && s_bsp_usb_device_cdc.is_initialized ? &s_bsp_usb_device_cdc : NULL;
#else
    return NULL;
#endif
}
