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

#if WT_BSP_DSI_ENABLE_IS_ENABLED

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "soc/soc_caps.h"

#if !SOC_MIPI_DSI_SUPPORTED
#error "wt_bsp_dsi requires a target with SOC_MIPI_DSI_SUPPORTED"
#endif

#include "esp_lcd_types.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#if LVGL_VERSION_MAJOR != 8
#error "wt_bsp_dsi LVGL integration requires LVGL 8.x"
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
#define WT_BSP_DSI_LVGL_DRAW_BUFF_LINES_DEFAULT (50U)

/**
 * @brief 表示未连接或不使用的 GPIO。
 */
#define WT_BSP_DSI_GPIO_NUM_NC (-1)

/* ==================== [Typedefs] ========================================== */

/**
 * @brief 支持的 DSI 面板型号。
 */
typedef enum {
    WT_BSP_DSI_PANEL_AUTO = 0,             /*!< 根据分辨率自动选择面板。当前支持 1024x600 和 800x1280。 */
    WT_BSP_DSI_PANEL_EK79007_1024_600,     /*!< 1024x600 EK79007 MIPI DSI 面板。 */
    WT_BSP_DSI_PANEL_ILI9881C_800_1280,    /*!< 800x1280 ILI9881C MIPI DSI 面板。 */
    WT_BSP_DSI_PANEL_ST7102_480_640,       /*!< 480x640 ST7102 MIPI DSI 面板 (2.8寸)。 */
} wt_bsp_dsi_panel_t;

/**
 * @brief DSI 帧缓冲输入颜色格式。
 */
typedef enum {
    WT_BSP_DSI_COLOR_FORMAT_RGB565 = 0,    /*!< RGB565，默认推荐格式，内存占用较低。 */
    WT_BSP_DSI_COLOR_FORMAT_RGB888,        /*!< RGB888，色彩精度更高，frame buffer 占用更大。 */
} wt_bsp_dsi_color_format_t;

/**
 * @brief DSI 硬件配置。
 */
typedef struct {
    int backlight_gpio_num;                 /*!< 连接背光 PWM 控制的 GPIO，未使用时设为 -1。 */
    int reset_gpio_num;                     /*!< 连接显示复位的 GPIO，未使用时设为 -1。 */
    uint16_t width;                         /*!< 显示屏水平分辨率，设为 0 时使用面板默认值。 */
    uint16_t height;                        /*!< 显示屏垂直分辨率，设为 0 时使用面板默认值。 */
    uint8_t dsi_lane_num;                   /*!< DSI 数据通道数量，设为 0 时默认使用 2 lane。 */
    wt_bsp_dsi_panel_t panel_type;          /*!< DSI 面板型号，设为 AUTO 时根据分辨率选择。 */
    wt_bsp_dsi_color_format_t color_format; /*!< 帧缓冲输入颜色格式。 */
    uint8_t dpi_frame_buffer_num;           /*!< DPI frame buffer 数量，设为 0 时默认使用 1 个。 */
    uint16_t lane_bit_rate_mbps;            /*!< DSI lane 速率，单位 Mbps，设为 0 时默认 1000 Mbps。 */
    int phy_ldo_channel;                    /*!< DSI PHY 供电 LDO 通道，设为 0 时默认使用通道 3，设为 -1 时不配置。 */
    int phy_ldo_voltage_mv;                 /*!< DSI PHY 供电电压，单位 mV，设为 0 时默认 2500 mV。 */
    int ledc_channel;                       /*!< 背光 LEDC 通道，设为 -1 时默认使用通道 1。 */
    int ledc_timer;                         /*!< 背光 LEDC 定时器，设为 -1 时默认使用定时器 1。 */
} wt_bsp_dsi_info_t;

/**
 * @brief DSI 显示对象句柄。
 */
typedef struct wt_bsp_dsi_obj_t *wt_bsp_dsi_t;

/**
 * @brief DSI 显示对象。
 */
typedef struct wt_bsp_dsi_obj_t {
    wt_bsp_dsi_info_t info;                 /*!< DSI 硬件配置。 */
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;  /*!< MIPI DSI bus handle。 */
    esp_lcd_panel_io_handle_t io;           /*!< ESP LCD IO handle。 */
    esp_lcd_panel_handle_t panel;           /*!< ESP LCD panel handle。 */
    esp_ldo_channel_handle_t phy_pwr_chan;  /*!< DSI PHY 供电 LDO handle。 */
    bool brightness_initialized;            /*!< 背光 PWM 是否已初始化。 */
    bool lvgl_port_initialized;             /*!< LVGL port 是否已由本对象初始化。 */
    lv_display_t *lvgl_display;             /*!< LVGL display handle。 */
    bool is_initialized;                    /*!< DSI 对象是否已初始化。 */
} wt_bsp_dsi_obj_t;

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
 * @brief 初始化 DSI 显示对象。
 * 
 * @param[in,out] dsi 待初始化的 DSI 对象。
 * @param[in] info 硬件配置。
 * @return esp_err_t 
 */
esp_err_t wt_bsp_dsi_init(wt_bsp_dsi_t dsi, const wt_bsp_dsi_info_t *info);

/**
 * @brief 反初始化 DSI 显示对象。
 * 
 * @param[in,out] dsi 待反初始化的 DSI 对象。
 * @return esp_err_t 
 */
esp_err_t wt_bsp_dsi_deinit(wt_bsp_dsi_t dsi);

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

#endif // WT_BSP_DSI_ENABLE_IS_ENABLED

#endif // __WT_BSP_DSI_H__
