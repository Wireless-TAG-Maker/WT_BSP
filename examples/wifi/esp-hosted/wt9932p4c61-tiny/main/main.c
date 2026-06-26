#if 0
/*
 * ESP32-P4 ESP-Hosted Master 示例
 *
 * SDIO 引脚配置（用于与 ESP32-C61 通信）:
 *   ESP32-P4          ESP32-C61
 *   --------         ----------
 *   GPIO 18    -->   CLK
 *   GPIO 19    -->   CMD
 *   GPIO 14    -->   D0
 *   GPIO 15    -->   D1
 *   GPIO 16    -->   D2
 *   GPIO 17    -->   D3
 *   GPIO 13    -->   EN (复位)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_wifi_remote.h"
#include "wt_bsp.h"

/* ==================== 配置 ==================== */

#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS 事件组 */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* ==================== 全局变量 ==================== */

static const char *TAG = "esp_hosted_master_p4";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

/* BSP 句柄 */
static wt_bsp_rgb_t s_rgb_led = NULL;

/* RGB LED 颜色定义 */
#define RGB_COLOR_IDLE     ((wt_bsp_rgb_color_t){.r = 0, .g = 0, .b = 0})       /* 熄灭 */
#define RGB_COLOR_CONNECT  ((wt_bsp_rgb_color_t){.r = 0, .g = 0, .b = 255})     /* 蓝色：连接中 */
#define RGB_COLOR_SUCCESS  ((wt_bsp_rgb_color_t){.r = 0, .g = 255, .b = 0})     /* 绿色：成功 */
#define RGB_COLOR_FAIL     ((wt_bsp_rgb_color_t){.r = 255, .g = 0, .b = 0})     /* 红色：失败 */

/* ==================== 事件处理器 ==================== */

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_REMOTE_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi Station started, connecting to AP");
        /* 设置 RGB LED 为蓝色，表示正在连接 */
        if (s_rgb_led) {
            wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_CONNECT);
        }
        esp_wifi_remote_connect();
    }
    else if (event_base == WIFI_REMOTE_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_remote_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry connecting to AP (attempt %d/%d)",
                     s_retry_num, EXAMPLE_ESP_MAXIMUM_RETRY);
            /* 设置 RGB LED 为蓝色，表示正在重连 */
            if (s_rgb_led) {
                wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_CONNECT);
            }
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Failed to connect to AP");
            /* 设置 RGB LED 为红色，表示连接失败 */
            if (s_rgb_led) {
                wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_FAIL);
            }
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        /* 设置 RGB LED 为绿色，表示连接成功 */
        if (s_rgb_led) {
            wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_SUCCESS);
        }
    }
}

/* ==================== 主要功能 ==================== */

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " ESP32-P4 ESP-Hosted Master Example");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "SDIO GPIO: CLK=18, CMD=19, D0-3=14-17, RESET=13");

    /* 1. 初始化 BSP（包含 RGB LED） */
    ESP_LOGI(TAG, "Initializing BSP...");
    esp_err_t ret = wt_bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "BSP initialization failed: %s", esp_err_to_name(ret));
    }

    /* 获取 RGB LED 句柄 */
    s_rgb_led = wt_bsp_get_rgb();
    if (s_rgb_led) {
        /* 启用自动刷新 */
        wt_bsp_rgb_set_auto_refresh(s_rgb_led, true);
        ESP_LOGI(TAG, "RGB LED initialized for status indication");
    } else {
        ESP_LOGW(TAG, "RGB LED not available on this board");
    }
    

    /* 2. 初始化 NVS */
    ESP_LOGI(TAG, "Initializing NVS...");
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. 创建事件组 */
    s_wifi_event_group = xEventGroupCreate();

    /* 3. 初始化网络和事件循环 */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 4. 注册事件处理器 */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_REMOTE_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    /* 5. 初始化 Wi-Fi Remote */
    ESP_LOGI(TAG, "Initializing Wi-Fi Remote...");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_remote_init(&cfg));

    /* 6. 配置 Wi-Fi */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_remote_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_remote_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_remote_start());

    ESP_LOGI(TAG, "Wi-Fi started, connecting to '%s'", EXAMPLE_ESP_WIFI_SSID);

    /* 7. 等待连接结果 */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(30000));  // 30秒超时

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, " Successfully connected to Wi-Fi!");
        ESP_LOGI(TAG, "========================================");

        /* 保持连接，可以做其他任务 */
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(10000));
            ESP_LOGI(TAG, "Wi-Fi is connected");
        }
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "========================================");
        ESP_LOGE(TAG, " Failed to connect to Wi-Fi!");
        ESP_LOGE(TAG, "========================================");
    } else {
        ESP_LOGE(TAG, "Connection timeout (30s)");
    }

    /* 清理 */
    esp_wifi_remote_stop();
    esp_wifi_remote_deinit();

    /* 反初始化 BSP */
    if (s_rgb_led) {
        wt_bsp_deinit();
    }
}

#else

#include "esp_wifi_remote.h"
#include "esp_log.h"
#include "wt_bsp.h"

#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY 10

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "wifi_remote";
static int s_retry_num = 0;

/* BSP 句柄 */
static wt_bsp_rgb_t s_rgb_led = NULL;

/* RGB LED 颜色定义 */
#define RGB_COLOR_IDLE     ((wt_bsp_rgb_color_t){.r = 0, .g = 0, .b = 0})       /* 熄灭 */
#define RGB_COLOR_CONNECT  ((wt_bsp_rgb_color_t){.r = 0, .g = 0, .b = 255})     /* 蓝色：连接中 */
#define RGB_COLOR_SUCCESS  ((wt_bsp_rgb_color_t){.r = 0, .g = 255, .b = 0})     /* 绿色：成功 */
#define RGB_COLOR_FAIL     ((wt_bsp_rgb_color_t){.r = 255, .g = 0, .b = 0})     /* 红色：失败 */


static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
        /* 设置 RGB LED 为蓝色，表示正在连接 */
        if (s_rgb_led) {
            wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_CONNECT);
            wt_bsp_rgb_refresh(s_rgb_led);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
            /* 设置 RGB LED 为蓝色，表示正在重连 */
            if (s_rgb_led) {
                wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_CONNECT);
                wt_bsp_rgb_refresh(s_rgb_led);
            }
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            /* 设置 RGB LED 为红色，表示连接失败 */
            if (s_rgb_led) {
                wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_FAIL);
                wt_bsp_rgb_refresh(s_rgb_led);
            }
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        /* 设置 RGB LED 为绿色，表示连接成功 */
        if (s_rgb_led) {
            wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_SUCCESS);
            wt_bsp_rgb_refresh(s_rgb_led);
        }
    }
}

void wifi_init_sta(void)
{
    /* 1. 初始化 BSP（包含 RGB LED） */
    ESP_LOGI(TAG, "Initializing BSP...");
    esp_err_t ret = wt_bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "BSP initialization failed: %s", esp_err_to_name(ret));
    }

    /* 获取 RGB LED 句柄 */
    s_rgb_led = wt_bsp_get_rgb();
    if (s_rgb_led) {
        ESP_LOGI(TAG, "RGB LED initialized for status indication");
    } else {
        ESP_LOGW(TAG, "RGB LED not available on this board");
    }

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}


void app_main(void)
{
    wifi_init_sta();
}

#endif
