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

#include <cstring>
#include <string>

#include "ssid_manager.h"
#include "wifi_manager.h"

static wifi_manager_event_cb_t s_event_callback = nullptr;
static void *s_event_user_data = nullptr;

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

static void copy_string(const std::string& source, char *buffer, size_t buffer_size)
{
    if (buffer == nullptr || buffer_size == 0) {
        return;
    }

    std::strncpy(buffer, source.c_str(), buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
}

extern "C" bool wifi_manager_initialize(const wifi_manager_config_t *config)
{
    if (config == nullptr) {
        return false;
    }

    WifiManagerConfig manager_config;
    manager_config.ssid_prefix = config->ssid_prefix != nullptr ? config->ssid_prefix : "ESP32";
    manager_config.language = config->language != nullptr ? config->language : "zh-CN";
    manager_config.station_scan_min_interval_seconds = config->station_scan_min_interval_seconds;
    manager_config.station_scan_max_interval_seconds = config->station_scan_max_interval_seconds;
    manager_config.station_failure_retry_cnt = config->station_failure_retry_count;
    manager_config.show_ota_config = config->show_ota_config;
    manager_config.show_sleep_config = config->show_sleep_config;

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

extern "C" size_t wifi_manager_get_saved_credentials_count(void)
{
    return SsidManager::GetInstance().GetSsidList().size();
}

extern "C" int wifi_manager_get_rssi(void)
{
    return WifiManager::GetInstance().GetRssi();
}

extern "C" int wifi_manager_get_channel(void)
{
    return WifiManager::GetInstance().GetChannel();
}

extern "C" void wifi_manager_get_ssid(char *buffer, size_t buffer_size)
{
    copy_string(WifiManager::GetInstance().GetSsid(), buffer, buffer_size);
}

extern "C" void wifi_manager_get_ip_address(char *buffer, size_t buffer_size)
{
    copy_string(WifiManager::GetInstance().GetIpAddress(), buffer, buffer_size);
}

extern "C" void wifi_manager_get_ap_ssid(char *buffer, size_t buffer_size)
{
    copy_string(WifiManager::GetInstance().GetApSsid(), buffer, buffer_size);
}
