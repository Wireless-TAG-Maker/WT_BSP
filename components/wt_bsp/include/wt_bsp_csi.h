/**
 * @file wt_bsp_csi.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP 组件的 MIPI CSI 摄像头接口。
 * @version 0.1
 * @date 2026-06-03
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

#ifndef __WT_BSP_CSI_H__
#define __WT_BSP_CSI_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"

/**
 * @brief CSI 摄像头对象句柄。
 */
typedef struct wt_bsp_csi_obj_t *wt_bsp_csi_t;

#if WT_BSP_CSI_ENABLE_IS_ENABLED

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

#define WT_BSP_CSI_DEV_PATH "/dev/video0"

/* ==================== [Typedefs] ========================================== */

/**
 * @brief 帧回调函数原型。
 * 
 * @param buf 帧数据缓冲区。
 * @param width 图像宽度。
 * @param height 图像高度。
 * @param len 缓冲区长度。
 * @param user_data 用户上下文数据。
 */
typedef void (*wt_bsp_csi_frame_cb_t)(uint8_t *buf, uint32_t width, uint32_t height, size_t len, void *user_data);

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 设置 CSI 采集像素格式。
 *
 * 该函数应在 @ref wt_bsp_csi_start 前调用。@p pixel_format 使用 V4L2 像素格式值。
 *
 * @param[in,out] csi CSI 对象句柄。
 * @param[in] pixel_format V4L2 像素格式。
 *
 * @return 成功时返回 ESP_OK。
 * @return 当 @p csi 为 NULL 时返回 ESP_ERR_INVALID_ARG。
 * @return 摄像头已经开始采集时返回 ESP_ERR_INVALID_STATE。
 */
esp_err_t wt_bsp_csi_set_pixel_format(wt_bsp_csi_t csi, uint32_t pixel_format);

/**
 * @brief 开始视频采集。
 * 
 * @param[in] csi CSI 对象句柄。
 * @param[in] frame_cb 帧数据回调。
 * @param[in] user_data 回调函数上下文。
 * @return esp_err_t 
 */
esp_err_t wt_bsp_csi_start(wt_bsp_csi_t csi, wt_bsp_csi_frame_cb_t frame_cb, void *user_data);

/**
 * @brief 停止视频采集。
 * 
 * @param[in] csi CSI 对象句柄。
 * @return esp_err_t 
 */
esp_err_t wt_bsp_csi_stop(wt_bsp_csi_t csi);

#ifdef __cplusplus
}
#endif

#endif // WT_BSP_CSI_ENABLE_IS_ENABLED

#endif // __WT_BSP_CSI_H__
