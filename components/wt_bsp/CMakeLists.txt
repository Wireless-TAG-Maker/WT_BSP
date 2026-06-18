include(${CMAKE_CURRENT_LIST_DIR}/boards/CMakeLists.txt)

file(GLOB WT_BSP_BASE_SRCS "${CMAKE_CURRENT_LIST_DIR}/src/*.c")

idf_component_register(SRCS
                        ${WT_BSP_BASE_SRCS}
                        ${WT_BSP_BOARD_SRCS}
                            INCLUDE_DIRS "include"
                                          "."
                                          ${WT_BSP_BOARD_INCLUDE_DIRS}
                            REQUIRES button led_strip esp_driver_gpio esp_timer esp_driver_rmt esp_driver_spi fatfs sdmmc esp_lcd esp_hw_support esp_video esp_cam_sensor esp_lcd_touch
                            PRIV_REQUIRES freertos esp_driver_ledc esp_lcd_ek79007 esp_lcd_ili9881c esp_lcd_st7102 esp_lvgl_port lvgl esp_sccb_intf esp_ipa esp_lcd_touch_st7123

                            )
