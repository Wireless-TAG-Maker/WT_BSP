/**
 * @file main.c
 * @author Wireless-Tag
 * @brief ESP32-P4 ESP-Hosted master example.
 * @version 0.1
 * @date 2026-06-29
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#include "wifi_manager_bridge.h"
#include "wt_bsp.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define RGB_COLOR_CONNECT  ((wt_bsp_rgb_color_t) {.r = 0, .g = 0, .b = 255})
#define RGB_COLOR_SUCCESS  ((wt_bsp_rgb_color_t) {.r = 0, .g = 255, .b = 0})
#define RGB_COLOR_FAIL     ((wt_bsp_rgb_color_t) {.r = 255, .g = 0, .b = 0})
#define RGB_COLOR_CONFIG   ((wt_bsp_rgb_color_t) {.r = 255, .g = 0, .b = 255})

#define WIFI_TEXT_BUFFER_SIZE  64

static const char *TAG = "esp_hosted_master_p4";
static wt_bsp_rgb_t s_rgb_led = NULL;
static TaskHandle_t s_config_task_handle = NULL;
static bool s_config_button_triggered = false;

static void config_button_event_cb(wt_bsp_button_t button,
                                   wt_bsp_button_event_t event,
                                   void *user_data);
static void config_task(void *arg);
static void wifi_event_cb(wifi_manager_event_t event, const char *data, void *user_data);

void app_main(void)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "ESP32-P4 ESP-Hosted Master Example");

    ret = wt_bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "BSP initialization failed: %s", esp_err_to_name(ret));
    }

    s_rgb_led = wt_bsp_get_rgb();
    if (s_rgb_led == NULL) {
        ESP_LOGW(TAG, "RGB LED not available on this board");
    }

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    const wifi_manager_config_t config = {
        .ssid_prefix = "WT9932P4C61",
        .language = "zh-CN",
        .station_scan_min_interval_seconds = 10,
        .station_scan_max_interval_seconds = 300,
        .station_failure_retry_count = 3,
        .show_ota_config = false,
        .show_sleep_config = false,
    };
    if (!wifi_manager_initialize(&config)) {
        ESP_LOGE(TAG, "WifiManager initialization failed");
        if (s_rgb_led != NULL) {
            wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_FAIL);
        }
        return;
    }

    wifi_manager_set_event_callback(wifi_event_cb, NULL);

    wt_bsp_button_t config_button = wt_bsp_get_button();
    if (config_button == NULL) {
        ESP_LOGW(TAG, "Button not available; runtime configuration trigger disabled");
    } else if (xTaskCreate(config_task, "wifi_config", 4096, NULL, 5,
                           &s_config_task_handle) != pdPASS) {
        s_config_task_handle = NULL;
        ESP_LOGE(TAG, "Failed to create Wi-Fi configuration task");
    } else {
        ret = wt_bsp_button_register_event_cb(config_button, config_button_event_cb, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register button callback: %s", esp_err_to_name(ret));
            vTaskDelete(s_config_task_handle);
            s_config_task_handle = NULL;
        } else {
            ESP_LOGI(TAG, "Long press the onboard button to enter configuration mode");
        }
    }

    if (wifi_manager_has_saved_credentials()) {
        ESP_LOGI(TAG, "Found %u saved SSID(s), starting station mode",
                 (unsigned int)wifi_manager_get_saved_credentials_count());
        wifi_manager_start_station();
    } else {
        ESP_LOGI(TAG, "No saved credentials, starting configuration AP mode");
        wifi_manager_start_config_ap();
    }

    while (true) {
        char name[WIFI_TEXT_BUFFER_SIZE] = {0};
        char ip_address[WIFI_TEXT_BUFFER_SIZE] = {0};

        vTaskDelay(pdMS_TO_TICKS(10000));
        if (wifi_manager_is_connected()) {
            wifi_manager_get_ssid(name, sizeof(name));
            wifi_manager_get_ip_address(ip_address, sizeof(ip_address));
            ESP_LOGI(TAG, "Wi-Fi connected to %s | IP: %s | RSSI: %d dBm",
                     name, ip_address, wifi_manager_get_rssi());
        } else if (wifi_manager_is_config_mode()) {
            wifi_manager_get_ap_ssid(name, sizeof(name));
            ESP_LOGI(TAG, "Wi-Fi configuration mode (AP: %s)", name);
        } else {
            ESP_LOGI(TAG, "Wi-Fi connecting");
        }
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
               s_config_task_handle != NULL) {
        s_config_button_triggered = true;
        xTaskNotifyGive(s_config_task_handle);
    }
}

static void config_task(void *arg)
{
    (void)arg;

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!wifi_manager_is_config_mode()) {
            ESP_LOGI(TAG, "Button long press detected, entering configuration mode");
            wifi_manager_start_config_ap();
        }
    }
}

static void wifi_event_cb(wifi_manager_event_t event, const char *data, void *user_data)
{
    char text[WIFI_TEXT_BUFFER_SIZE] = {0};

    (void)user_data;
    switch (event) {
    case WIFI_MANAGER_EVENT_SCANNING:
        ESP_LOGI(TAG, "Scanning for networks");
        if (s_rgb_led != NULL) {
            wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_CONNECT);
        }
        break;
    case WIFI_MANAGER_EVENT_CONNECTING:
        ESP_LOGI(TAG, "Connecting to %s", data);
        if (s_rgb_led != NULL) {
            wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_CONNECT);
        }
        break;
    case WIFI_MANAGER_EVENT_CONNECTED:
        wifi_manager_get_ip_address(text, sizeof(text));
        ESP_LOGI(TAG, "Connected to %s | IP: %s | RSSI: %d dBm | Channel: %d",
                 data, text, wifi_manager_get_rssi(), wifi_manager_get_channel());
        if (s_rgb_led != NULL) {
            wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_SUCCESS);
        }
        break;
    case WIFI_MANAGER_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from AP, reason: %s", data);
        if (s_rgb_led != NULL) {
            wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_FAIL);
        }
        break;
    case WIFI_MANAGER_EVENT_CONFIG_MODE_ENTER:
        wifi_manager_get_ap_ssid(text, sizeof(text));
        ESP_LOGI(TAG, "Configuration mode started | AP: %s | URL: http://192.168.4.1", text);
        if (s_rgb_led != NULL) {
            wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_CONFIG);
        }
        break;
    case WIFI_MANAGER_EVENT_CONFIG_MODE_EXIT:
        ESP_LOGI(TAG, "Configuration mode exited, restarting station");
        if (s_rgb_led != NULL) {
            wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_CONNECT);
        }
        break;
    }
}
