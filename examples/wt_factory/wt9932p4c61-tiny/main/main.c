/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <time.h>
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
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/ppa.h"
#include "linux/videodev2.h"
#include "wifi_manager_bridge.h"
#include "wt_bsp.h"

static const char *TAG = "factory_firmware";

#define WIFI_TASK_CONFIG_BIT     BIT0
#define WIFI_TASK_CONNECTED_BIT  BIT1
#define WIFI_TASK_STATION_BIT    BIT2
#define WIFI_TASK_CONNECTING_BIT BIT3
#define WIFI_TASK_DISCONNECTED_BIT BIT4
#define WIFI_TASK_CONFIG_MODE_BIT BIT5
#define WIFI_TEXT_BUFFER_SIZE    64

/* External UI functions from lvgl_demo_ui.c */
extern void lvgl_ui(lv_display_t *disp);
extern void update_camera_frame(uint8_t *buf, uint32_t width, uint32_t height);
extern void set_camera_error(const char *msg);
extern void update_led_color(uint8_t r, uint8_t g, uint8_t b);
extern void update_wifi_status(bool connected, const char *ap_name, const char *time_text);
extern bool is_fullscreen;

/* Camera streaming state */
static ppa_client_handle_t s_ppa_srm_handle = NULL;
static uint8_t *s_ui_cam_buffer[2] = {NULL, NULL};
static uint8_t s_current_buf_idx = 0;
static bool s_csi_detected = false;
static TaskHandle_t s_wifi_task_handle = NULL;
static bool s_config_button_triggered = false;
static bool s_ui_ready = false;
static uint8_t s_led_red = 0;
static uint8_t s_led_green = 0;
static uint8_t s_led_blue = 0;

static esp_err_t initialize_wifi(void);
static void set_status_led_color(uint8_t r, uint8_t g, uint8_t b);
static void wifi_task(void *arg);
static void wifi_event_cb(wifi_manager_event_t event, const char *data, void *user_data);
static void config_button_event_cb(wt_bsp_button_t button,
                                   wt_bsp_button_event_t event,
                                   void *user_data);

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
    s_led_red = r;
    s_led_green = g;
    s_led_blue = b;

    wt_bsp_rgb_t rgb = wt_bsp_get_rgb();
    if (rgb) {
        wt_bsp_rgb_set_pixel(rgb, 0, (wt_bsp_rgb_color_t){r, g, b});
        wt_bsp_rgb_refresh(rgb);
    }
}

static void set_status_led_color(uint8_t r, uint8_t g, uint8_t b)
{
    example_set_led_color(r, g, b);

    if (s_ui_ready && wt_bsp_dsi_lvgl_lock(pdMS_TO_TICKS(100))) {
        update_led_color(r, g, b);
        wt_bsp_dsi_lvgl_unlock();
    }
}

