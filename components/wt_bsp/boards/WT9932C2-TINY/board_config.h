/**
 * @file board_config.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief WT9932C2-TINY 的功能配置。
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

/** @brief 该板卡是否具备 RGB LED 能力。 */
#define WT_BSP_BOARD_HAS_RGB 1
/** @brief 该板卡暴露的 RGB LED 数量。 */
#define WT_BSP_RGB_NUM 1
/** @brief 该板卡是否具备按键能力。 */
#define WT_BSP_BOARD_HAS_BUTTON 1
/** @brief 该板卡是否具备 SD 卡能力。 */
#define WT_BSP_BOARD_HAS_SDMMC 0

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/* ==================== [Macros] ============================================ */

#endif // __BOARD_CONFIG_H__
