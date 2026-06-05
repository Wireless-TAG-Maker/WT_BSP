/**
 * @file board_config.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief WT9932P4-TINY 的功能配置。
 * @version 0.1
 * @date 2026-05-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

/* ==================== [Includes] ========================================== */

/* ==================== [Defines] =========================================== */

/** @brief 是否启用该板卡的 RGB LED 支持。 */
#define WT_BSP_RGB_ENABLE 1
/** @brief 该板卡暴露的 RGB LED 数量。 */
#define WT_BSP_RGB_NUM 1
/** @brief 是否启用该板卡的按键支持。 */
#define WT_BSP_BUTTON_ENABLE 1
/** @brief 是否启用该板卡的 LCD 支持。 */
#define WT_BSP_LCD_ENABLE 1
/** @brief 是否启用该板卡的 SD 卡支持。 */
#define WT_BSP_SDCARD_ENABLE 1
/** @brief 是否启用该板卡的 DSI 显示支持。 */
#define WT_BSP_DSI_ENABLE 1
/** @brief 是否启用该板卡的 CSI 摄像头支持。 */
#define WT_BSP_CSI_ENABLE 1

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/* ==================== [Macros] ============================================ */

#endif // __BOARD_CONFIG_H__
