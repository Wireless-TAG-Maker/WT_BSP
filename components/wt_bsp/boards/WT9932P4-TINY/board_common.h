/**
 * @file board_common.h
 * @author Wireless-Tag
 * @brief Private shared peripheral implementation for the P4/P4X TINY family.
 * @version 0.1
 * @date 2026-09-23
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __WT_BSP_P4_TINY_COMMON_H__
#define __WT_BSP_P4_TINY_COMMON_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"
#include "wt_bsp_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief Get the shared board-owned interface with the selected board name.
 * @param[in] name Static board name that remains valid for the BSP lifetime.
 * @return Static interface, or NULL if name is NULL. The caller must not free it.
 *
 * Only the selected board entry calls this before BSP initialization. GPIOs and
 * resource lifetimes are identical on WT9932P4-TINY and WT9932P4X-TINY.
 */
wt_bsp_interface_t *board_p4_tiny_get_interface(const char *name);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __WT_BSP_P4_TINY_COMMON_H__
