list(APPEND WT_BSP_FEATURE_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")

if("dsi" IN_LIST WT_BSP_BOARD_FEATURES AND CONFIG_WT_BSP_ENABLE_DSI)
    list(APPEND WT_BSP_FEATURE_SRCS "${CMAKE_CURRENT_LIST_DIR}/wt_bsp_dsi.c")
    if(CONFIG_WT_BSP_DSI_LCD_ADAPTER_WLK7002_MIPI_15P)
        list(APPEND WT_BSP_FEATURE_SRCS
             "${CMAKE_CURRENT_LIST_DIR}/panels/wt_bsp_dsi_jd9165ba.c")
        list(APPEND WT_BSP_FEATURE_PRIV_REQUIRES esp_hal_lcd)
    endif()
    list(APPEND WT_BSP_FEATURE_REQUIRES esp_driver_i2c esp_lcd esp_lvgl_port lvgl)
    list(APPEND WT_BSP_FEATURE_PRIV_REQUIRES
         esp_driver_ledc
         esp_lcd_ek79007
         esp_lcd_ili9881c
         esp_lcd_st7102)
endif()
