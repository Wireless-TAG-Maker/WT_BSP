/**
 * @file wt_bsp_board.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-05-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include <stdio.h>
#include <string.h>
#include "wt_bsp_board.h"
#include "esp_log.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_board";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_board_init(wt_bsp_board_t board, wt_bsp_board_info_t *info)
{
    if (info == NULL) {
        ESP_LOGE(TAG, "Board info is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (board == NULL) {
        ESP_LOGE(TAG, "Board is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    board->info = *info;
    memset(board->version_str, 0, sizeof(board->version_str));
    snprintf(board->version_str, sizeof(board->version_str), "V%d.%d", info->version_major, info->version_minor);

    return ESP_OK;
}

const char *wt_bsp_board_get_name(wt_bsp_board_t board)
{
    if (board == NULL) {
        ESP_LOGE(TAG, "Board is NULL");
        return "";
    }
    return board->info.name;
}

const char *wt_bsp_board_get_version_str(wt_bsp_board_t board)
{
    if (board == NULL) {
        ESP_LOGE(TAG, "Board is NULL");
        return "V0.0";
    }
    return board->version_str;
}

uint16_t wt_bsp_board_get_version_major(wt_bsp_board_t board)
{
    if (board == NULL) {
        ESP_LOGE(TAG, "Board is NULL");
        return 0;
    }
    return board->info.version_major;
}

uint16_t wt_bsp_board_get_version_minor(wt_bsp_board_t board)
{
    if (board == NULL) {
        ESP_LOGE(TAG, "Board is NULL");
        return 0;
    }
    return board->info.version_minor;
}

/* ==================== [Static Functions] ================================== */
