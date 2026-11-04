/**
 * @file camera_uvc_csi.h
 * @author Wireless-Tag
 * @brief Application-owned CSI/JPEG to USB UVC bridge interface.
 * @version 0.1
 * @date 2026-07-28
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __CAMERA_UVC_CSI_H__
#define __CAMERA_UVC_CSI_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp.h"
#include <stdbool.h>

#if WT_BSP_USB_DEVICE_UVC_ENABLED && WT_BSP_CSI_ENABLED

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 初始化 CSI 到 USB Device UVC 的共享数据链路。
 *
 * 由应用在 BSP 初始化成功后调用, 桥接层持有采集和 USB 缓冲资源。
 * 应用不得直接释放这些资源。
 *
 * @return 成功时返回 ESP_OK。
 * @return 摄像头、JPEG 编码器或 USB Device UVC 初始化失败时返回对应错误。
 */
esp_err_t camera_uvc_csi_start(void);

/**
 * @brief 停止 USB Device UVC 并释放共享 CSI 数据链路资源。
 *
 * 由应用在释放 BSP 摄像头前调用, 允许重复反初始化。
 * USB 停止失败时保留缓冲和采集资源, 调用方应保留摄像头并重试。
 *
 * @return 成功时返回 ESP_OK。
 * @return 停止 USB Device UVC 失败时返回对应错误。
 */
esp_err_t camera_uvc_csi_stop(void);

/** @brief Return true while the host is consuming the application CSI stream. */
bool camera_uvc_csi_is_streaming(void);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_USB_DEVICE_UVC_ENABLED && WT_BSP_CSI_ENABLED

#endif // __CAMERA_UVC_CSI_H__
