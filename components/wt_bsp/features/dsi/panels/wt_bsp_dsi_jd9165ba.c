/**
 * @file wt_bsp_dsi_jd9165ba.c
 * @author Wireless-Tag
 * @brief JD9165BA 面板初始化与 DPI 包装.
 * @version 0.1
 * @date 2026-11-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Adapted from wireless-tag-com/WT99P461_S1_1V1, commit
 * b5d292f3292c5d73652f678f76b0771fde166117,
 * components/esp_lcd_jd9165/esp_lcd_jd9165.c.
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_dsi_jd9165ba.h"

#if WT_BSP_DSI_ENABLED

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/mipi_dsi_host_ll.h"
#include "hal/mipi_dsi_ll.h"

#include <stdlib.h>

/* ==================== [Defines] =========================================== */

#define WT_BSP_DSI_JD9165BA_PAGE_SELECT 0x30
#define WT_BSP_DSI_JD9165BA_GS_BIT     (1U << 0)
#define WT_BSP_DSI_JD9165BA_SS_BIT     (1U << 1)

/* ==================== [Typedefs] ========================================== */

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t data[14];
} wt_bsp_dsi_jd9165ba_init_cmd_t;

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_active_high;
    uint8_t madctl;
    esp_err_t (*dpi_del)(esp_lcd_panel_t *panel);
    esp_err_t (*dpi_init)(esp_lcd_panel_t *panel);
} wt_bsp_dsi_jd9165ba_panel_t;

/* ==================== [Static Prototypes] ================================= */

static esp_err_t wt_bsp_dsi_jd9165ba_select_page_zero(wt_bsp_dsi_jd9165ba_panel_t *ctx);
static esp_err_t wt_bsp_dsi_jd9165ba_del(esp_lcd_panel_t *panel);
static esp_err_t wt_bsp_dsi_jd9165ba_reset(esp_lcd_panel_t *panel);
static esp_err_t wt_bsp_dsi_jd9165ba_init(esp_lcd_panel_t *panel);
static esp_err_t wt_bsp_dsi_jd9165ba_disp_on_off(esp_lcd_panel_t *panel, bool on);
static esp_err_t wt_bsp_dsi_jd9165ba_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t wt_bsp_dsi_jd9165ba_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t wt_bsp_dsi_jd9165ba_invert(esp_lcd_panel_t *panel, bool invert);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_dsi_jd9165ba";

/* 保留已验证模组的寄存器值和顺序; page 1 的 0x0B=0x11 选择两条通道,
 * page 0 的 0x3A=0x55 选择 RGB565. Sleep Out / Display On 单独发送.
 */
