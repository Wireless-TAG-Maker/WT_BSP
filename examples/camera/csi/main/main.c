/**
 * @file main.c
 * @brief CSI 摄像头采集并显示在 DSI 屏上的示例。
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "wt_bsp.h"

static const char *TAG = "csi_example";

/**
 * @brief 摄像头帧回调函数。
 * 
 * 当 CSI 采集到一帧数据时，该函数会被调用。
 * 我们在这里直接将数据刷到 DSI 屏幕上。
 */
static void camera_frame_cb(uint8_t *buf, uint32_t width, uint32_t height, size_t len, void *user_data)
{
    wt_bsp_dsi_t dsi = (wt_bsp_dsi_t)user_data;
    esp_lcd_panel_handle_t panel_handle = wt_bsp_dsi_get_panel_handle(dsi);

    if (panel_handle) {
        /* 将采集到的 RGB565 数据直接绘制到屏幕上 */
        esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, width, height, buf);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing Wireless-Tag BSP");
    
    /* 初始化 BSP，内部会自动初始化 DSI 屏和 CSI 摄像头控制器 */
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
