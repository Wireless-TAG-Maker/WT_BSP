idf_component_register(SRCS "main.c" "wifi_manager_bridge.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_netif esp_event esp_wifi nvs_flash esp_wifi_remote wt_bsp esp_timer esp_http_server)
