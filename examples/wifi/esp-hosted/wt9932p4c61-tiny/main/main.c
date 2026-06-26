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

