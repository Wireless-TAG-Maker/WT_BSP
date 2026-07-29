/**
 * @file board_usb_device_uvc.h
 * @author Wireless-Tag
 * @brief ESP32-P4 板级 USB Device UVC 共享私有接口。
 * @version 0.1
 * @date 2026-07-28
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __BOARD_USB_DEVICE_UVC_H__
#define __BOARD_USB_DEVICE_UVC_H__

/* ==================== [Includes] ========================================== */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 初始化 CSI 到 USB Device UVC 的板级数据链路。
 *
 * @return 成功时返回 ESP_OK。
 * @return 摄像头、JPEG 编码器或 USB Device UVC 初始化失败时返回对应错误。
 */
esp_err_t board_usb_device_uvc_init(void);

/**
 * @brief 反初始化板级 USB Device UVC 数据链路。
 *
 * @return 成功时返回 ESP_OK。
 * @return 停止 USB Device UVC 失败时返回对应错误。
 */
esp_err_t board_usb_device_uvc_deinit(void);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __BOARD_USB_DEVICE_UVC_H__
