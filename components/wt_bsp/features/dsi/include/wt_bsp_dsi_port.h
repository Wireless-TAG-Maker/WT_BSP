/**
 * @file wt_bsp_dsi_port.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief Wireless-Tag BSP DSI port 接口。
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __WT_BSP_DSI_PORT_H__
#define __WT_BSP_DSI_PORT_H__

/* ==================== [Includes] ========================================== */

#include "wt_bsp_dsi.h"

#if WT_BSP_DSI_ENABLED

#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

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

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // WT_BSP_DSI_ENABLED

#endif // __WT_BSP_DSI_PORT_H__
