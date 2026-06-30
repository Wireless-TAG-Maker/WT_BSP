/**
 * @file wifi_manager_bridge.cpp
 * @author Wireless-Tag
 * @brief esp-wifi-connect C interface implementation.
 * @version 0.1
 * @date 2026-06-29
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 */

#include "wifi_remote_adapter.h"
#include "wifi_manager_bridge.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "ssid_manager.h"
#include "wifi_manager.h"

static wifi_manager_event_cb_t s_event_callback = nullptr;
static void *s_event_user_data = nullptr;
static std::string s_ssid_prefix = "ESP32";

static wifi_manager_event_t map_event(WifiEvent event)
{
    switch (event) {
    case WifiEvent::Scanning:
        return WIFI_MANAGER_EVENT_SCANNING;
    case WifiEvent::Connecting:
        return WIFI_MANAGER_EVENT_CONNECTING;
    case WifiEvent::Connected:
        return WIFI_MANAGER_EVENT_CONNECTED;
    case WifiEvent::Disconnected:
        return WIFI_MANAGER_EVENT_DISCONNECTED;
    case WifiEvent::ConfigModeEnter:
        return WIFI_MANAGER_EVENT_CONFIG_MODE_ENTER;
    case WifiEvent::ConfigModeExit:
        return WIFI_MANAGER_EVENT_CONFIG_MODE_EXIT;
    }

    return WIFI_MANAGER_EVENT_DISCONNECTED;
}

extern "C" bool wifi_manager_initialize(const wifi_manager_config_t *config)
{
    if (config == nullptr) {
        return false;
    }

    WifiManagerConfig manager_config;
    manager_config.ssid_prefix = config->ssid_prefix != nullptr ? config->ssid_prefix : "ESP32";
    s_ssid_prefix = manager_config.ssid_prefix;
    manager_config.language = config->language != nullptr ? config->language : "zh-CN";
    manager_config.station_scan_min_interval_seconds = config->station_scan_min_interval_seconds;
    manager_config.station_scan_max_interval_seconds = config->station_scan_max_interval_seconds;
    manager_config.station_failure_retry_cnt = config->station_failure_retry_count;

    return WifiManager::GetInstance().Initialize(manager_config);
}

extern "C" void wifi_manager_set_event_callback(wifi_manager_event_cb_t callback, void *user_data)
{
    s_event_callback = callback;
    s_event_user_data = user_data;

    WifiManager::GetInstance().SetEventCallback([](WifiEvent event, const std::string& data) {
        if (s_event_callback != nullptr) {
            s_event_callback(map_event(event), data.c_str(), s_event_user_data);
        }
    });
}

extern "C" void wifi_manager_start_station(void)
{
    WifiManager::GetInstance().StartStation();
}

extern "C" void wifi_manager_start_config_ap(void)
{
    WifiManager::GetInstance().StartConfigAp();
}

extern "C" bool wifi_manager_is_connected(void)
{
    return WifiManager::GetInstance().IsConnected();
}

extern "C" bool wifi_manager_is_config_mode(void)
{
    return WifiManager::GetInstance().IsConfigMode();
}

extern "C" bool wifi_manager_has_saved_credentials(void)
{
    return !SsidManager::GetInstance().GetSsidList().empty();
}

extern "C" void wifi_manager_get_ap_ssid(char *buffer, size_t buffer_size)
{
    if (buffer == nullptr || buffer_size == 0) {
        return;
    }

    uint8_t mac[6] = {0};
    if (esp_wifi_remote_get_mac(WIFI_IF_AP, mac) == ESP_OK) {
        std::snprintf(buffer, buffer_size, "%s-%02X%02X",
                      s_ssid_prefix.c_str(), mac[4], mac[5]);
        return;
    }

    const std::string ap_ssid = WifiManager::GetInstance().GetApSsid();
    std::strncpy(buffer, ap_ssid.empty() ? "-" : ap_ssid.c_str(), buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
}
