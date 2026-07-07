include(${CMAKE_CURRENT_LIST_DIR}/boards/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/features/CMakeLists.txt)

set(WT_BSP_BASE_SRCS "${CMAKE_CURRENT_LIST_DIR}/src/wt_bsp.c")
set(WT_BSP_IDF_REQUIRES
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_rmt
    esp_driver_spi
    esp_driver_sdmmc
    esp_lcd
    esp_hw_support
    sdmmc)
set(WT_BSP_IDF_PRIV_REQUIRES
    freertos
    esp_driver_ledc
    esp_timer
    fatfs)

list(REMOVE_DUPLICATES WT_BSP_IDF_REQUIRES)
list(REMOVE_DUPLICATES WT_BSP_IDF_PRIV_REQUIRES)
list(REMOVE_DUPLICATES WT_BSP_FEATURE_INCLUDE_DIRS)
list(REMOVE_DUPLICATES WT_BSP_FEATURE_REQUIRES)
list(REMOVE_DUPLICATES WT_BSP_FEATURE_PRIV_REQUIRES)

idf_component_register(SRCS
                        ${WT_BSP_BASE_SRCS}
                        ${WT_BSP_FEATURE_SRCS}
                        ${WT_BSP_BOARD_SRCS}
                            INCLUDE_DIRS "include"
                                          "."
                                          ${WT_BSP_FEATURE_INCLUDE_DIRS}
                                          ${WT_BSP_BOARD_INCLUDE_DIRS}
                            REQUIRES ${WT_BSP_IDF_REQUIRES}
                                     ${WT_BSP_FEATURE_REQUIRES}
                            PRIV_REQUIRES ${WT_BSP_IDF_PRIV_REQUIRES}
                                          ${WT_BSP_FEATURE_PRIV_REQUIRES}

                            )

idf_build_get_property(wt_bsp_build_components BUILD_COMPONENTS)

# Apply managed component fixes required by enabled WT_BSP features.
# Keep this internal to wt_bsp so examples only declare their own dependencies.
if(CONFIG_WT_BSP_ENABLE_CSI AND "espressif__esp_ipa" IN_LIST wt_bsp_build_components AND "esp_hal_cam" IN_LIST wt_bsp_build_components)
    idf_component_get_property(wt_bsp_esp_ipa_lib espressif__esp_ipa COMPONENT_LIB)
    idf_component_get_property(wt_bsp_esp_hal_cam_lib esp_hal_cam COMPONENT_LIB)

    target_link_libraries(${wt_bsp_esp_ipa_lib} PUBLIC ${wt_bsp_esp_hal_cam_lib})
endif()

if(CONFIG_WT_BSP_ENABLE_CSI AND "espressif__esp_video" IN_LIST wt_bsp_build_components AND "esp_driver_gpio" IN_LIST wt_bsp_build_components)
    idf_component_get_property(wt_bsp_esp_video_lib espressif__esp_video COMPONENT_LIB)
    idf_component_get_property(wt_bsp_esp_driver_gpio_lib esp_driver_gpio COMPONENT_LIB)

    target_link_libraries(${wt_bsp_esp_video_lib} PRIVATE ${wt_bsp_esp_driver_gpio_lib})
    target_compile_definitions(${wt_bsp_esp_video_lib} PRIVATE CAM_CTLR_COLOR_YUV422=CAM_CTLR_COLOR_YUV422_YUYV)
endif()

if(CONFIG_WT_BSP_ENABLE_DSI AND "kevincoooool__esp_lcd_st7102" IN_LIST wt_bsp_build_components)
    idf_component_get_property(wt_bsp_st7102_lib kevincoooool__esp_lcd_st7102 COMPONENT_LIB)

    target_compile_definitions(${wt_bsp_st7102_lib} PRIVATE color_space=rgb_ele_order)
endif()

add_custom_target(set-board
    COMMAND ${PYTHON} ${CMAKE_CURRENT_LIST_DIR}/tools/wt_bsp_set_board.py
            --project-path ${PROJECT_DIR}
            --build-dir ${CMAKE_BINARY_DIR}
            --sdkconfig ${SDKCONFIG}
            --generate-only
            --fullclean-on-target-change
    WORKING_DIRECTORY ${PROJECT_DIR}
    VERBATIM
    USES_TERMINAL
)

add_custom_target(p4_flash
    COMMAND ${PYTHON} ${CMAKE_CURRENT_LIST_DIR}/tools/wt_bsp_p4_flash.py
    WORKING_DIRECTORY ${PROJECT_DIR}
    VERBATIM
    USES_TERMINAL
)
