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
 *
 * 本示例使用 esp-wifi-connect 组件进行 Wi-Fi 管理:
 * - 首次启动时，如果没有保存的 Wi-Fi 凭据，将进入配置 AP 模式
 * - 用户连接到 AP SSID (默认 "WT9932xxxx") 并在浏览器打开 http://192.168.4.1
 * - 配置完成后自动连接到指定的 Wi-Fi 网络
 * - Wi-Fi 凭据保存在 NVS 中，下次启动自动连接
 */

// 必须首先包含适配器，在所有其他头文件之前
#include "wifi_remote_adapter.h"

// C++ 标准库
#include <string>

// ESP-IDF C 头文件（不使用 extern "C" 包裹，让编译器自动处理）
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wt_bsp.h"

// C++ Wi-Fi Manager 头文件
#include "wifi_manager.h"
#include "ssid_manager.h"

static const char *TAG = "esp_hosted_master_p4";

/* RGB LED 颜色定义 */
#define RGB_COLOR_IDLE     ((wt_bsp_rgb_color_t){.r = 0, .g = 0, .b = 0})       /* 熄灭 */
#define RGB_COLOR_CONNECT  ((wt_bsp_rgb_color_t){.r = 0, .g = 0, .b = 255})     /* 蓝色：连接中 */
#define RGB_COLOR_SUCCESS  ((wt_bsp_rgb_color_t){.r = 0, .g = 255, .b = 0})     /* 绿色：成功 */
#define RGB_COLOR_FAIL     ((wt_bsp_rgb_color_t){.r = 255, .g = 0, .b = 0})     /* 红色：失败 */
#define RGB_COLOR_CONFIG   ((wt_bsp_rgb_color_t){.r = 255, .g = 0, .b = 255})   /* 紫色：配置模式 */

/* 全局变量 */
static wt_bsp_rgb_t s_rgb_led = NULL;

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " ESP32-P4 ESP-Hosted Master Example");
    ESP_LOGI(TAG, " with esp-wifi-connect component");
    ESP_LOGI(TAG, "========================================");

    /* 1. 初始化 BSP（包含 RGB LED） */
    ESP_LOGI(TAG, "Initializing BSP...");
    esp_err_t ret = wt_bsp_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "BSP initialization failed: %s", esp_err_to_name(ret));
    }

    /* 获取 RGB LED 句柄 */
    s_rgb_led = wt_bsp_get_rgb();
    if (s_rgb_led) {
        //wt_bsp_rgb_set_auto_refresh(s_rgb_led, true);
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

    /* 3. 初始化网络和事件循环 */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 4. 获取 WifiManager 单例并配置 */
    auto& wifi_manager = WifiManager::GetInstance();

    WifiManagerConfig config;
    config.ssid_prefix = "WT9932";      // AP mode SSID 前缀
    config.language = "zh-CN";         // Web UI 语言
    config.station_scan_min_interval_seconds = 10;   // 扫描间隔（秒）
    config.station_scan_max_interval_seconds = 300;   // 最大扫描间隔

    /* 5. 初始化 WifiManager */
    ESP_LOGI(TAG, "Initializing WifiManager...");
    if (!wifi_manager.Initialize(config)) {
        ESP_LOGE(TAG, "WifiManager initialization failed!");
        if (s_rgb_led) {
            wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_FAIL);
        }
        return;
    }

    /* 6. 设置事件回调 */
    wifi_manager.SetEventCallback([&](WifiEvent event, const std::string& data) {
        switch (event) {
            case WifiEvent::Scanning:
                ESP_LOGI(TAG, "Scanning for networks...");
                if (s_rgb_led) {
                    wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_CONNECT);
                }
                break;

            case WifiEvent::Connecting:
                ESP_LOGI(TAG, "Connecting to %s...", data.c_str());
                if (s_rgb_led) {
                    wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_CONNECT);
                }
                break;

            case WifiEvent::Connected:
                ESP_LOGI(TAG, "========================================");
                ESP_LOGI(TAG, " Connected to %s!", data.c_str());
                ESP_LOGI(TAG, " IP Address: %s", wifi_manager.GetIpAddress().c_str());
                ESP_LOGI(TAG, " RSSI: %d dBm", wifi_manager.GetRssi());
                ESP_LOGI(TAG, " Channel: %d", wifi_manager.GetChannel());
                ESP_LOGI(TAG, "========================================");
                if (s_rgb_led) {
                    wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_SUCCESS);
                }
                // 连接成功后延时 2 秒重启
                ESP_LOGI(TAG, "Connection successful, restarting in 2 seconds...");
                vTaskDelay(pdMS_TO_TICKS(2000));
                esp_restart();
                break;

            case WifiEvent::Disconnected:
                ESP_LOGW(TAG, "Disconnected from AP, reason: %s", data.c_str());
                if (s_rgb_led) {
                    wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_FAIL);
                }
                break;

            case WifiEvent::ConfigModeEnter:
                ESP_LOGI(TAG, "========================================");
                ESP_LOGI(TAG, " Entered Configuration Mode");
                ESP_LOGI(TAG, "========================================");
                ESP_LOGI(TAG, " AP SSID: %s", wifi_manager.GetApSsid().c_str());
                ESP_LOGI(TAG, " Web URL: http://192.168.4.1");
                ESP_LOGI(TAG, " Please connect to AP and open the URL in browser");
                ESP_LOGI(TAG, "========================================");
                if (s_rgb_led) {
                    wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_CONFIG);
                }
                break;

            case WifiEvent::ConfigModeExit:
                ESP_LOGI(TAG, "Exited config mode, restarting station...");
                if (s_rgb_led) {
                    wt_bsp_rgb_set_color(s_rgb_led, RGB_COLOR_CONNECT);
                }
                break;
        }
    });

    /* 7. 检查是否有保存的 Wi-Fi 凭据 */
    auto& ssid_list = SsidManager::GetInstance().GetSsidList();
    if (ssid_list.empty()) {
        /* 没有保存的凭据，启动配置 AP 模式 */
        ESP_LOGI(TAG, "No saved credentials found");
        ESP_LOGI(TAG, "Starting configuration AP mode...");
        wifi_manager.StartConfigAp();
    } else {
        /* 有保存的凭据，启动 Station 模式 */
        ESP_LOGI(TAG, "Found %d saved SSID(s), starting station mode", ssid_list.size());
        wifi_manager.StartStation();
    }

    /* 8. 主循环 - 保持运行 */
    ESP_LOGI(TAG, "Entering main loop...");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  // 每 10 秒打印一次状态

        if (wifi_manager.IsConnected()) {
            ESP_LOGI(TAG, "Wi-Fi Status: Connected to %s | IP: %s | RSSI: %d dBm",
                     wifi_manager.GetSsid().c_str(),
                     wifi_manager.GetIpAddress().c_str(),
                     wifi_manager.GetRssi());
        } else if (wifi_manager.IsConfigMode()) {
            ESP_LOGI(TAG, "Wi-Fi Status: Configuration Mode (AP: %s)",
                     wifi_manager.GetApSsid().c_str());
        } else {
            ESP_LOGI(TAG, "Wi-Fi Status: Connecting...");
        }
    }
}
