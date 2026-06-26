/**
 * @file wt_bsp_port.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP board 移植入口。
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_PORT_H__
#define __WT_BSP_PORT_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp.h"
#include "wt_bsp_board_port.h"
#include "wt_bsp_button_port.h"
#include "wt_bsp_rgb_port.h"
#include "wt_bsp_sdmmc_port.h"
#include "wt_bsp_dsi_port.h"
#include "wt_bsp_csi_port.h"
#include "wt_bsp_touch_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/**
 * @brief 板级 BSP 操作表。
 *
 * 每个板级实现都会提供一个接口实例。@ref wt_bsp_init 成功后，
 * 通用的 `wt_bsp_*` 接口会通过该表转发到具体板级实现。
 */
typedef struct {
    esp_err_t (*init)(void);               /*!< 初始化当前选中板卡的资源。 */
    esp_err_t (*deinit)(void);             /*!< 反初始化当前选中板卡的资源。 */
    wt_bsp_board_t (*get_board)(void);     /*!< 获取板卡信息句柄。 */
#if WT_BSP_BUTTON_ENABLE_IS_ENABLED
    wt_bsp_button_t (*get_button)(void);   /*!< 获取默认按键句柄。 */
#endif
#if WT_BSP_RGB_ENABLE_IS_ENABLED
    wt_bsp_rgb_t (*get_rgb)(void);         /*!< 获取默认 RGB LED 句柄。 */
#endif
#if WT_BSP_SDCARD_ENABLE_IS_ENABLED
    wt_bsp_sdmmc_t (*get_sdmmc)(void);     /*!< 获取默认 SD 卡句柄。 */
#endif
#if WT_BSP_DSI_ENABLE_IS_ENABLED
    wt_bsp_dsi_t (*get_dsi)(void);         /*!< 获取默认 DSI 显示句柄。 */
#endif
#if WT_BSP_CSI_ENABLE_IS_ENABLED
    wt_bsp_csi_t (*get_csi)(void);         /*!< 获取默认 CSI 摄像头句柄。 */
#endif
#if WT_BSP_TOUCH_ENABLE_IS_ENABLED
    wt_bsp_touch_t (*get_touch)(void);     /*!< 获取默认触摸句柄。 */
#endif
} wt_bsp_interface_t;

/**
 * @brief 通用 BSP 对象。
 */
typedef struct wt_bsp_obj_t {
    /**
     * @brief 当前激活的板级接口。
     */
    wt_bsp_interface_t *interface;
} wt_bsp_obj_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 获取当前选中板卡的 BSP 操作表。
 *
 * @return 板级 BSP 操作表；不可用时返回 NULL。
 */
wt_bsp_interface_t *board_get_bsp_interface(void);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __WT_BSP_PORT_H__
