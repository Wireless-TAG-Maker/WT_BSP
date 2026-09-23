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
list(REMOVE_DUPLICATES WT_BSP_BOARD_PRIV_REQUIRES)

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
                                          ${WT_BSP_BOARD_PRIV_REQUIRES}
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

if(CONFIG_WT_BSP_BOARD_WT9932S31_TINY AND CONFIG_WT_BSP_ENABLE_USB_DEVICE_UVC)
    # Both the UVC component and TinyUSB include the same sensor-dependent
    # endpoint configuration through TinyUSB's public configuration hook.
    set(wt_bsp_s31_dir "${CMAKE_CURRENT_LIST_DIR}/boards/WT9932S31-TINY")
    idf_component_get_property(wt_bsp_tinyusb_lib espressif__tinyusb COMPONENT_LIB)
    target_include_directories(${wt_bsp_tinyusb_lib} PUBLIC "${wt_bsp_s31_dir}")
    target_compile_definitions(${wt_bsp_tinyusb_lib} PUBLIC CFG_TUSB_CONFIG_FILE="board_tusb_config.h")

    if(CONFIG_WT_BSP_USB_DEVICE_COMPOSITE)
        # usb_device_uvc exports its descriptor source as PUBLIC; remove both
        # copies so dependents cannot compile the original UVC-only descriptor.
        foreach(wt_bsp_source_property SOURCES INTERFACE_SOURCES)
            get_target_property(wt_bsp_tinyusb_sources ${wt_bsp_tinyusb_lib} ${wt_bsp_source_property})
            if(wt_bsp_tinyusb_sources)
                list(FILTER wt_bsp_tinyusb_sources EXCLUDE REGEX "(^|/)tusb/usb_descriptors\\.c$")
                set_property(TARGET ${wt_bsp_tinyusb_lib} PROPERTY ${wt_bsp_source_property} "${wt_bsp_tinyusb_sources}")
            endif()
        endforeach()
        target_sources(${wt_bsp_tinyusb_lib} PRIVATE "${wt_bsp_s31_dir}/board_usb_descriptors.c")
    endif()

    if(CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145 OR CONFIG_WT_BSP_USB_DEVICE_COMPOSITE)
        # TinyUSB 0.19 needs a ZLP after packet-aligned, short UVC bulk payloads.
        # Substitute a board-local adapter without modifying managed sources.
        get_target_property(wt_bsp_tinyusb_sources ${wt_bsp_tinyusb_lib} SOURCES)
        set(wt_bsp_tinyusb_video_sources ${wt_bsp_tinyusb_sources})
        list(FILTER wt_bsp_tinyusb_video_sources INCLUDE REGEX "(^|/)class/video/video_device\\.c$")
        list(LENGTH wt_bsp_tinyusb_video_sources wt_bsp_tinyusb_video_count)
        if(NOT wt_bsp_tinyusb_video_count EQUAL 1)
            message(FATAL_ERROR "Expected one TinyUSB video driver for the S31 bulk completion adapter")
        endif()
        list(REMOVE_ITEM wt_bsp_tinyusb_sources ${wt_bsp_tinyusb_video_sources})
        list(APPEND wt_bsp_tinyusb_sources "${wt_bsp_s31_dir}/board_tinyusb_video.c")
        set_property(TARGET ${wt_bsp_tinyusb_lib} PROPERTY SOURCES "${wt_bsp_tinyusb_sources}")
    endif()
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
