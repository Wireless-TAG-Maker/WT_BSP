/**
 * @file board.c
 * @author Wireless-Tag
 * @brief WT9932P4X-TINY board identity and shared peripheral entry.
 * @version 0.1
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

/* ==================== [Includes] ========================================== */

#include "board.h"
#include "board_common.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

wt_bsp_interface_t *board_get_bsp_interface(void)
{
    return board_p4_tiny_get_interface("WT9932P4X-TINY");
}

/* ==================== [Static Functions] ================================== */
