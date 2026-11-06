set(camera_uvc_srcs "camera_uvc.c")

if(CONFIG_WT_BSP_ENABLE_USB_DEVICE_UVC AND CONFIG_WT_BSP_ENABLE_CSI)
    list(APPEND camera_uvc_srcs "camera_uvc_csi.c")
endif()

# Kconfig is unavailable during IDF requirements discovery. Managed camera
# dependencies are selected in the manifest; IDF requirements stay unconditional.
idf_component_register(SRCS ${camera_uvc_srcs}
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "."
                       REQUIRES wt_bsp
                       PRIV_REQUIRES freertos heap esp_timer esp_driver_jpeg)

# The application chooses this camera's USB transport and optional CDC function.
# BSP camera acquisition and the UVC feature do not select this device profile.
if(CONFIG_IDF_TARGET_ESP32S31 AND CONFIG_WT_BSP_ENABLE_USB_DEVICE_UVC AND CONFIG_WT_BSP_ENABLE_CAMERA)
    set(camera_uvc_usb_dir "${CMAKE_CURRENT_LIST_DIR}/usb")
    idf_component_get_property(camera_uvc_tinyusb_lib espressif__tinyusb COMPONENT_LIB)
    target_include_directories(${camera_uvc_tinyusb_lib} PUBLIC "${camera_uvc_usb_dir}")
    target_compile_definitions(${camera_uvc_tinyusb_lib} PUBLIC CFG_TUSB_CONFIG_FILE="camera_uvc_tusb_config.h")

    if(CONFIG_WT_BSP_USB_DEVICE_COMPOSITE)
        foreach(camera_uvc_source_property SOURCES INTERFACE_SOURCES)
            get_target_property(camera_uvc_tinyusb_sources ${camera_uvc_tinyusb_lib} ${camera_uvc_source_property})
            if(camera_uvc_tinyusb_sources)
                list(FILTER camera_uvc_tinyusb_sources EXCLUDE REGEX "(^|/)tusb/usb_descriptors\\.c$")
                set_property(TARGET ${camera_uvc_tinyusb_lib} PROPERTY ${camera_uvc_source_property} "${camera_uvc_tinyusb_sources}")
            endif()
        endforeach()
        target_sources(${camera_uvc_tinyusb_lib} PRIVATE "${camera_uvc_usb_dir}/camera_uvc_usb_descriptors.c")
    endif()

    if(CONFIG_WT_BSP_S31_CAMERA_SENSOR_GC2145 OR CONFIG_WT_BSP_USB_DEVICE_COMPOSITE)
        get_target_property(camera_uvc_tinyusb_sources ${camera_uvc_tinyusb_lib} SOURCES)
        set(camera_uvc_original_video_sources ${camera_uvc_tinyusb_sources})
        list(FILTER camera_uvc_original_video_sources INCLUDE REGEX "(^|/)class/video/video_device\\.c$")
        list(LENGTH camera_uvc_original_video_sources camera_uvc_video_count)
        if(NOT camera_uvc_video_count EQUAL 1)
            message(FATAL_ERROR "Expected one TinyUSB video driver for the application bulk completion adapter")
        endif()
        list(REMOVE_ITEM camera_uvc_tinyusb_sources ${camera_uvc_original_video_sources})
        list(APPEND camera_uvc_tinyusb_sources "${camera_uvc_usb_dir}/camera_uvc_tinyusb_video.c")
        set_property(TARGET ${camera_uvc_tinyusb_lib} PROPERTY SOURCES "${camera_uvc_tinyusb_sources}")
    endif()
endif()
