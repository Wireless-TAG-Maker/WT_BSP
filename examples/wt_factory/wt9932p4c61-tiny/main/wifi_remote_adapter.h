/*
 * WiFi Remote Adapter
 *
 * Redirect standard esp_wifi APIs used by esp-wifi-connect to ESP-Hosted.
 */

#ifndef __WIFI_REMOTE_ADAPTER_H__
#define __WIFI_REMOTE_ADAPTER_H__

#ifdef __cplusplus

#include "esp_wifi_remote.h"

#define esp_wifi_init esp_wifi_remote_init
#define esp_wifi_deinit esp_wifi_remote_deinit
#define esp_wifi_set_mode esp_wifi_remote_set_mode
#define esp_wifi_get_mode esp_wifi_remote_get_mode
#define esp_wifi_start esp_wifi_remote_start
#define esp_wifi_stop esp_wifi_remote_stop
#define esp_wifi_connect esp_wifi_remote_connect
#define esp_wifi_disconnect esp_wifi_remote_disconnect
#define esp_wifi_scan_start esp_wifi_remote_scan_start
#define esp_wifi_scan_stop esp_wifi_remote_scan_stop
#define esp_wifi_scan_get_ap_num esp_wifi_remote_scan_get_ap_num
#define esp_wifi_scan_get_ap_records esp_wifi_remote_scan_get_ap_records
#define esp_wifi_scan_get_ap_record esp_wifi_remote_scan_get_ap_record
#define esp_wifi_sta_get_ap_info esp_wifi_remote_sta_get_ap_info
#define esp_wifi_set_ps esp_wifi_remote_set_ps
#define esp_wifi_get_ps esp_wifi_remote_get_ps
#define esp_wifi_set_config esp_wifi_remote_set_config
#define esp_wifi_get_config esp_wifi_remote_get_config
#define esp_wifi_get_mac esp_wifi_remote_get_mac
#define esp_wifi_set_max_tx_power esp_wifi_remote_set_max_tx_power
#define esp_wifi_get_max_tx_power esp_wifi_remote_get_max_tx_power

#define esp_netif_create_default_wifi_sta esp_wifi_remote_create_default_sta
#define esp_netif_create_default_wifi_ap esp_wifi_remote_create_default_ap

#endif /* __cplusplus */

#endif /* __WIFI_REMOTE_ADAPTER_H__ */
