/**
 * @file wt_bsp_rgb.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-05-11
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "wt_bsp_rgb.h"

#if WT_BSP_RGB_ENABLE_IS_ENABLED

#include <string.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "soc/soc_caps.h"

/* ==================== [Defines] =========================================== */

#define WT_BSP_RGB_DEFAULT_RMT_RESOLUTION_HZ 10000000U
#define WT_BSP_RGB_DEFAULT_RMT_MEM_BLOCK_SYMBOLS 0
#define WT_BSP_RGB_DEFAULT_RMT_WITH_DMA false
#define WT_BSP_RGB_DEFAULT_SPI_BUS SPI2_HOST
#define WT_BSP_RGB_DEFAULT_SPI_WITH_DMA true
#define WT_BSP_RGB_DEFAULT_AUTO_REFRESH true
#define WT_BSP_RGB_DEFAULT_CLEAR_ON_INIT true

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static esp_err_t wt_bsp_rgb_write_pixel(wt_bsp_rgb_t rgb, uint32_t index, wt_bsp_rgb_color_t color);
static esp_err_t wt_bsp_rgb_refresh_if_needed(wt_bsp_rgb_t rgb);
static void wt_bsp_rgb_cleanup(wt_bsp_rgb_t rgb);
static led_color_component_format_t wt_bsp_rgb_get_component_format(wt_bsp_rgb_format_t format);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "wt_bsp_rgb";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t wt_bsp_rgb_init(wt_bsp_rgb_t rgb, const wt_bsp_rgb_info_t *info)
{
    esp_err_t ret = ESP_OK;

    if (rgb == NULL || info == NULL) {
        ESP_LOGE(TAG, "Invalid argument: rgb=%p, info=%p", rgb, info);
        return ESP_ERR_INVALID_ARG;
    }

    if (info == NULL) {
        ESP_LOGE(TAG, "RGB info is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!GPIO_IS_VALID_OUTPUT_GPIO(info->gpio_num)) {
        ESP_LOGE(TAG, "Invalid GPIO number: %d", info->gpio_num);
        return ESP_ERR_INVALID_ARG;
    }

    if (info->model > WT_BSP_RGB_MODEL_WS2816 || info->format >= _WT_BSP_RGB_FORMAT_MAX) {
        ESP_LOGE(TAG, "Invalid RGB model or format");
        return ESP_ERR_INVALID_ARG;
    }

    rgb->info = *info;
    rgb->auto_refresh = WT_BSP_RGB_DEFAULT_AUTO_REFRESH;
    rgb->strip = NULL;
    memset(rgb->pixels, 0, sizeof(rgb->pixels));

    led_strip_config_t strip_config = {
        .strip_gpio_num = rgb->info.gpio_num,
        .max_leds = WT_BSP_RGB_NUM,
        .led_model = (led_model_t)rgb->info.model,
        .color_component_format = wt_bsp_rgb_get_component_format(rgb->info.format),
        .flags.invert_out = rgb->info.invert_out,
    };

#if SOC_RMT_SUPPORTED
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = WT_BSP_RGB_DEFAULT_RMT_RESOLUTION_HZ,
        .mem_block_symbols = WT_BSP_RGB_DEFAULT_RMT_MEM_BLOCK_SYMBOLS,
        .flags.with_dma = WT_BSP_RGB_DEFAULT_RMT_WITH_DMA,
    };

    ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &rgb->strip);
#elif SOC_GPSPI_SUPPORTED
    led_strip_spi_config_t spi_config = {
        .clk_src = SPI_CLK_SRC_DEFAULT,
        .spi_bus = WT_BSP_RGB_DEFAULT_SPI_BUS,
        .flags.with_dma = WT_BSP_RGB_DEFAULT_SPI_WITH_DMA,
    };

    ret = led_strip_new_spi_device(&strip_config, &spi_config, &rgb->strip);
#else
    ESP_LOGE(TAG, "no supported LED strip backend");
    wt_bsp_rgb_cleanup(rgb);
    return ESP_ERR_NOT_SUPPORTED;
#endif
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "create led strip failed: %s", esp_err_to_name(ret));
        wt_bsp_rgb_cleanup(rgb);
        return ret;
    }

    ret = led_strip_clear(rgb->strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "clear led strip failed: %s", esp_err_to_name(ret));
        wt_bsp_rgb_cleanup(rgb);
        return ret;
    }

    return ESP_OK;
}

