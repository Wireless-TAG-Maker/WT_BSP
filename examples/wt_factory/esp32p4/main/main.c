/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/lock.h>
#include <sys/param.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"
#include "esp_heap_caps.h"
#include "driver/ppa.h"
#include "linux/videodev2.h"
#include "wt_bsp.h"

static const char *TAG = "factory_firmware";

/* External UI functions from lvgl_demo_ui.c */
extern void example_lvgl_demo_ui(lv_display_t *disp);
extern void update_camera_frame(uint8_t *buf, uint32_t width, uint32_t height);
extern void set_camera_error(const char *msg);
extern bool is_fullscreen;

/* Camera streaming state */
static ppa_client_handle_t s_ppa_srm_handle = NULL;
static uint8_t *s_ui_cam_buffer[2] = {NULL, NULL};
static uint8_t s_current_buf_idx = 0;
static bool s_csi_detected = false;

/**
 * @brief Callback for camera detection. Just sets flag on first frame.
 */
static void s_csi_detect_cb(uint8_t *buf, uint32_t width, uint32_t height, size_t len, void *user_data)
{
    s_csi_detected = true;
}

/**
 * @brief Camera frame callback. Processes frame using PPA and updates UI.
 */
static void camera_frame_cb(uint8_t *buf, uint32_t width, uint32_t height, size_t len, void *user_data)
{
    if (s_ppa_srm_handle && s_ui_cam_buffer[0] && s_ui_cam_buffer[1]) {
        ppa_srm_oper_config_t srm_config = {
            .in.buffer = buf,
            .in.pic_w = width,
            .in.pic_h = height,
            .in.srm_cm = PPA_SRM_COLOR_MODE_RGB565,
            .out.buffer = s_ui_cam_buffer[s_current_buf_idx],
            .out.srm_cm = PPA_SRM_COLOR_MODE_RGB888,
            .scale_x = 1.0f,
            .scale_y = 1.0f,
            .rgb_swap = 0,
            .byte_swap = 0,
            .mode = PPA_TRANS_MODE_BLOCKING,
        };

        uint32_t out_w, out_h;

        if (is_fullscreen) {
            /* Fullscreen: 640x480 block from input -> Rotate 90 -> 480x640 output */
            srm_config.in.block_w = 640;
            srm_config.in.block_h = 480;
            srm_config.in.block_offset_x = (width > 640) ? (width - 640) / 2 : 0;
            srm_config.in.block_offset_y = (height > 480) ? (height - 480) / 2 : 0;
            
            srm_config.out.pic_w = 480;
            srm_config.out.pic_h = 640;
            srm_config.out.buffer_size = 480 * 640 * 3;
            srm_config.rotation_angle = PPA_SRM_ROTATION_ANGLE_90;
            out_w = 480;
            out_h = 640;
        } else {
            /* Normal: 480x384 crop, no rotation */
            srm_config.in.block_w = 480;
            srm_config.in.block_h = 384;
            srm_config.in.block_offset_x = (width > 480) ? (width - 480) / 2 : 0;
            srm_config.in.block_offset_y = (height > 384) ? (height - 384) / 2 : 0;
            
            srm_config.out.pic_w = 480;
            srm_config.out.pic_h = 384;
            srm_config.out.buffer_size = 480 * 640 * 3; /* Keep max size */
            srm_config.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
            out_w = 480;
            out_h = 384;
        }

        if (ppa_do_scale_rotate_mirror(s_ppa_srm_handle, &srm_config) == ESP_OK) {
            /* Invalidate CPU cache (RGB888 is 3 bytes/pixel) */
            esp_cache_msync((void *)s_ui_cam_buffer[s_current_buf_idx], out_w * out_h * 3, ESP_CACHE_MSYNC_FLAG_DIR_M2C);

            if (wt_bsp_dsi_lvgl_lock(pdMS_TO_TICKS(10))) {
                update_camera_frame(s_ui_cam_buffer[s_current_buf_idx], out_w, out_h);
                wt_bsp_dsi_lvgl_unlock();
                s_current_buf_idx = !s_current_buf_idx;
            }
        }
    }
}

/* Functions required by lvgl_demo_ui.c implemented using BSP */

void example_set_led_color(uint8_t r, uint8_t g, uint8_t b)
{
    wt_bsp_rgb_t rgb = wt_bsp_get_rgb();
    if (rgb) {
        wt_bsp_rgb_set_pixel(rgb, 0, (wt_bsp_rgb_color_t){r, g, b});
        wt_bsp_rgb_refresh(rgb);
    }
}

