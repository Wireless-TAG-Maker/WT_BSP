/**
 * @file main.c
 * @brief CSI 摄像头采集并显示在 DSI 屏上的示例。
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"
#include "driver/ppa.h"
#include "wt_bsp.h"

static const char *TAG = "csi_example";

#define PPA_SCALE_FRACTION_STEPS 256U

static ppa_client_handle_t s_ppa_client = NULL;
static uint8_t *s_ui_buffer = NULL;
static uint32_t s_disp_width = 0;
static uint32_t s_disp_height = 0;
static size_t s_data_cache_line_size = 0;
static size_t s_ui_buffer_size = 0;

/**
 * @brief 计算能够精确覆盖目标轴的 PPA 裁剪尺寸和缩放系数。
 */
static void calculate_axis_scale(uint32_t output_size, uint32_t *crop_size, float *scale)
{
    for (uint32_t size = *crop_size; size > 0; size--) {
        uint32_t scale_fixed =
            (uint32_t)(((uint64_t)output_size * PPA_SCALE_FRACTION_STEPS + size - 1) / size);

        if ((uint64_t)size * scale_fixed / PPA_SCALE_FRACTION_STEPS == output_size) {
            *crop_size = size;
            *scale = (float)scale_fixed / PPA_SCALE_FRACTION_STEPS;
            return;
        }
    }

    *scale = (float)output_size / *crop_size;
}

/**
 * @brief 摄像头帧回调函数。
 */
static void camera_frame_cb(uint8_t *buf, uint32_t width, uint32_t height, size_t len, void *user_data)
{
    wt_bsp_dsi_t dsi = (wt_bsp_dsi_t)user_data;
    esp_lcd_panel_handle_t panel_handle = wt_bsp_dsi_get_panel_handle(dsi);

    if (panel_handle && s_ppa_client && s_ui_buffer) {
        uint32_t crop_width = width;
        uint32_t crop_height = height;

        /*
         * 按屏幕纵横比居中裁剪输入画面，再等比缩放至全屏。
         * PPA 的缩放系数精度为 1/256，后续会微调裁剪尺寸，
         * 确保缩放结果完整覆盖屏幕，避免底部留下黑边。
         */
        if ((uint64_t)width * s_disp_height > (uint64_t)height * s_disp_width) {
            crop_width = (uint32_t)((uint64_t)height * s_disp_width / s_disp_height);
        } else {
            crop_height = (uint32_t)((uint64_t)width * s_disp_height / s_disp_width);
        }

        float scale_x = 1.0f;
        float scale_y = 1.0f;
        calculate_axis_scale(s_disp_width, &crop_width, &scale_x);
        calculate_axis_scale(s_disp_height, &crop_height, &scale_y);

        ppa_srm_oper_config_t srm_config = {
            .in.buffer = buf,
            .in.pic_w = width,
            .in.pic_h = height,
            .in.block_w = crop_width,
            .in.block_h = crop_height,
            .in.block_offset_x = (width - crop_width) / 2,
            .in.block_offset_y = (height - crop_height) / 2,
            .in.srm_cm = PPA_SRM_COLOR_MODE_RGB888,
            .out.buffer = s_ui_buffer,
            .out.buffer_size = s_ui_buffer_size,
            .out.pic_w = s_disp_width,
            .out.pic_h = s_disp_height,
            .out.block_offset_x = 0,
            .out.block_offset_y = 0,
            .out.srm_cm = PPA_SRM_COLOR_MODE_RGB888,
            .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
            .scale_x = scale_x,
            .scale_y = scale_y,
            .mode = PPA_TRANS_MODE_BLOCKING,
        };

        if (ppa_do_scale_rotate_mirror(s_ppa_client, &srm_config) == ESP_OK) {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, s_disp_width, s_disp_height, s_ui_buffer);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing Wireless-Tag BSP");
    
    esp_err_t ret = wt_bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP init failed: %s", esp_err_to_name(ret));
        return;
    }

    wt_bsp_dsi_t dsi = wt_bsp_get_dsi();
    wt_bsp_csi_t csi = wt_bsp_get_csi();

    if (dsi == NULL || csi == NULL) {
        ESP_LOGE(TAG, "Failed to get DSI or CSI handle. Make sure they are enabled in board_config.h");
        return;
    }

    /* 初始化 PPA 用于图像裁剪和格式转换 */
    ppa_client_config_t ppa_client_config = {
        .oper_type = PPA_OPERATION_SRM,
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_client_config, &s_ppa_client));

    uint16_t disp_width = 0;
    uint16_t disp_height = 0;
    ESP_ERROR_CHECK(wt_bsp_dsi_get_resolution(dsi, &disp_width, &disp_height));
    s_disp_width = disp_width;
    s_disp_height = disp_height;
    
    esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &s_data_cache_line_size);
    s_ui_buffer_size =
        (s_disp_width * s_disp_height * 3 + s_data_cache_line_size - 1) & ~(s_data_cache_line_size - 1);
    
    s_ui_buffer = heap_caps_aligned_calloc(s_data_cache_line_size, 1, s_ui_buffer_size, MALLOC_CAP_SPIRAM);
    if (!s_ui_buffer) {
        ESP_LOGE(TAG, "Failed to allocate UI buffer");
        return;
    }

    /* 点亮屏幕并设置亮度 */
    ESP_LOGI(TAG, "Setting up DSI display");
    wt_bsp_dsi_display_on(dsi);
    wt_bsp_dsi_set_brightness(dsi, 100);

    /* 开始 CSI 视频流采集 */
    ESP_LOGI(TAG, "Starting CSI camera stream");
    ret = wt_bsp_csi_start(csi, camera_frame_cb, dsi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start CSI stream: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "System ready. Camera feed should be visible on the screen.");

    /* 主循环，监控系统状态 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
