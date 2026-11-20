idf_component_register(SRCS "main.c" "lvgl_ui.c" "../../common/factory_compact_controls.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "../../common"
                       REQUIRES wt_bsp wt_camera_uvc esp_driver_ppa esp_timer esp_lcd esp_mm)