esp_err_t wt_bsp_rgb_deinit(wt_bsp_rgb_t rgb)
{
    if (rgb == NULL) {
        ESP_LOGE(TAG, "RGB is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    wt_bsp_rgb_cleanup(rgb);
    return ESP_OK;
}

esp_err_t wt_bsp_rgb_set_auto_refresh(wt_bsp_rgb_t rgb, bool enable)
{
    if (rgb == NULL) {
        ESP_LOGE(TAG, "RGB is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    rgb->auto_refresh = enable;
    return ESP_OK;
}

esp_err_t wt_bsp_rgb_set_pixel(wt_bsp_rgb_t rgb, uint32_t index, wt_bsp_rgb_color_t color)
{
    esp_err_t ret = ESP_OK;

    if (rgb == NULL || index >= WT_BSP_RGB_NUM) {
        ESP_LOGE(TAG, "Invalid argument: rgb=%p, index=%lu", rgb, (unsigned long)index);
        return ESP_ERR_INVALID_ARG;
    }

    rgb->pixels[index] = color;
    ret = wt_bsp_rgb_write_pixel(rgb, index, color);
    if (ret != ESP_OK) {
        return ret;
    }

    return wt_bsp_rgb_refresh_if_needed(rgb);
}

esp_err_t wt_bsp_rgb_set_color(wt_bsp_rgb_t rgb, wt_bsp_rgb_color_t color)
{
    esp_err_t ret = ESP_OK;

    if (rgb == NULL) {
        ESP_LOGE(TAG, "RGB is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    for (uint32_t i = 0; i < WT_BSP_RGB_NUM; i++) {
        rgb->pixels[i] = color;
        ret = wt_bsp_rgb_write_pixel(rgb, i, color);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return wt_bsp_rgb_refresh_if_needed(rgb);
}

esp_err_t wt_bsp_rgb_refresh(wt_bsp_rgb_t rgb)
{
    if (rgb == NULL) {
        ESP_LOGE(TAG, "RGB is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    return led_strip_refresh(rgb->strip);
}

esp_err_t wt_bsp_rgb_clear(wt_bsp_rgb_t rgb)
{
    if (rgb == NULL) {
        ESP_LOGE(TAG, "RGB is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(rgb->pixels, 0, WT_BSP_RGB_NUM * sizeof(*rgb->pixels));
    return led_strip_clear(rgb->strip);
}

/* ==================== [Static Functions] ================================== */

static esp_err_t wt_bsp_rgb_write_pixel(wt_bsp_rgb_t rgb, uint32_t index, wt_bsp_rgb_color_t color)
{
#if !WT_BSP_RGBW_ENABLE_IS_ENABLED
    return led_strip_set_pixel(rgb->strip,
                               index,
                               color.r,
                               color.g,
                               color.b);
#else
    return led_strip_set_pixel_rgbw(rgb->strip,
                                    index,
                                    color.r,
                                    color.g,
                                    color.b,
                                    color.w);
#endif
}

static esp_err_t wt_bsp_rgb_refresh_if_needed(wt_bsp_rgb_t rgb)
{
    if (!rgb->auto_refresh) {
        return ESP_OK;
    }

    return led_strip_refresh(rgb->strip);
}

static void wt_bsp_rgb_cleanup(wt_bsp_rgb_t rgb)
{
    if (rgb == NULL) {
        ESP_LOGE(TAG, "RGB is NULL");
        return;
    }

    if (rgb->strip != NULL) {
        esp_err_t ret = led_strip_del(rgb->strip);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "delete led strip failed: %s", esp_err_to_name(ret));
        }
        rgb->strip = NULL;
    }

    memset(rgb->pixels, 0, sizeof(rgb->pixels));
}

static led_color_component_format_t wt_bsp_rgb_get_component_format(wt_bsp_rgb_format_t format)
{
    switch (format) {
    case WT_BSP_RGB_FORMAT_RGB:
        return LED_STRIP_COLOR_COMPONENT_FMT_RGB;
#if WT_BSP_RGBW_ENABLE_IS_ENABLED
    case WT_BSP_RGB_FORMAT_GRBW:
        return LED_STRIP_COLOR_COMPONENT_FMT_GRBW;
    case WT_BSP_RGB_FORMAT_RGBW:
        return LED_STRIP_COLOR_COMPONENT_FMT_RGBW;
#endif
    case WT_BSP_RGB_FORMAT_GRB:
    default:
        return LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    }
}

#endif // WT_BSP_RGB_ENABLE_IS_ENABLED
