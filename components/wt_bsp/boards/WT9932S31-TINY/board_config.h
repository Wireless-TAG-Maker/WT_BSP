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
/** @brief J1 supports USB Device UVC with an application-provided frame source. */
#define WT_BSP_BOARD_HAS_USB_DEVICE_UVC 1
/** @brief The board provides an independent DVP JPEG camera adapter. */
#define WT_BSP_BOARD_HAS_CAMERA 1

#if CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145
/** @brief Driver identifier resolved by the camera interface. */
#define BOARD_CAMERA_SENSOR_NAME "gc2145"
/** @brief GC2145 fixed capture width in pixels. */
#define BOARD_CAMERA_FRAME_WIDTH 1600
/** @brief GC2145 fixed capture height in pixels. */
#define BOARD_CAMERA_FRAME_HEIGHT 1200
/** @brief GC2145 nominal capture rate for the selected register configuration. */
#define BOARD_CAMERA_FRAME_RATE 8
/** @brief One DVP user buffer leaves room for independent JPEG snapshots in 16 MiB PSRAM. */
#define BOARD_CAMERA_CAPTURE_BUFFER_COUNT 1
/** @brief GC2145 encoded-frame capacity in bytes. */
#define BOARD_CAMERA_JPEG_BUFFER_CAPACITY (BOARD_CAMERA_FRAME_WIDTH * BOARD_CAMERA_FRAME_HEIGHT)
/** @brief Drain incomplete frames after enabling the GC2145 output. */
#define BOARD_CAMERA_STARTUP_DISCARD_COUNT 2
/** @brief Hardware JPEG deadline selected for UXGA capture. */
#define BOARD_CAMERA_JPEG_ENCODE_TIMEOUT_MS CONFIG_WT_BSP_S31_JPEG_ENCODE_TIMEOUT_MS
#elif CONFIG_WT_BSP_S31_CAMERA_SENSOR_OV3660
/** @brief Driver identifier resolved by the camera interface. */
#define BOARD_CAMERA_SENSOR_NAME "ov3660"
/** @brief OV3660 fixed capture width in pixels. */
#define BOARD_CAMERA_FRAME_WIDTH 640
/** @brief OV3660 fixed capture height in pixels. */
#define BOARD_CAMERA_FRAME_HEIGHT 480
/** @brief OV3660 nominal capture rate for the selected register configuration. */
#define BOARD_CAMERA_FRAME_RATE 25
/** @brief Number of sensor-native JPEG capture buffers. */
#define BOARD_CAMERA_CAPTURE_BUFFER_COUNT 2
/** @brief OV3660 encoded-frame capacity in bytes. */
#define BOARD_CAMERA_JPEG_BUFFER_CAPACITY (BOARD_CAMERA_FRAME_WIDTH * BOARD_CAMERA_FRAME_HEIGHT * 2)
/** @brief Native JPEG does not need startup-frame draining. */
#define BOARD_CAMERA_STARTUP_DISCARD_COUNT 0
/** @brief Unused hardware encoder deadline for the native JPEG profile. */
#define BOARD_CAMERA_JPEG_ENCODE_TIMEOUT_MS 200
#endif

/* ==================== [Typedefs] ========================================== */

/* ==================== [Macros] ============================================ */

/* ==================== [Global Prototypes] ================================= */

#endif // __BOARD_CONFIG_H__