static const wt_bsp_dsi_jd9165ba_init_cmd_t s_init_commands[] = {
    {
        .cmd = 0x30,
        .len = 1,
        .data = {0x00},
    },
    {
        .cmd = 0xF7,
        .len = 4,
        .data = {0x49, 0x61, 0x02, 0x00},
    },
    {
        .cmd = 0x30,
        .len = 1,
        .data = {0x01},
    },
    {
        .cmd = 0x04,
        .len = 1,
        .data = {0x0C},
    },
    {
        .cmd = 0x05,
        .len = 1,
        .data = {0x08},
    },
    {
        .cmd = 0x0B,
        .len = 1,
        .data = {0x11},
    },
    {
        .cmd = 0x20,
        .len = 1,
        .data = {0x04},
    },
    {
        .cmd = 0x1F,
        .len = 1,
        .data = {0x00},
    },
    {
        .cmd = 0x23,
        .len = 1,
        .data = {0x38},
    },
    {
        .cmd = 0x28,
        .len = 1,
        .data = {0x18},
    },
    {
        .cmd = 0x29,
        .len = 1,
        .data = {0x29},
    },
    {
        .cmd = 0x2A,
        .len = 1,
        .data = {0x01},
    },
    {
        .cmd = 0x2B,
        .len = 1,
        .data = {0x29},
    },
    {
        .cmd = 0x2C,
        .len = 1,
        .data = {0x01},
    },
    {
        .cmd = 0x30,
        .len = 1,
        .data = {0x02},
    },
    {
        .cmd = 0x00,
        .len = 1,
        .data = {0x05},
    },
    {
        .cmd = 0x01,
        .len = 1,
        .data = {0x22},
    },
    {
        .cmd = 0x02,
        .len = 1,
        .data = {0x08},
    },
    {
        .cmd = 0x03,
        .len = 1,
        .data = {0x12},
    },
    {
        .cmd = 0x04,
        .len = 1,
        .data = {0x16},
    },
    {
        .cmd = 0x05,
        .len = 1,
        .data = {0x64},
    },
    {
        .cmd = 0x06,
        .len = 1,
        .data = {0x00},
    },
    {
        .cmd = 0x07,
        .len = 1,
        .data = {0x00},
    },
    {
        .cmd = 0x08,
        .len = 1,
        .data = {0x78},
    },
    {
        .cmd = 0x09,
        .len = 1,
        .data = {0x00},
    },
    {
        .cmd = 0x0A,
        .len = 1,
        .data = {0x04},
    },
    {
        .cmd = 0x0B,
        .len = 11,
        .data = {0x16, 0x17, 0x0B, 0x0D, 0x0D, 0x0D, 0x11, 0x10, 0x07, 0x07, 0x09},
    },
    {
        .cmd = 0x0C,
        .len = 11,
        .data = {0x09, 0x1E, 0x1E, 0x1C, 0x1C, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D},
    },
    {
        .cmd = 0x0D,
        .len = 11,
        .data = {0x0A, 0x05, 0x0B, 0x0D, 0x0D, 0x0D, 0x11, 0x10, 0x06, 0x06, 0x08},
    },
    {
        .cmd = 0x0E,
        .len = 11,
        .data = {0x08, 0x1F, 0x1F, 0x1D, 0x1D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D},
    },
    {
        .cmd = 0x0F,
        .len = 11,
        .data = {0x0A, 0x05, 0x0D, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x1D, 0x1D, 0x1F},
    },
    {
        .cmd = 0x10,
        .len = 11,
        .data = {0x1F, 0x08, 0x08, 0x06, 0x06, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D},
    },
    {
        .cmd = 0x11,
        .len = 11,
        .data = {0x16, 0x17, 0x0D, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x1C, 0x1C, 0x1E},
    },
    {
        .cmd = 0x12,
        .len = 11,
        .data = {0x1E, 0x09, 0x09, 0x07, 0x07, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D},
    },
    {
        .cmd = 0x13,
        .len = 4,
        .data = {0x00, 0x00, 0x00, 0x00},
    },
    {
        .cmd = 0x14,
        .len = 4,
        .data = {0x00, 0x00, 0x41, 0x41},
    },
    {
        .cmd = 0x15,
        .len = 4,
        .data = {0x00, 0x00, 0x00, 0x00},
    },
    {
        .cmd = 0x17,
        .len = 1,
        .data = {0x00},
    },
    {
        .cmd = 0x18,
        .len = 1,
        .data = {0x85},
    },
    {
        .cmd = 0x19,
        .len = 2,
        .data = {0x06, 0x09},
    },
    {
        .cmd = 0x1A,
        .len = 2,
        .data = {0x05, 0x08},
    },
    {
        .cmd = 0x1B,
        .len = 2,
        .data = {0x0A, 0x04},
    },
    {
        .cmd = 0x26,
        .len = 1,
        .data = {0x00},
    },
    {
        .cmd = 0x27,
        .len = 1,
        .data = {0x00},
    },
    {
        .cmd = 0x30,
        .len = 1,
        .data = {0x06},
    },
    {
        .cmd = 0x12,
        .len = 14,
        .data = {0x3F, 0x26, 0x27, 0x35, 0x2D, 0x34, 0x3F, 0x3F, 0x3F, 0x35, 0x2A, 0x20, 0x16, 0x08},
    },
    {
        .cmd = 0x13,
        .len = 14,
        .data = {0x3F, 0x26, 0x28, 0x35, 0x27, 0x29, 0x29, 0x2F, 0x35, 0x2F, 0x26, 0x20, 0x16, 0x08},
    },
    {
        .cmd = 0x30,
        .len = 1,
        .data = {0x0A},
    },
    {
        .cmd = 0x02,
        .len = 1,
        .data = {0x4F},
    },
    {
        .cmd = 0x0B,
        .len = 1,
        .data = {0x40},
    },
    {
        .cmd = 0x30,
        .len = 1,
        .data = {0x0D},
    },
    {
        .cmd = 0x0D,
        .len = 1,
        .data = {0x04},
    },
    {
        .cmd = 0x10,
        .len = 1,
        .data = {0x0C},
    },
    {
        .cmd = 0x11,
        .len = 1,
        .data = {0x0C},
    },
    {
        .cmd = 0x12,
        .len = 1,
        .data = {0x0C},
    },
    {
        .cmd = 0x13,
        .len = 1,
        .data = {0x0C},
    },
    {
        .cmd = 0x30,
        .len = 1,
        .data = {0x00},
    },
    {
        .cmd = 0x3A,
        .len = 1,
        .data = {0x55},
    },
};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_dsi_new_panel_jd9165ba(esp_lcd_panel_io_handle_t io,
                                      const esp_lcd_panel_dev_config_t *panel_config,
                                      esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    esp_lcd_panel_handle_t panel = NULL;
    wt_bsp_dsi_jd9165ba_panel_t *ctx = NULL;

    if (ret_panel != NULL) {
        *ret_panel = NULL;
    }
    ESP_RETURN_ON_FALSE(io && panel_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    const wt_bsp_dsi_jd9165ba_vendor_config_t *vendor = panel_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor && vendor->dsi_bus && vendor->dpi_config,
                        ESP_ERR_INVALID_ARG, TAG, "Missing DSI configuration");
    ESP_RETURN_ON_FALSE(panel_config->bits_per_pixel == 16 &&
                        vendor->dpi_config->in_color_format == LCD_COLOR_FMT_RGB565 &&
                        (!vendor->dpi_config->out_color_format ||
                         vendor->dpi_config->out_color_format == LCD_COLOR_FMT_RGB565),
                        ESP_ERR_NOT_SUPPORTED, TAG, "JD9165BA requires RGB565 input/output");
    ESP_RETURN_ON_FALSE(panel_config->rgb_ele_order == LCD_RGB_ELEMENT_ORDER_RGB ||
                        panel_config->rgb_ele_order == LCD_RGB_ELEMENT_ORDER_BGR,
                        ESP_ERR_NOT_SUPPORTED, TAG, "Unsupported RGB order");
    ESP_RETURN_ON_FALSE(panel_config->reset_gpio_num == -1 ||
                        GPIO_IS_VALID_OUTPUT_GPIO(panel_config->reset_gpio_num),
                        ESP_ERR_INVALID_ARG, TAG, "Invalid reset GPIO");

    ctx = calloc(1, sizeof(*ctx));
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_NO_MEM, TAG, "Allocate panel failed");
    ctx->io = io;
    ctx->reset_gpio_num = panel_config->reset_gpio_num;
    ctx->reset_active_high = panel_config->flags.reset_active_high;
    ctx->madctl = panel_config->rgb_ele_order == LCD_RGB_ELEMENT_ORDER_BGR ? LCD_CMD_BGR_BIT : 0;

    if (ctx->reset_gpio_num >= 0) {
        const gpio_config_t gpio_cfg = {
            .pin_bit_mask = 1ULL << ctx->reset_gpio_num,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&gpio_cfg), err, TAG, "Configure reset GPIO failed");
    }

    /* 该模组不使用 BTA/ACK. DPI factory 会重新设置 frame ACK, 须在其后关闭. */
    mipi_dsi_host_ll_enable_cmd_ack(MIPI_DSI_LL_GET_HOST(0), false);
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_dpi(vendor->dsi_bus, vendor->dpi_config, &panel),
                      err, TAG, "Create DPI panel failed");
    mipi_dsi_host_ll_dpi_enable_frame_ack(MIPI_DSI_LL_GET_HOST(0), false);

    ctx->dpi_del = panel->del;
    ctx->dpi_init = panel->init;
    panel->del = wt_bsp_dsi_jd9165ba_del;
    panel->reset = wt_bsp_dsi_jd9165ba_reset;
    panel->init = wt_bsp_dsi_jd9165ba_init;
    panel->disp_on_off = wt_bsp_dsi_jd9165ba_disp_on_off;
    panel->mirror = wt_bsp_dsi_jd9165ba_mirror;
    panel->swap_xy = wt_bsp_dsi_jd9165ba_swap_xy;
    panel->invert_color = wt_bsp_dsi_jd9165ba_invert;
    panel->user_data = ctx;
    *ret_panel = panel;
    return ESP_OK;

