idf_component_register(SRCS "main.c" "lvgl_ui.c"
                       INCLUDE_DIRS "."
                       REQUIRES wt_bsp fatfs sdmmc led_strip esp_lcd esp_lcd_ili9881c esp_lcd_ek79007 esp_lcd_st7102 esp_lcd_touch_st7123 esp_driver_i2c esp_timer esp_hw_support esp_driver_gpio lvgl esp_driver_sdmmc esp_common bootloader_support vfs esp_video esp_cam_sensor esp_sccb_intf esp_ipa esp_driver_ppa)
