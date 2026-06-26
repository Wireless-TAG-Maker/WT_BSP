/**
 * @file wt_bsp.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-05-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_port.h"
#include "esp_log.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp";
static wt_bsp_obj_t g_bsp = {0};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_init(void)
{
    wt_bsp_interface_t * bsp_interface = board_get_bsp_interface();
    if (bsp_interface == NULL) {
        ESP_LOGE(TAG, "Failed to get BSP interface");
        return ESP_ERR_INVALID_ARG;
    }

    g_bsp.interface = bsp_interface;
    return bsp_interface->init();
}

esp_err_t wt_bsp_deinit(void)
{
    if ( g_bsp.interface == NULL || g_bsp.interface->deinit == NULL) {
        ESP_LOGE(TAG, "BSP is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = g_bsp.interface->deinit();
    g_bsp.interface = NULL;
    return ret;
}

wt_bsp_board_t wt_bsp_get_board(void)
{
    if ( g_bsp.interface == NULL || g_bsp.interface->get_board == NULL) {
        ESP_LOGE(TAG, "BSP is NULL");
        return NULL;
    }
    return g_bsp.interface->get_board();
}

wt_bsp_button_t wt_bsp_get_button(void)
{
    if ( g_bsp.interface == NULL || g_bsp.interface->get_button == NULL) {
        ESP_LOGE(TAG, "BSP is NULL");
        return NULL;
    }
    return g_bsp.interface->get_button();
}

wt_bsp_rgb_t wt_bsp_get_rgb(void)
{
    if ( g_bsp.interface == NULL || g_bsp.interface->get_rgb == NULL) {
        ESP_LOGE(TAG, "BSP is NULL");
        return NULL;
    }
    return g_bsp.interface->get_rgb();
}

wt_bsp_sdmmc_t wt_bsp_get_sdmmc(void)
{
    if ( g_bsp.interface == NULL || g_bsp.interface->get_sdmmc == NULL) {
        ESP_LOGE(TAG, "BSP is NULL");
        return NULL;
    }
    return g_bsp.interface->get_sdmmc();
}

const char *wt_bsp_get_sdmmc_mount_point(void)
{
#if WT_BSP_SDMMC_ENABLED
    wt_bsp_sdmmc_t sdmmc = wt_bsp_get_sdmmc();
    if (sdmmc == NULL) {
        return "";
    }
    return wt_bsp_sdmmc_get_mount_point(sdmmc);
#else
    return "";
#endif
}

wt_bsp_dsi_t wt_bsp_get_dsi(void)
{
    if ( g_bsp.interface == NULL || g_bsp.interface->get_dsi == NULL) {
        ESP_LOGE(TAG, "BSP is NULL");
        return NULL;
    }
    return g_bsp.interface->get_dsi();
}

wt_bsp_csi_t wt_bsp_get_csi(void)
{
    if ( g_bsp.interface == NULL || g_bsp.interface->get_csi == NULL) {
        ESP_LOGE(TAG, "BSP is NULL");
        return NULL;
    }
    return g_bsp.interface->get_csi();
}

wt_bsp_touch_t wt_bsp_get_touch(void)
{
    if ( g_bsp.interface == NULL || g_bsp.interface->get_touch == NULL) {
        ESP_LOGE(TAG, "BSP is NULL");
        return NULL;
    }
    return g_bsp.interface->get_touch();
}


/* ==================== [Static Functions] ================================== */