esp_err_t example_sdcard_mount(void)
{
    /* Check if SD card is actually mounted */
    wt_bsp_sdmmc_t sdmmc = wt_bsp_get_sdmmc();
    if (sdmmc == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    /* Check is_mounted flag and card handle */
    if (!sdmmc->is_mounted || sdmmc->card == NULL) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

uint64_t example_sdcard_get_capacity(void)
{
    wt_bsp_sdmmc_t sdmmc = wt_bsp_get_sdmmc();
    if (sdmmc) {
        sdmmc_card_t *card = wt_bsp_sdmmc_get_card(sdmmc);
        if (card) {
            return ((uint64_t)card->csd.capacity) * card->csd.sector_size;
        }
    }
    return 0;
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
    wt_bsp_touch_t touch = wt_bsp_get_touch();
    wt_bsp_sdmmc_t sdmmc = wt_bsp_get_sdmmc();

    /* Hardware status detection */
    bool dsi_ok = (dsi != NULL && dsi->is_initialized);
    bool sdmmc_ok = (sdmmc != NULL && sdmmc->is_mounted);
    bool csi_initialized = (csi != NULL && csi->is_initialized);

    /* CSI status will be determined later when we actually try to start it */
    /* For LED indication, we assume CSI is OK if it initialized, will update if start fails */

    ESP_LOGI(TAG, "Hardware status: DSI=%d, CSI_init=%d, SDMMC=%d", dsi_ok, csi_initialized, sdmmc_ok);

    /* Set LED color based on initial hardware status (CSI will be verified later) */
    wt_bsp_rgb_t rgb = wt_bsp_get_rgb();
    if (rgb) {
        if (!dsi_ok && !csi_initialized && !sdmmc_ok) {
            /* Screen, camera, and SD card all not connected: RED */
            ESP_LOGW(TAG, "No peripherals detected - LED RED");
            wt_bsp_rgb_set_pixel(rgb, 0, (wt_bsp_rgb_color_t){255, 0, 0});
            wt_bsp_rgb_refresh(rgb);
        } else if (!csi_initialized && !sdmmc_ok) {
            /* Camera and SD card not connected (based on init): ORANGE */
            ESP_LOGW(TAG, "Camera and SD card not detected - LED ORANGE");
            wt_bsp_rgb_set_pixel(rgb, 0, (wt_bsp_rgb_color_t){255, 165, 0});
            wt_bsp_rgb_refresh(rgb);
        } else if (!csi_initialized) {
            /* Only camera not connected (based on init): BLUE */
            ESP_LOGW(TAG, "Camera not detected (init failed) - LED BLUE");
            wt_bsp_rgb_set_pixel(rgb, 0, (wt_bsp_rgb_color_t){0, 0, 255});
            wt_bsp_rgb_refresh(rgb);
        } else if (!sdmmc_ok) {
            /* Only SD card not connected: YELLOW */
            ESP_LOGW(TAG, "SD card not detected - LED YELLOW");
            wt_bsp_rgb_set_pixel(rgb, 0, (wt_bsp_rgb_color_t){255, 255, 0});
            wt_bsp_rgb_refresh(rgb);
        } else {
            /* All hardware initialized: no LED indication */
            ESP_LOGI(TAG, "All peripherals initialized - LED OFF");
        }
    }

    /* Check if display is available before proceeding with UI */
    if (!dsi_ok) {
        ESP_LOGW(TAG, "Display not available, skipping UI initialization");
        ESP_LOGI(TAG, "Factory firmware example running (no display mode)");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(100000));
        }
        return;
    }

    /* 1. Start LVGL */
    ESP_LOGI(TAG, "Starting LVGL");
    wt_bsp_dsi_lvgl_config_t lvgl_cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .double_buffer = true,
        /* Allocate buffer for 1/10th of the screen */
        .buffer_size = 480 * 640,
        .flags = {
            .avoid_tearing = false,
        }
    };
    lvgl_cfg.lvgl_port_cfg.task_stack = 16384;
    //lvgl_cfg.lvgl_port_cfg.task_priority = 10;
    lv_display_t *disp = wt_bsp_dsi_lvgl_start(dsi, &lvgl_cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Failed to start LVGL display");
        return;
    }

    /* 2. Setup Touch */
    if (touch) {
        wt_bsp_touch_lvgl_start(touch, disp);
    }

    /* 3. Setup UI */
    if (wt_bsp_dsi_lvgl_lock(portMAX_DELAY)) {
        example_lvgl_demo_ui(disp);
        wt_bsp_dsi_lvgl_unlock();
    }

    /* 4. Power on display backlight */
    wt_bsp_dsi_display_on(dsi);
    wt_bsp_dsi_set_brightness(dsi, 100);

    /* 5. Initialize PPA and Camera */
    if (csi) {
        ESP_LOGI(TAG, "Initializing PPA and Camera");
        ppa_client_handle_t ppa_srm_handle = NULL;
        ppa_client_config_t ppa_srm_config = {
            .oper_type = PPA_OPERATION_SRM,
        };
        if (ppa_register_client(&ppa_srm_config, &ppa_srm_handle) == ESP_OK) {
            s_ppa_srm_handle = ppa_srm_handle;
            size_t data_cache_line_size = 0;
            esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size);
            /* Buffers sized for 480x640 RGB888 fullscreen support */
            s_ui_cam_buffer[0] = heap_caps_aligned_calloc(data_cache_line_size, 1, 480 * 640 * 3, MALLOC_CAP_SPIRAM);
            s_ui_cam_buffer[1] = heap_caps_aligned_calloc(data_cache_line_size, 1, 480 * 640 * 3, MALLOC_CAP_SPIRAM);
        }

        /* Configure camera to output RGB565 for the factory UI */
        wt_bsp_csi_info_t *csi_info = &csi->info;
        csi_info->pixel_format = V4L2_PIX_FMT_RGB565;

        ret = wt_bsp_csi_start(csi, camera_frame_cb, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start camera: %s", esp_err_to_name(ret));
            if (wt_bsp_dsi_lvgl_lock(portMAX_DELAY)) {
                set_camera_error("Camera Init Failed");
                wt_bsp_dsi_lvgl_unlock();
            }
        }
    }

    ESP_LOGI(TAG, "Factory firmware example running");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100000));
    }
}
