idf_component_register(SRCS "main.c" "lvgl_ui.c"
                       INCLUDE_DIRS "."
                       REQUIRES wt_bsp esp_driver_ppa esp_timer esp_lcd esp_mm)