err:
    if (ctx->reset_gpio_num >= 0) {
        (void)gpio_reset_pin(ctx->reset_gpio_num);
    }
    free(ctx);
    return ret;
}

/* ==================== [Static Functions] ================================== */

static esp_err_t wt_bsp_dsi_jd9165ba_select_page_zero(wt_bsp_dsi_jd9165ba_panel_t *ctx)
{
    const uint8_t page = 0;
    return esp_lcd_panel_io_tx_param(ctx->io, WT_BSP_DSI_JD9165BA_PAGE_SELECT, &page, sizeof(page));
}

static esp_err_t wt_bsp_dsi_jd9165ba_del(esp_lcd_panel_t *panel)
{
    wt_bsp_dsi_jd9165ba_panel_t *ctx = panel->user_data;

    ESP_RETURN_ON_ERROR(ctx->dpi_del(panel), TAG, "Delete DPI panel failed");
    if (ctx->reset_gpio_num >= 0) {
        (void)gpio_reset_pin(ctx->reset_gpio_num);
    }
    free(ctx);
    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_jd9165ba_reset(esp_lcd_panel_t *panel)
{
    wt_bsp_dsi_jd9165ba_panel_t *ctx = panel->user_data;

    if (ctx->reset_gpio_num >= 0) {
        ESP_RETURN_ON_ERROR(gpio_set_level(ctx->reset_gpio_num, !ctx->reset_active_high),
                            TAG, "Release reset GPIO failed");
        vTaskDelay(pdMS_TO_TICKS(5));
        ESP_RETURN_ON_ERROR(gpio_set_level(ctx->reset_gpio_num, ctx->reset_active_high),
                            TAG, "Assert reset GPIO failed");
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_RETURN_ON_ERROR(gpio_set_level(ctx->reset_gpio_num, !ctx->reset_active_high),
                            TAG, "Release reset GPIO failed");
    } else {
        ESP_RETURN_ON_ERROR(wt_bsp_dsi_jd9165ba_select_page_zero(ctx), TAG, "Select page failed");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(ctx->io, LCD_CMD_SWRESET, NULL, 0),
                            TAG, "Software reset failed");
    }
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_jd9165ba_init(esp_lcd_panel_t *panel)
{
    wt_bsp_dsi_jd9165ba_panel_t *ctx = panel->user_data;

    /* 此模组只发送命令, 不使用 DCS ID 读取. */
    for (size_t i = 0; i < sizeof(s_init_commands) / sizeof(s_init_commands[0]); ++i) {
        const wt_bsp_dsi_jd9165ba_init_cmd_t *command = &s_init_commands[i];
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(ctx->io, command->cmd, command->data, command->len),
                            TAG, "Init command %u (0x%02x) failed", (unsigned int)i, command->cmd);
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(ctx->io, LCD_CMD_MADCTL, &ctx->madctl, 1),
                        TAG, "MADCTL failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(ctx->io, LCD_CMD_SLPOUT, NULL, 0), TAG, "Sleep out failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_RETURN_ON_ERROR(ctx->dpi_init(panel), TAG, "Start DPI failed");
    ESP_LOGI(TAG, "Vendor sequence sent (%u registers), RGB565, 2 lanes",
             (unsigned int)(sizeof(s_init_commands) / sizeof(s_init_commands[0])));
    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_jd9165ba_disp_on_off(esp_lcd_panel_t *panel, bool on)
{
    wt_bsp_dsi_jd9165ba_panel_t *ctx = panel->user_data;

    ESP_RETURN_ON_ERROR(wt_bsp_dsi_jd9165ba_select_page_zero(ctx), TAG, "Select page failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(ctx->io, on ? LCD_CMD_DISPON : LCD_CMD_DISPOFF, NULL, 0),
                        TAG, "Display command failed");
    vTaskDelay(pdMS_TO_TICKS(20));
    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_jd9165ba_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    wt_bsp_dsi_jd9165ba_panel_t *ctx = panel->user_data;
    uint8_t madctl = ctx->madctl & ~(WT_BSP_DSI_JD9165BA_GS_BIT | WT_BSP_DSI_JD9165BA_SS_BIT);

    if (mirror_x) {
        madctl |= WT_BSP_DSI_JD9165BA_GS_BIT;
    }
    if (mirror_y) {
        madctl |= WT_BSP_DSI_JD9165BA_SS_BIT;
    }
    ESP_RETURN_ON_ERROR(wt_bsp_dsi_jd9165ba_select_page_zero(ctx), TAG, "Select page failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(ctx->io, LCD_CMD_MADCTL, &madctl, 1), TAG, "Mirror failed");
    ctx->madctl = madctl;
    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_jd9165ba_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    (void)panel;
    return swap_axes ? ESP_ERR_NOT_SUPPORTED : ESP_OK;
}

static esp_err_t wt_bsp_dsi_jd9165ba_invert(esp_lcd_panel_t *panel, bool invert)
{
    wt_bsp_dsi_jd9165ba_panel_t *ctx = panel->user_data;

    ESP_RETURN_ON_ERROR(wt_bsp_dsi_jd9165ba_select_page_zero(ctx), TAG, "Select page failed");
    return esp_lcd_panel_io_tx_param(ctx->io, invert ? LCD_CMD_INVON : LCD_CMD_INVOFF, NULL, 0);
}

#endif // WT_BSP_DSI_ENABLED
