/**
 * @file wt_bsp_csi_port.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP CSI port 接口。
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_CSI_PORT_H__
#define __WT_BSP_CSI_PORT_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_csi.h"

#if WT_BSP_CSI_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

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
    uint32_t pixel_format;   /*!< 采集像素格式 (如 V4L2_PIX_FMT_RGB565)。为 0 则默认 V4L2_PIX_FMT_RGB24。 */
    uint8_t buffer_count;    /*!< 缓冲区数量, 范围为 1 到 3。 */
} wt_bsp_csi_info_t;

/**
 * @brief CSI 对象结构。
 */
typedef struct wt_bsp_csi_obj_t {
    wt_bsp_csi_info_t info;
    int v4l2_fd;
    wt_bsp_csi_frame_cb_t frame_cb;
    void *user_data;
    void *video_stream_task_handle; /*!< 采集任务句柄; 生命周期调用由同一应用任务串行执行。 */
    void *stream_stop_semaphore; /*!< 对象持有的二值停止信号量, 仅在任务 join 后释放。 */
    uint8_t *buffers[3];
    size_t buffer_size;
    bool is_initialized;
    volatile bool is_streaming; /*!< 采集任务读取的停止请求状态。 */
    bool stream_task_active; /*!< 生命周期任务尚未 join 采集任务, 即使任务已自然退出。 */
} wt_bsp_csi_obj_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 初始化 CSI 摄像头。
 *
 * @param[in,out] csi CSI 对象句柄。
 * @param[in] info 硬件配置信息。
 * 对象必须由板级静态存储持有, 生命周期调用必须串行。
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_NO_MEM 或摄像头初始化错误。
 */
esp_err_t wt_bsp_csi_init(wt_bsp_csi_t csi, const wt_bsp_csi_info_t *info);

/**
 * @brief 反初始化 CSI 摄像头并释放资源。
 *
 * @param[in] csi CSI 对象句柄。
 * 必须在帧回调之外调用。只有采集任务已经停止且不再使用回调上下文后才释放对象资源。
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE 或 ESP_ERR_TIMEOUT。
 *         停止失败时保留对象和资源, 调用方应重试而非释放对象。
 */
esp_err_t wt_bsp_csi_deinit(wt_bsp_csi_t csi);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
}
#endif

#endif // WT_BSP_CSI_ENABLED

#endif // __WT_BSP_CSI_PORT_H__