esp_err_t example_sdcard_mount(void)
{
    wt_bsp_sdmmc_t sdmmc = wt_bsp_get_sdmmc();
    if (sdmmc == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    return wt_bsp_sdmmc_mount(sdmmc);
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

static esp_err_t initialize_wifi(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    const wifi_manager_config_t config = {
        .ssid_prefix = "WT9932P4C61",
        .language = "zh-CN",
        .station_scan_min_interval_seconds = 10,
        .station_scan_max_interval_seconds = 300,
        .station_failure_retry_count = 3,
    };
    if (!wifi_manager_initialize(&config)) {
        return ESP_FAIL;
    }

    if (xTaskCreate(wifi_task, "factory_wifi", 4096, NULL, 5,
                    &s_wifi_task_handle) != pdPASS) {
        s_wifi_task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    wifi_manager_set_event_callback(wifi_event_cb, NULL);

    wt_bsp_button_t button = wt_bsp_get_button();
    if (button == NULL) {
        ESP_LOGW(TAG, "Button not available; runtime configuration trigger disabled");
    } else {
        ret = wt_bsp_button_register_event_cb(button, config_button_event_cb, NULL);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to register button callback: %s", esp_err_to_name(ret));
        }
    }

    if (wifi_manager_has_saved_credentials()) {
        wifi_manager_start_station();
    } else {
        wifi_manager_start_config_ap();
    }

    return ESP_OK;
}

static void wifi_task(void *arg)
{
    bool sntp_initialized = false;
    bool restart_after_connect = false;

    (void)arg;
    while (true) {
        uint32_t notification = 0;
        xTaskNotifyWait(0, UINT32_MAX, &notification, pdMS_TO_TICKS(1000));

        if ((notification & WIFI_TASK_CONFIG_BIT) != 0 && !wifi_manager_is_config_mode()) {
            ESP_LOGI(TAG, "Button long press detected, entering configuration mode");
            wifi_manager_start_config_ap();
        }

        if ((notification & WIFI_TASK_CONNECTED_BIT) != 0 && !sntp_initialized) {
            const esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            esp_err_t ret = esp_netif_sntp_init(&sntp_config);
            if (ret == ESP_OK) {
                sntp_initialized = true;
            } else {
                ESP_LOGW(TAG, "SNTP initialization failed: %s", esp_err_to_name(ret));
            }
        }

        if ((notification & WIFI_TASK_STATION_BIT) != 0) {
            restart_after_connect = true;
            wifi_manager_start_station();
        }

        const uint32_t status_bits = WIFI_TASK_CONNECTED_BIT |
                                     WIFI_TASK_STATION_BIT |
                                     WIFI_TASK_CONNECTING_BIT |
                                     WIFI_TASK_DISCONNECTED_BIT |
                                     WIFI_TASK_CONFIG_MODE_BIT;
        if ((notification & status_bits) != 0) {
            if (wifi_manager_is_config_mode()) {
                set_status_led_color(255, 0, 255);
            } else if (wifi_manager_is_connected()) {
                set_status_led_color(0, 255, 0);
            } else if ((notification & WIFI_TASK_DISCONNECTED_BIT) != 0) {
                set_status_led_color(255, 0, 0);
            } else {
                set_status_led_color(0, 0, 255);
            }
        }

        if ((notification & WIFI_TASK_CONNECTED_BIT) != 0 && restart_after_connect) {
            ESP_LOGI(TAG, "Provisioned Wi-Fi connected, restarting device");
            if (s_ui_ready && wt_bsp_dsi_lvgl_lock(pdMS_TO_TICKS(100))) {
                lv_obj_t *black_overlay = lv_obj_create(lv_layer_top());
                lv_obj_set_size(black_overlay, lv_pct(100), lv_pct(100));
                lv_obj_set_pos(black_overlay, 0, 0);
                lv_obj_set_style_bg_color(black_overlay, lv_color_hex(0x000000), 0);
                lv_obj_set_style_bg_opa(black_overlay, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(black_overlay, 0, 0);
                lv_obj_set_style_radius(black_overlay, 0, 0);
                lv_obj_clear_flag(black_overlay, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_invalidate(black_overlay);
                lv_refr_now(NULL);
                wt_bsp_dsi_lvgl_unlock();
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        }

        if (s_ui_ready && wt_bsp_dsi_lvgl_lock(pdMS_TO_TICKS(100))) {
            const bool connected = wifi_manager_is_connected();
            char ap_name[WIFI_TEXT_BUFFER_SIZE] = "-";
            char time_text[WIFI_TEXT_BUFFER_SIZE] = "1970-01-01 00:00:00";

            if (connected) {
                time_t now = 0;
                struct tm time_info = {0};

                time(&now);
                localtime_r(&now, &time_info);
                strftime(time_text, sizeof(time_text), "%Y-%m-%d %H:%M:%S", &time_info);
            } else if (wifi_manager_is_config_mode()) {
                wifi_manager_get_ap_ssid(ap_name, sizeof(ap_name));
            }

            update_wifi_status(connected, ap_name, time_text);
            update_led_color(s_led_red, s_led_green, s_led_blue);
            wt_bsp_dsi_lvgl_unlock();
        }
    }
}

static void wifi_event_cb(wifi_manager_event_t event, const char *data, void *user_data)
{
    uint32_t notification = 0;

    (void)data;
    (void)user_data;

    switch (event) {
    case WIFI_MANAGER_EVENT_SCANNING:
    case WIFI_MANAGER_EVENT_CONNECTING:
        notification = WIFI_TASK_CONNECTING_BIT;
        break;
    case WIFI_MANAGER_EVENT_CONNECTED:
        notification = WIFI_TASK_CONNECTED_BIT;
        break;
    case WIFI_MANAGER_EVENT_DISCONNECTED:
        notification = WIFI_TASK_DISCONNECTED_BIT;
        break;
    case WIFI_MANAGER_EVENT_CONFIG_MODE_ENTER:
        notification = WIFI_TASK_CONFIG_MODE_BIT;
        break;
    case WIFI_MANAGER_EVENT_CONFIG_MODE_EXIT:
        notification = WIFI_TASK_STATION_BIT;
        break;
    }

    if (notification != 0 && s_wifi_task_handle != NULL) {
        xTaskNotify(s_wifi_task_handle, notification, eSetBits);
    }
}

static void config_button_event_cb(wt_bsp_button_t button,
                                   wt_bsp_button_event_t event,
                                   void *user_data)
{
    (void)button;
    (void)user_data;

    if (event == WT_BSP_BUTTON_EVENT_RELEASE) {
        s_config_button_triggered = false;
    } else if (event == WT_BSP_BUTTON_EVENT_KEEPALIVE &&
               !s_config_button_triggered &&
               s_wifi_task_handle != NULL) {
        s_config_button_triggered = true;
        xTaskNotify(s_wifi_task_handle, WIFI_TASK_CONFIG_BIT, eSetBits);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing Wireless-Tag BSP");
    esp_err_t ret = wt_bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BSP init failed: %s", esp_err_to_name(ret));
        //return;
    }

    wt_bsp_dsi_t dsi = wt_bsp_get_dsi();
    wt_bsp_csi_t csi = wt_bsp_get_csi();
    wt_bsp_touch_t touch = wt_bsp_get_touch();
    wt_bsp_sdmmc_t sdmmc = wt_bsp_get_sdmmc();

    /* Hardware status detection */
    bool dsi_ok = (dsi != NULL);
    bool sdmmc_ok = false;
    if (sdmmc != NULL) {
        ret = wt_bsp_sdmmc_mount(sdmmc);
        if (ret == ESP_OK) {
            sdmmc_ok = true;
        } else {
            ESP_LOGW(TAG, "SDMMC mount failed: %s", esp_err_to_name(ret));
        }
    }
    bool csi_initialized = (csi != NULL);

    /* CSI status will be determined later when we actually try to start it */
    /* For LED indication, we assume CSI is OK if it initialized, will update if start fails */

    ESP_LOGI(TAG, "Hardware status: DSI=%d, CSI_init=%d, SDMMC=%d", dsi_ok, csi_initialized, sdmmc_ok);

    /* Set LED color based on initial hardware status (CSI will be verified later) */
    wt_bsp_rgb_t rgb = wt_bsp_get_rgb();
    if (rgb) {
        if (!dsi_ok && !csi_initialized && !sdmmc_ok) {
            /* Screen, camera, and SD card all not connected: RED */
            ESP_LOGW(TAG, "No peripherals detected - LED RED");
            example_set_led_color(255, 0, 0);
        } else if (!csi_initialized && !sdmmc_ok) {
            /* Camera and SD card not connected (based on init): ORANGE */
            ESP_LOGW(TAG, "Camera and SD card not detected - LED ORANGE");
            example_set_led_color(255, 165, 0);
        } else if (!dsi_ok) {
            /* Screen not connected (with any other state): PINK */
            ESP_LOGW(TAG, "Screen not detected - LED PINK");
            example_set_led_color(255, 105, 180);
        } else if (!csi_initialized) {
            /* Only camera not connected (based on init): BLUE */
            ESP_LOGW(TAG, "Camera not detected (init failed) - LED BLUE");
            example_set_led_color(0, 0, 255);
        } else if (!sdmmc_ok) {
            /* Only SD card not connected: YELLOW */
            ESP_LOGW(TAG, "SD card not detected - LED YELLOW");
            example_set_led_color(255, 255, 0);
        } else {
            /* All hardware initialized: GREEN */
            ESP_LOGI(TAG, "All peripherals initialized - LED GREEN");
            example_set_led_color(0, 255, 0);
        }
    }

    /* Check if display is available before proceeding with UI */
    if (!dsi_ok) {
        ESP_LOGW(TAG, "Display not available, skipping UI initialization");
        ESP_LOGI(TAG, "Factory firmware example running (no display mode)");
        wt_bsp_deinit();
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
        lvgl_ui(disp);
        update_led_color(s_led_red, s_led_green, s_led_blue);
        s_ui_ready = true;
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
        wt_bsp_csi_set_pixel_format(csi, V4L2_PIX_FMT_RGB565);

        ret = wt_bsp_csi_start(csi, camera_frame_cb, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start camera: %s", esp_err_to_name(ret));
            if (wt_bsp_dsi_lvgl_lock(portMAX_DELAY)) {
                set_camera_error("Camera Init Failed");
                wt_bsp_dsi_lvgl_unlock();
            }
        }
    }

    /* 6. Initialize Wi-Fi after CSI has claimed its hardware resources. */
    ret = initialize_wifi();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi initialization failed: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Factory firmware example running");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100000));
    }
}
