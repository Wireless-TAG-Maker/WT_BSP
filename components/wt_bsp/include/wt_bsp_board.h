/**
 * @file wt_bsp_board.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP 组件的板卡信息接口。
 * @version 0.1
 * @date 2026-05-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_BOARD_H__
#define __WT_BSP_BOARD_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief 板卡信息对象句柄。
 */
typedef struct wt_bsp_board_obj_t *wt_bsp_board_t;

/**
 * @brief 初始化时使用的静态板卡信息。
 */
typedef struct {
    /**
     * @brief 便于阅读的板卡名称。
     */
    const char *name;

    /**
     * @brief 主版本号。
     */
    uint16_t version_major;

    /**
     * @brief 次版本号。
     */
    uint16_t version_minor;
} wt_bsp_board_info_t;

/**
 * @brief 板卡信息对象。
 */
typedef struct wt_bsp_board_obj_t{
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

/**
 * @brief 获取板卡名称。
 *
 * @param[in] board 板卡对象句柄。
 *
 * @return 板卡名称字符串；当 @p board 为 NULL 时返回空字符串。
 */
const char *wt_bsp_board_get_name(wt_bsp_board_t board);

/**
 * @brief 获取格式化后的板卡版本字符串。
 *
 * @param[in] board 板卡对象句柄。
 *
 * @return 格式为 "V<major>.<minor>" 的版本字符串；当 @p board 为 NULL
 * 时返回 "V0.0"。
 */
const char *wt_bsp_board_get_version_str(wt_bsp_board_t board);

/**
 * @brief 获取板卡主版本号。
 *
 * @param[in] board 板卡对象句柄。
 *
 * @return 主版本号；当 @p board 为 NULL 时返回 0。
 */
uint16_t wt_bsp_board_get_version_major(wt_bsp_board_t board);

/**
 * @brief 获取板卡次版本号。
 *
 * @param[in] board 板卡对象句柄。
 *
 * @return 次版本号；当 @p board 为 NULL 时返回 0。
 */
uint16_t wt_bsp_board_get_version_minor(wt_bsp_board_t board);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __WT_BSP_BOARD_H__
