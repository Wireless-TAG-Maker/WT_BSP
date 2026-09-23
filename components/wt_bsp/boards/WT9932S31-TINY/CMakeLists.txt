if(CONFIG_WT_BSP_BOARD_WT9932S31_TINY)
    list(APPEND WT_BSP_BOARD_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}")
    list(APPEND WT_BSP_BOARD_FEATURES board button rgb sdmmc usb_device_cdc camera)
    list(APPEND WT_BSP_BOARD_SRCS "${CMAKE_CURRENT_LIST_DIR}/board.c")

    if(CONFIG_WT_BSP_ENABLE_USB_DEVICE_UVC)
        list(APPEND WT_BSP_BOARD_SRCS "${CMAKE_CURRENT_LIST_DIR}/board_usb_device_uvc.c")
        if(CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145)
            list(APPEND WT_BSP_BOARD_SRCS "${CMAKE_CURRENT_LIST_DIR}/board_gc2145.c")
        endif()
        list(APPEND WT_BSP_BOARD_PRIV_REQUIRES esp_video esp_cam_sensor usb_device_uvc
             esp_driver_jpeg esp_psram)
    endif()
endif()
