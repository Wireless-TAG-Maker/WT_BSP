/**
 * @file wt_bsp_dsi_ili9488.c
 * @author Wireless-Tag
 * @brief EQT350XJI718P 模组的 ILI9488 初始化与 DPI 包装.
 * @version 0.1
 * @date 2026-11-20
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_dsi_ili9488.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdlib.h>

#if WT_BSP_DSI_ENABLED

/* ==================== [Defines] =========================================== */

#define WT_BSP_DSI_ILI9488_MADCTL_MX  0x40
#define WT_BSP_DSI_ILI9488_MADCTL_MY  0x80
#define WT_BSP_DSI_ILI9488_MADCTL_ML  0x10
/* Scan rows in the same order as the module's bottom-to-top image data. */
#define WT_BSP_DSI_EQT350_MADCTL      (WT_BSP_DSI_ILI9488_MADCTL_MY | WT_BSP_DSI_ILI9488_MADCTL_ML | LCD_CMD_BGR_BIT)

/* ==================== [Typedefs] ========================================== */

typedef struct {
    uint8_t command;
    uint8_t data[15];
    uint8_t size;
    uint16_t delay_ms;
} wt_bsp_dsi_ili9488_init_cmd_t;

typedef struct {
    esp_lcd_panel_io_handle_t io;
    int reset_gpio;
    esp_err_t (*dpi_init)(esp_lcd_panel_t *panel);
    esp_err_t (*dpi_del)(esp_lcd_panel_t *panel);
} wt_bsp_dsi_ili9488_panel_t;

/* ==================== [Static Prototypes] ================================= */

static esp_err_t wt_bsp_dsi_ili9488_reset(esp_lcd_panel_t *panel);
static esp_err_t wt_bsp_dsi_ili9488_init(esp_lcd_panel_t *panel);
static esp_err_t wt_bsp_dsi_ili9488_display_on_off(esp_lcd_panel_t *panel, bool on);
static esp_err_t wt_bsp_dsi_ili9488_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t wt_bsp_dsi_ili9488_swap_xy(esp_lcd_panel_t *panel, bool swap);
static esp_err_t wt_bsp_dsi_ili9488_delete(esp_lcd_panel_t *panel);

/* ==================== [Static Variables] ================================== */

/* EQT350XJI718P vendor DCS sequence with module-specific row orientation. */
static const wt_bsp_dsi_ili9488_init_cmd_t s_panel_init_commands[] = {
    {
        .command = 0x11,
        .delay_ms = 250,
    },
    {
        .command = 0xe0,
        .data = {0x00, 0x12, 0x17, 0x05, 0x11, 0x07, 0x3f,
                 0x56, 0x52, 0x04, 0x0c, 0x0a, 0x32, 0x36, 0x0f},
        .size = 15,
    },
    {
        .command = 0xe1,
        .data = {0x00, 0x0c, 0x10, 0x03, 0x0f, 0x06, 0x2e,
                 0x36, 0x42, 0x03, 0x0c, 0x0a, 0x28, 0x2e, 0x0f},
        .size = 15,
    },
    {
        .command = 0xc0,
        .data = {0x15, 0x15},
        .size = 2,
    },
    {
        .command = 0xc1,
        .data = {0x47},
        .size = 1,
    },
    {
        .command = 0xc5,
        .data = {0x00, 0x7e, 0x80},
        .size = 3,
    },
    {
        .command = 0x36,
        .data = {WT_BSP_DSI_EQT350_MADCTL},
        .size = 1,
    },
    {
        .command = 0x3a,
        .data = {0x55},
        .size = 1,
    },
    {
        .command = 0xb0,
        .data = {0x00},
        .size = 1,
    },
    {
        .command = 0xb1,
        .data = {0xa0},
        .size = 1,
    },
    {
        .command = 0xb4,
        .data = {0x00},
        .size = 1,
    },
    {
        .command = 0xb6,
        .data = {0x02, 0x02},
        .size = 2,
    },
    {
        .command = 0xb7,
        .data = {0xc6},
        .size = 1,
    },
    {
        .command = 0xe9,
        .data = {0x00},
        .size = 1,
    },
    {
        .command = 0xf8,
        .data = {0x21, 0x01},
        .size = 2,
    },
    {
        .command = 0xf7,
        .data = {0xa9, 0x51, 0x2c, 0x82},
        .size = 4,
    },
    {
        .command = 0x21,
    },
    {
        .command = 0x35,
        .data = {0x00},
        .size = 1,
    },
    {
        .command = 0x29,
        .delay_ms = 200,
    },
};

