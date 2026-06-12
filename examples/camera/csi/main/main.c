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
#include "driver/ppa.h"
#include "wt_bsp.h"

static const char *TAG = "csi_example";

static ppa_client_handle_t s_ppa_client = NULL;
static uint8_t *s_ui_buffer = NULL;
static uint32_t s_disp_width = 0;
static uint32_t s_disp_height = 0;

/**
 * @brief 摄像头帧回调函数。
 */
static void camera_frame_cb(uint8_t *buf, uint32_t width, uint32_t height, size_t len, void *user_data)
{
    wt_bsp_dsi_t dsi = (wt_bsp_dsi_t)user_data;
    esp_lcd_panel_handle_t panel_handle = wt_bsp_dsi_get_panel_handle(dsi);

    if (panel_handle && s_ppa_client && s_ui_buffer) {
        /* 使用 PPA 硬件将 1024x600 裁剪并缩放到 480x640 */
        ppa_srm_oper_config_t srm_config = {
            .in.buffer = buf,
            .in.pic_w = width,
            .in.pic_h = height,
            .in.block_w = s_disp_width,
            .in.block_h = height > s_disp_height ? s_disp_height : height,
            .in.block_offset_x = (width > s_disp_width) ? (width - s_disp_width) / 2 : 0,
            .in.block_offset_y = (height > s_disp_height) ? (height - s_disp_height) / 2 : 0,
            .in.srm_cm = PPA_SRM_COLOR_MODE_RGB888,
            .out.buffer = s_ui_buffer,
            .out.buffer_size = s_disp_width * s_disp_height * 3, // RGB888 is 3 bytes per pixel
            .out.pic_w = s_disp_width,
            .out.pic_h = s_disp_height,
            .out.block_offset_x = 0,
            .out.block_offset_y = 0,
            .out.srm_cm = PPA_SRM_COLOR_MODE_RGB888,
            .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
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

    wt_bsp_dsi_get_resolution(dsi, (uint16_t *)&s_disp_width, (uint16_t *)&s_disp_height);
    s_ui_buffer = heap_caps_calloc(1, s_disp_width * s_disp_height * 3, MALLOC_CAP_SPIRAM);
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
