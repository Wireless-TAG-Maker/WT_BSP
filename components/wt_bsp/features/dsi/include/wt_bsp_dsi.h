/**
 * @file wt_bsp_dsi.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP 组件的 MIPI DSI 显示接口。
 *
 * 该模块负责完成 MIPI DSI、面板驱动、背光 PWM 和 LVGL8 显示端口的初始化。
 * 普通应用不需要也不建议直接操作底层 panel 画点、画线或刷 bitmap；推荐通过
 * @ref wt_bsp_dsi_lvgl_start 获取 LVGL display 后，直接使用 LVGL API 构建界面。
 * @version 0.1
 * @date 2026-05-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_DSI_H__
#define __WT_BSP_DSI_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_config_internal.h"

/**
 * @brief DSI 显示对象句柄。
 */
typedef struct wt_bsp_dsi_obj_t *wt_bsp_dsi_t;

#if WT_BSP_DSI_ENABLED

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "soc/soc_caps.h"

#if !SOC_MIPI_DSI_SUPPORTED
#error "wt_bsp_dsi requires a target with SOC_MIPI_DSI_SUPPORTED"
#endif

#include "esp_lcd_types.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#if LVGL_VERSION_MAJOR < 8
#error "wt_bsp_dsi LVGL integration requires at least LVGL 8.x"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/**
 * @brief 默认 LVGL draw buffer 行数。
 *
 * 默认 draw buffer 大小为 `hres * WT_BSP_DSI_LVGL_DRAW_BUFF_LINES_DEFAULT` 个像素。
 */
#define WT_BSP_DSI_LVGL_DRAW_BUFF_LINES_DEFAULT (64U)

/**
 * @brief 表示未连接或不使用的 GPIO。
 */
#define WT_BSP_DSI_GPIO_NUM_NC (-1)

/* ==================== [Typedefs] ========================================== */

/**
 * @brief DSI 显示的 LVGL 配置。
 */
typedef struct {
    lvgl_port_cfg_t lvgl_port_cfg;           /*!< LVGL port 任务、tick 和调度配置。 */
    uint32_t buffer_size;                    /*!< LVGL draw buffer 大小，单位为像素；设为 0 时默认 hres * 50。 */
    bool double_buffer;                      /*!< 是否使用双 draw buffer，可提升刷新流畅度但会增加内存占用。 */
    struct {
        unsigned int buff_dma: 1;            /*!< draw buffer 是否分配为 DMA capable 内存。 */
        unsigned int buff_spiram: 1;         /*!< draw buffer 是否优先分配到 PSRAM。高分辨率屏建议启用 PSRAM。 */
        unsigned int sw_rotate: 1;           /*!< 是否使用 LVGL 软件旋转。DSI 默认方向通常不需要开启。 */
        unsigned int avoid_tearing: 1;       /*!< 是否启用 DSI 避免撕裂策略；需要至少 2 个 DPI frame buffer。 */
    } flags;
} wt_bsp_dsi_lvgl_config_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 打开显示输出。
 */
esp_err_t wt_bsp_dsi_display_on(wt_bsp_dsi_t dsi);

/**
 * @brief 关闭显示输出。
 */
esp_err_t wt_bsp_dsi_display_off(wt_bsp_dsi_t dsi);

/**
 * @brief 设置背光亮度。
 */
esp_err_t wt_bsp_dsi_set_brightness(wt_bsp_dsi_t dsi, uint8_t brightness);

/**
 * @brief 获取显示分辨率。
 */
esp_err_t wt_bsp_dsi_get_resolution(wt_bsp_dsi_t dsi, uint16_t *width, uint16_t *height);

/**
 * @brief 启动 LVGL 并注册 DSI display。
 */
lv_display_t *wt_bsp_dsi_lvgl_start(wt_bsp_dsi_t dsi, const wt_bsp_dsi_lvgl_config_t *config);

/**
 * @brief 获取已注册的 LVGL display。
 */
lv_display_t *wt_bsp_dsi_get_lvgl_display(wt_bsp_dsi_t dsi);

/**
 * @brief 获取 LVGL 互斥锁。
 */
bool wt_bsp_dsi_lvgl_lock(uint32_t timeout_ms);

/**
 * @brief 释放 LVGL 互斥锁。
 */
void wt_bsp_dsi_lvgl_unlock(void);

/**
 * @brief 获取底层 ESP LCD panel handle。
 */
esp_lcd_panel_handle_t wt_bsp_dsi_get_panel_handle(wt_bsp_dsi_t dsi);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_DSI_ENABLED

#endif // __WT_BSP_DSI_H__
