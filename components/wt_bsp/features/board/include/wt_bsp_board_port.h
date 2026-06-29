/**
 * @file wt_bsp_board_port.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP board port 接口。
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_BOARD_PORT_H__
#define __WT_BSP_BOARD_PORT_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief 板卡信息对象。
 */
typedef struct wt_bsp_board_obj_t {
    /**
     * @brief 从初始化参数复制得到的板卡元数据。
     */
    wt_bsp_board_info_t info;

    /**
     * @brief 缓存的版本字符串，格式为 "V<major>.<minor>"。
     */
    char version_str[16];
} wt_bsp_board_obj_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 初始化板卡信息对象。
 *
 * @param[in,out] board 待初始化的板卡对象。
 * @param[in] info 需要复制到对象中的板卡信息。
 *
 * @return 成功时返回 ESP_OK。
 * @return 当 @p board 或 @p info 为 NULL 时返回 ESP_ERR_INVALID_ARG。
 */
esp_err_t wt_bsp_board_init(wt_bsp_board_t board, wt_bsp_board_info_t *info);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __WT_BSP_BOARD_PORT_H__
