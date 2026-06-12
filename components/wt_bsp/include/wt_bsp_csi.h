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
 * @brief CSI 摄像头对象句柄。
 */
typedef struct wt_bsp_csi_obj_t *wt_bsp_csi_t;

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

/**
 * @brief CSI 硬件配置。
 */
typedef struct {
    void *i2c_bus_handle;    /*!< 外部提供的 I2C 总线句柄 (i2c_master_bus_handle_t)。如果非 NULL，则不内部初始化 SCCB I2C。 */
    int sccb_scl_pin;        /*!< SCCB (I2C) SCL 引脚。 */
    int sccb_sda_pin;        /*!< SCCB (I2C) SDA 引脚。 */
    int reset_pin;           /*!< 摄像头复位引脚，未使用设为 -1。 */
    int pwdn_pin;            /*!< 摄像头电源控制引脚，未使用设为 -1。 */
    uint32_t width;          /*!< 采集宽度。 */
    uint32_t height;         /*!< 采集高度。 */
    uint32_t fps;            /*!< 帧率。 */
    uint8_t buffer_count;    /*!< 缓冲区数量，建议 2 或 3。 */
} wt_bsp_csi_info_t;

/**
 * @brief CSI 对象结构（内部使用，暴露给 board.c 实例化）。
 */
typedef struct wt_bsp_csi_obj_t {
    wt_bsp_csi_info_t info;
    int v4l2_fd;
    wt_bsp_csi_frame_cb_t frame_cb;
    void *user_data;
    void *video_stream_task_handle;
    uint8_t *buffers[3];
    size_t buffer_size;
    bool is_initialized;
    bool is_streaming;
} wt_bsp_csi_obj_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 初始化 CSI 摄像头。
 * 
 * @param[in,out] csi CSI 对象句柄。
 * @param[in] info 硬件配置信息。
 * @return esp_err_t 
 */
esp_err_t wt_bsp_csi_init(wt_bsp_csi_t csi, const wt_bsp_csi_info_t *info);

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

/**
 * @brief 反初始化 CSI 摄像头并释放资源。
 * 
 * @param[in] csi CSI 对象句柄。
 * @return esp_err_t 
 */
esp_err_t wt_bsp_csi_deinit(wt_bsp_csi_t csi);

#ifdef __cplusplus
}
#endif

#endif // WT_BSP_CSI_ENABLE_IS_ENABLED

#endif // __WT_BSP_CSI_H__
