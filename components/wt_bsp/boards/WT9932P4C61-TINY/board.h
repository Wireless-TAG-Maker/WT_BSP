/**
 * @file board.h
 * @author YobieZhou
 * @brief WT9932P4C61-TINY 的板级接口提供者。
 * @version 0.1
 * @date 2026-06-09
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __BOARD_H__
#define __BOARD_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#include "wt_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 获取板级 BSP 接口。
 *
 * @return 指向该板卡静态 BSP 接口的指针。
 */
wt_bsp_interface_t *board_get_bsp_interface(void);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __BOARD_H__
