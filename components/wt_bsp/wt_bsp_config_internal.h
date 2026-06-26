/**
 * @file wt_bsp_config_internal.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief BSP 组件内部功能配置辅助宏。
 * @version 0.1
 * @date 2026-05-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_CONFIG_INTERNAL_H__
#define __WT_BSP_CONFIG_INTERNAL_H__

/* ==================== [Includes] ========================================== */

#include "sdkconfig.h"
#include "board_config.h"

/* ==================== [Defines] =========================================== */

#ifndef WT_BSP_RGB_NUM
/**
 * @def WT_BSP_RGB_NUM
 * @brief 板卡默认暴露的可寻址 RGB LED 数量。
 */
#define WT_BSP_RGB_NUM 1
#endif

/**
 * @def WT_BSP_RGBW_ENABLED
 * @brief RGBW 颜色支持是否启用。
 */
#if !defined(WT_BSP_RGBW_DISABLE) || (WT_BSP_RGBW_DISABLE)
#define WT_BSP_RGBW_ENABLED 0
#else
#define WT_BSP_RGBW_ENABLED 1
#endif

/**
 * @def WT_BSP_RGB_ENABLED
 * @brief RGB LED 支持是否在当前工程中启用。
 */
#if defined(WT_BSP_BOARD_HAS_RGB) && (WT_BSP_BOARD_HAS_RGB) && defined(CONFIG_WT_BSP_ENABLE_RGB) && (CONFIG_WT_BSP_ENABLE_RGB)
#define WT_BSP_RGB_ENABLED 1
#else
#define WT_BSP_RGB_ENABLED 0
#endif

/**
 * @def WT_BSP_BUTTON_ENABLED
 * @brief 按键支持是否在当前工程中启用。
 */
#if defined(WT_BSP_BOARD_HAS_BUTTON) && (WT_BSP_BOARD_HAS_BUTTON) && defined(CONFIG_WT_BSP_ENABLE_BUTTON) && (CONFIG_WT_BSP_ENABLE_BUTTON)
#define WT_BSP_BUTTON_ENABLED 1
#else
#define WT_BSP_BUTTON_ENABLED 0
#endif

/**
 * @def WT_BSP_SDMMC_ENABLED
 * @brief SD 卡支持是否在当前工程中启用。
 */
#if defined(WT_BSP_BOARD_HAS_SDMMC) && (WT_BSP_BOARD_HAS_SDMMC) && defined(CONFIG_WT_BSP_ENABLE_SDMMC) && (CONFIG_WT_BSP_ENABLE_SDMMC)
#define WT_BSP_SDMMC_ENABLED 1
#else
#define WT_BSP_SDMMC_ENABLED 0
#endif

/**
 * @def WT_BSP_DSI_ENABLED
 * @brief DSI 显示支持是否在当前工程中启用。
 */
#if defined(WT_BSP_BOARD_HAS_DSI) && (WT_BSP_BOARD_HAS_DSI) && defined(CONFIG_WT_BSP_ENABLE_DSI) && (CONFIG_WT_BSP_ENABLE_DSI)
#define WT_BSP_DSI_ENABLED 1
#else
#define WT_BSP_DSI_ENABLED 0
#endif

/**
 * @def WT_BSP_CSI_ENABLED
 * @brief CSI 摄像头支持是否在当前工程中启用。
 */
#if defined(WT_BSP_BOARD_HAS_CSI) && (WT_BSP_BOARD_HAS_CSI) && defined(CONFIG_WT_BSP_ENABLE_CSI) && (CONFIG_WT_BSP_ENABLE_CSI)
#define WT_BSP_CSI_ENABLED 1
#else
#define WT_BSP_CSI_ENABLED 0
#endif

/**
 * @def WT_BSP_TOUCH_ENABLED
 * @brief 触摸支持是否在当前工程中启用。
 */
#if defined(WT_BSP_BOARD_HAS_TOUCH) && (WT_BSP_BOARD_HAS_TOUCH) && defined(CONFIG_WT_BSP_ENABLE_TOUCH) && (CONFIG_WT_BSP_ENABLE_TOUCH)
#define WT_BSP_TOUCH_ENABLED 1
#else
#define WT_BSP_TOUCH_ENABLED 0
#endif

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/* ==================== [Macros] ============================================ */


#endif // __WT_BSP_CONFIG_INTERNAL_H__