static const char *TAG = "wt_bsp_dsi_ili9488";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_dsi_new_panel_ili9488(esp_lcd_dsi_bus_handle_t bus, esp_lcd_panel_io_handle_t io,
                                       const esp_lcd_dpi_panel_config_t *dpi, int reset_gpio,
                                       esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(bus && io && dpi && ret_panel, ESP_ERR_INVALID_ARG, TAG, "Invalid panel configuration");
    *ret_panel = NULL;
    ESP_RETURN_ON_FALSE(reset_gpio < 0 || GPIO_IS_VALID_OUTPUT_GPIO(reset_gpio),
                        ESP_ERR_INVALID_ARG, TAG, "Invalid reset GPIO");
    wt_bsp_dsi_ili9488_panel_t *ctx = calloc(1, sizeof(*ctx));
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_NO_MEM, TAG, "Allocate panel context failed");
    ctx->io = io;
    ctx->reset_gpio = reset_gpio;

    esp_err_t ret = ESP_OK;
    if (reset_gpio >= 0) {
        const gpio_config_t reset_config = {
            .pin_bit_mask = 1ULL << reset_gpio,
            .mode = GPIO_MODE_OUTPUT,
        };
        ret = gpio_config(&reset_config);
        if (ret != ESP_OK) {
            free(ctx);
            return ret;
        }
    }

    ret = esp_lcd_new_panel_dpi(bus, dpi, ret_panel);
    if (ret != ESP_OK) {
        if (reset_gpio >= 0) {
            gpio_reset_pin(reset_gpio);
        }
        free(ctx);
        return ret;
    }

    ctx->dpi_init = (*ret_panel)->init;
    ctx->dpi_del = (*ret_panel)->del;
    (*ret_panel)->init = wt_bsp_dsi_ili9488_init;
    (*ret_panel)->reset = wt_bsp_dsi_ili9488_reset;
    (*ret_panel)->del = wt_bsp_dsi_ili9488_delete;
    (*ret_panel)->disp_on_off = wt_bsp_dsi_ili9488_display_on_off;
    (*ret_panel)->mirror = wt_bsp_dsi_ili9488_mirror;
    (*ret_panel)->swap_xy = wt_bsp_dsi_ili9488_swap_xy;
    (*ret_panel)->user_data = ctx;
    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static esp_err_t wt_bsp_dsi_ili9488_reset(esp_lcd_panel_t *panel)
{
    wt_bsp_dsi_ili9488_panel_t *ctx = panel->user_data;
    if (ctx->reset_gpio >= 0) {
        ESP_RETURN_ON_ERROR(gpio_set_level(ctx->reset_gpio, 1), TAG, "Reset high failed");
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_RETURN_ON_ERROR(gpio_set_level(ctx->reset_gpio, 0), TAG, "Reset low failed");
        vTaskDelay(pdMS_TO_TICKS(20));
        ESP_RETURN_ON_ERROR(gpio_set_level(ctx->reset_gpio, 1), TAG, "Reset release failed");
    } else {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(ctx->io, LCD_CMD_SWRESET, NULL, 0),
                            TAG, "Software reset failed");
    }
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}

static esp_err_t wt_bsp_dsi_ili9488_init(esp_lcd_panel_t *panel)
{
    wt_bsp_dsi_ili9488_panel_t *ctx = panel->user_data;
    for (size_t i = 0; i < sizeof(s_panel_init_commands) / sizeof(s_panel_init_commands[0]); ++i) {
        const wt_bsp_dsi_ili9488_init_cmd_t *cmd = &s_panel_init_commands[i];
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(ctx->io, cmd->command, cmd->data, cmd->size),
                            TAG, "DCS initialization command 0x%02x failed", cmd->command);
        if (cmd->delay_ms != 0) {
            vTaskDelay(pdMS_TO_TICKS(cmd->delay_ms));
        }
    }

    return ctx->dpi_init(panel);
}

static esp_err_t wt_bsp_dsi_ili9488_display_on_off(esp_lcd_panel_t *panel, bool on)
{
    wt_bsp_dsi_ili9488_panel_t *ctx = panel->user_data;
    esp_err_t ret = esp_lcd_panel_io_tx_param(ctx->io, on ? LCD_CMD_DISPON : LCD_CMD_DISPOFF, NULL, 0);
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return ret;
}

static esp_err_t wt_bsp_dsi_ili9488_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    wt_bsp_dsi_ili9488_panel_t *ctx = panel->user_data;
    uint8_t madctl = WT_BSP_DSI_EQT350_MADCTL;
    /* Interpret the port's flags relative to the panel's native orientation. */
    if (mirror_x) {
        madctl ^= WT_BSP_DSI_ILI9488_MADCTL_MX;
    }
    if (mirror_y) {
        madctl ^= WT_BSP_DSI_ILI9488_MADCTL_MY | WT_BSP_DSI_ILI9488_MADCTL_ML;
    }
    return esp_lcd_panel_io_tx_param(ctx->io, LCD_CMD_MADCTL, &madctl, 1);
}

static esp_err_t wt_bsp_dsi_ili9488_swap_xy(esp_lcd_panel_t *panel, bool swap)
{
    (void)panel;
    /* A swapped video stream also needs different DPI geometry and timing. */
    return swap ? ESP_ERR_NOT_SUPPORTED : ESP_OK;
}

static esp_err_t wt_bsp_dsi_ili9488_delete(esp_lcd_panel_t *panel)
{
    wt_bsp_dsi_ili9488_panel_t *ctx = panel->user_data;
    esp_err_t ret = ctx->dpi_del(panel);
    if (ret != ESP_OK) {
        return ret;
    }
    if (ctx->reset_gpio >= 0) {
        gpio_reset_pin(ctx->reset_gpio);
    }
    free(ctx);
    return ESP_OK;
}

#endif // WT_BSP_DSI_ENABLED
