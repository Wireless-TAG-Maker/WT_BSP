/**
 * @file main.c
 * @brief CSI 摄像头采集并显示在 DSI 屏上的示例。
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "driver/ppa.h"
#include "wt_bsp.h"

static const char *TAG = "csi_example";

#define PPA_SCALE_FRACTION_STEPS 256U
#define CSI_DISPLAY_FB_NUM 2U
#define CSI_REFRESH_WAIT_TIMEOUT_MS 50U

static ppa_client_handle_t s_ppa_client = NULL;
static uint8_t *s_display_frame_buffers[CSI_DISPLAY_FB_NUM] = {NULL};
static uint32_t s_disp_width = 0;
static uint32_t s_disp_height = 0;
static size_t s_display_frame_buffer_size = 0;
static uint8_t s_next_frame_buffer_index = 1;
static SemaphoreHandle_t s_refresh_done_sem = NULL;
static volatile uint32_t s_refresh_done_count = 0;

typedef struct {
    uint32_t input_width;
    uint32_t input_height;
    uint32_t crop_width;
    uint32_t crop_height;
    float scale_x;
    float scale_y;
} csi_display_transform_t;

static csi_display_transform_t s_transform = {0};

/**
 * @brief DSI 刷新完成回调。
 */
IRAM_ATTR static bool display_refresh_done_cb(esp_lcd_panel_handle_t panel,
                                              esp_lcd_dpi_panel_event_data_t *edata,
                                              void *user_ctx)
{
    (void)panel;
    (void)edata;

    BaseType_t task_woken = pdFALSE;
    SemaphoreHandle_t refresh_done_sem = (SemaphoreHandle_t)user_ctx;

    s_refresh_done_count++;

    if (refresh_done_sem != NULL) {
        xSemaphoreGiveFromISR(refresh_done_sem, &task_woken);
    }

    return task_woken == pdTRUE;
}

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
 * @brief 更新当前摄像头输入尺寸对应的裁剪和缩放参数。
 */
static void update_display_transform(uint32_t width, uint32_t height)
{
    if (s_transform.input_width == width && s_transform.input_height == height) {
        return;
    }

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

    s_transform = (csi_display_transform_t) {
        .input_width = width,
        .input_height = height,
        .crop_width = crop_width,
        .crop_height = crop_height,
        .scale_x = scale_x,
        .scale_y = scale_y,
    };
}

/**
 * @brief 等待 DSI 完成一次新的刷新，避免过早复用正在扫描的 frame buffer。
 */
static void wait_display_refresh_done(uint32_t previous_refresh_count)
{
    if (s_refresh_done_sem == NULL) {
        return;
    }

    while (s_refresh_done_count == previous_refresh_count) {
        if (xSemaphoreTake(s_refresh_done_sem, pdMS_TO_TICKS(CSI_REFRESH_WAIT_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGW(TAG, "Timed out waiting for DSI refresh done");
            return;
        }
    }
}

/**
 * @brief 摄像头帧回调函数。
 */
static void camera_frame_cb(uint8_t *buf, uint32_t width, uint32_t height, size_t len, void *user_data)
{
    wt_bsp_dsi_t dsi = (wt_bsp_dsi_t)user_data;
    esp_lcd_panel_handle_t panel_handle = wt_bsp_dsi_get_panel_handle(dsi);

    (void)len;

    if (panel_handle && s_ppa_client && s_display_frame_buffers[0] && s_display_frame_buffers[1]) {
        update_display_transform(width, height);

        uint8_t *display_frame_buffer = s_display_frame_buffers[s_next_frame_buffer_index];

        ppa_srm_oper_config_t srm_config = {
            .in.buffer = buf,
            .in.pic_w = width,
            .in.pic_h = height,
            .in.block_w = s_transform.crop_width,
            .in.block_h = s_transform.crop_height,
            .in.block_offset_x = (width - s_transform.crop_width) / 2,
            .in.block_offset_y = (height - s_transform.crop_height) / 2,
            .in.srm_cm = PPA_SRM_COLOR_MODE_RGB888,
            .out.buffer = display_frame_buffer,
            .out.buffer_size = s_display_frame_buffer_size,
            .out.pic_w = s_disp_width,
            .out.pic_h = s_disp_height,
            .out.block_offset_x = 0,
            .out.block_offset_y = 0,
            .out.srm_cm = PPA_SRM_COLOR_MODE_RGB888,
            .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
            .scale_x = s_transform.scale_x,
            .scale_y = s_transform.scale_y,
            .mode = PPA_TRANS_MODE_BLOCKING,
        };

        if (ppa_do_scale_rotate_mirror(s_ppa_client, &srm_config) == ESP_OK) {
            esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_handle,
                                                      0,
                                                      0,
                                                      s_disp_width,
                                                      s_disp_height,
                                                      display_frame_buffer);
            if (ret == ESP_OK) {
                uint32_t refresh_count = s_refresh_done_count;
                wait_display_refresh_done(refresh_count);
                s_next_frame_buffer_index = (s_next_frame_buffer_index + 1) % CSI_DISPLAY_FB_NUM;
            } else {
                ESP_LOGW(TAG, "Failed to draw camera frame: %s", esp_err_to_name(ret));
            }
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
    s_display_frame_buffer_size = s_disp_width * s_disp_height * 3;

    esp_lcd_panel_handle_t panel_handle = wt_bsp_dsi_get_panel_handle(dsi);
    if (panel_handle == NULL) {
        ESP_LOGE(TAG, "Failed to get DSI panel handle");
        return;
    }

    ret = esp_lcd_dpi_panel_get_frame_buffer(panel_handle,
                                             CSI_DISPLAY_FB_NUM,
                                             (void **)&s_display_frame_buffers[0],
                                             (void **)&s_display_frame_buffers[1]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get DSI frame buffers: %s", esp_err_to_name(ret));
        return;
    }

    s_refresh_done_sem = xSemaphoreCreateBinary();
    if (s_refresh_done_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create refresh done semaphore");
        return;
    }

    esp_lcd_dpi_panel_event_callbacks_t panel_callbacks = {
        .on_refresh_done = display_refresh_done_cb,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(panel_handle, &panel_callbacks, s_refresh_done_sem));

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
