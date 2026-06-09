include(${CMAKE_CURRENT_LIST_DIR}/boards/CMakeLists.txt)

file(GLOB WT_BSP_BASE_SRCS "${CMAKE_CURRENT_LIST_DIR}/src/*.c")
set(WT_BSP_LWBTN_DIR "${CMAKE_CURRENT_LIST_DIR}/third_party/lwbtn")
set(WT_BSP_LED_STRIP_DIR "${CMAKE_CURRENT_LIST_DIR}/third_party/led_strip")
list(APPEND WT_BSP_BASE_SRCS "${WT_BSP_LWBTN_DIR}/src/lwbtn/lwbtn.c")
list(APPEND WT_BSP_BASE_SRCS
    "${WT_BSP_LED_STRIP_DIR}/src/led_strip_api.c"
)
if(CONFIG_SOC_RMT_SUPPORTED)
    list(APPEND WT_BSP_BASE_SRCS
        "${WT_BSP_LED_STRIP_DIR}/src/led_strip_rmt_dev.c"
        "${WT_BSP_LED_STRIP_DIR}/src/led_strip_rmt_encoder.c"
    )
endif()
if(CONFIG_SOC_GPSPI_SUPPORTED)
    list(APPEND WT_BSP_BASE_SRCS
        "${WT_BSP_LED_STRIP_DIR}/src/led_strip_spi_dev.c"
    )
endif()

idf_component_register(SRCS
                        ${WT_BSP_BASE_SRCS}
                        ${WT_BSP_BOARD_SRCS}
	                    INCLUDE_DIRS "include"
	                                  "."
	                                  "${WT_BSP_LWBTN_DIR}/src/include"
	                                  "${WT_BSP_LWBTN_DIR}/port"
	                                  "${WT_BSP_LED_STRIP_DIR}/include"
	                                  ${WT_BSP_BOARD_INCLUDE_DIRS}
	                    PRIV_INCLUDE_DIRS "${WT_BSP_LED_STRIP_DIR}/interface"
	                                      "${WT_BSP_LED_STRIP_DIR}/src"
	                    REQUIRES esp_driver_gpio esp_timer esp_driver_rmt esp_driver_spi fatfs sdmmc esp_lcd esp_hw_support esp_video esp_cam_sensor
	                    PRIV_REQUIRES freertos esp_driver_ledc esp_lcd_ek79007 esp_lcd_ili9881c esp_lvgl_port lvgl esp_sccb_intf esp_ipa

	                    )
