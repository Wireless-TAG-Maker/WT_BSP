/**
 * @file board_config.h
 * @author Wireless-Tag
 * @brief WT9932S31-TINY hardware capabilities.
 * @version 0.1
 * @date 2026-09-18
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__

/* ==================== [Includes] ========================================== */

/* ==================== [Defines] =========================================== */

/** @brief One addressable RGB LED is fitted. */
#define WT_BSP_BOARD_HAS_RGB 1
/** @brief Number of addressable LEDs. */
#define WT_BSP_RGB_NUM 1
/** @brief SW2 is available as a user button. */
#define WT_BSP_BOARD_HAS_BUTTON 1
/** @brief The board has a four-bit SDMMC card socket. */
#define WT_BSP_BOARD_HAS_SDMMC 1
/** @brief This board has no MIPI DSI display. */
#define WT_BSP_BOARD_HAS_DSI 0
/** @brief The camera connector is DVP, not MIPI CSI. */
#define WT_BSP_BOARD_HAS_CSI 0
/** @brief No touch controller is fitted. */
#define WT_BSP_BOARD_HAS_TOUCH 0
/** @brief J1 supports USB Device CDC. */
#define WT_BSP_BOARD_HAS_USB_DEVICE_CDC 1
/** @brief J1 supports DVP camera to USB Device UVC. */
#define WT_BSP_BOARD_HAS_USB_DEVICE_UVC 1
/** @brief Local JPEG frames can share the board's DVP/UVC capture path. */
#define WT_BSP_BOARD_HAS_CAMERA 1

/* ==================== [Typedefs] ========================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

#endif // __BOARD_CONFIG_H__
