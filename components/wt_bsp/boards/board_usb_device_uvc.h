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

#include "sdkconfig.h"
#include "esp_err.h"

#if defined(CONFIG_WT_BSP_BOARD_WT9932P4X_TINY) && CONFIG_WT_BSP_BOARD_WT9932P4X_TINY
#include "wt_bsp_rgb.h"
#endif

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

#if defined(CONFIG_WT_BSP_BOARD_WT9932P4X_TINY) && CONFIG_WT_BSP_BOARD_WT9932P4X_TINY
/**
 * @brief 启动 WT9932P4X-TINY UVC 状态灯任务。
 *
 * 未枚举时以最大亮度 5 绿色闪烁，已枚举未推流时在亮度 0 到 5
 * 之间以约 4.8 秒周期显示自然绿色呼吸灯（吸气约 1.5 秒、顶部停留约 0.2 秒、
 * 呼气约 2.1 秒、全黑休息约 1 秒），推流时以亮度 1 常亮绿色。
 *
 * @param[in,out] rgb 已初始化的板载 RGB LED 对象。
 *
 * @return 成功时返回 ESP_OK。
 * @return 参数无效或无法创建任务时返回对应错误。
 */
esp_err_t board_usb_device_uvc_status_led_start(wt_bsp_rgb_t rgb);
#endif

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __BOARD_USB_DEVICE_UVC_H__
