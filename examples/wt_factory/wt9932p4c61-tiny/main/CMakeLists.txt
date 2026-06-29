idf_component_register(SRCS "main.c" "lvgl_ui.c" "wifi_manager_bridge.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES wt_bsp esp_driver_ppa esp_timer esp_lcd esp_mm
                                esp_netif esp_event nvs_flash esp_wifi_remote)
