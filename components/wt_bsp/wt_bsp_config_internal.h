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
 * @def WT_BSP_RGBW_ENABLE_IS_ENABLED
 * @brief RGBW 颜色支持是否启用。
 */
#if !defined(WT_BSP_RGBW_DISABLE) || (WT_BSP_RGBW_DISABLE)
#define WT_BSP_RGBW_ENABLE_IS_ENABLED 0
#else
#define WT_BSP_RGBW_ENABLE_IS_ENABLED 1
#endif

/**
 * @def WT_BSP_RGB_ENABLE_IS_ENABLED
 * @brief RGB LED 支持是否启用。
 */
#if !defined(WT_BSP_RGB_ENABLE) || (WT_BSP_RGB_ENABLE)
#define WT_BSP_RGB_ENABLE_IS_ENABLED 1
#else
#define WT_BSP_RGB_ENABLE_IS_ENABLED 0
#endif

/**
 * @def WT_BSP_BUTTON_ENABLE_IS_ENABLED
 * @brief 按键支持是否启用。
 */
#if !defined(WT_BSP_BUTTON_ENABLE) || (WT_BSP_BUTTON_ENABLE)
#define WT_BSP_BUTTON_ENABLE_IS_ENABLED 1
#else
#define WT_BSP_BUTTON_ENABLE_IS_ENABLED 0
#endif

/**
 * @def WT_BSP_LCD_ENABLE_IS_ENABLED
 * @brief LCD 支持是否启用。
 */
#if !defined(WT_BSP_LCD_ENABLE) || (WT_BSP_LCD_ENABLE)
#define WT_BSP_LCD_ENABLE_IS_ENABLED 1
#else
#define WT_BSP_LCD_ENABLE_IS_ENABLED 0
#endif

/**
 * @def WT_BSP_SDCARD_ENABLE_IS_ENABLED
 * @brief SD 卡支持是否启用。
 */
#if !defined(WT_BSP_SDCARD_ENABLE) || (WT_BSP_SDCARD_ENABLE)
#define WT_BSP_SDCARD_ENABLE_IS_ENABLED 1
#else
#define WT_BSP_SDCARD_ENABLE_IS_ENABLED 0
#endif

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/* ==================== [Macros] ============================================ */


#endif // __WT_BSP_CONFIG_INTERNAL_H__
