list(APPEND WT_BSP_FEATURE_INCLUDE_DIRS
     "${CMAKE_CURRENT_LIST_DIR}/include"
     "${CMAKE_CURRENT_LIST_DIR}/interface")

# Requirements discovery uses CMake script mode, where CONFIGURE_DEPENDS is
# unavailable. Normal configuration watches new sensor files automatically.
set(wt_bsp_camera_glob_options)
if(NOT CMAKE_SCRIPT_MODE_FILE)
    list(APPEND wt_bsp_camera_glob_options CONFIGURE_DEPENDS)
endif()
file(GLOB_RECURSE wt_bsp_camera_sensor_sources ${wt_bsp_camera_glob_options}
     "${CMAKE_CURRENT_LIST_DIR}/sensors/*.c")
file(GLOB_RECURSE wt_bsp_camera_sensor_headers ${wt_bsp_camera_glob_options}
     "${CMAKE_CURRENT_LIST_DIR}/sensors/*.h")
foreach(wt_bsp_camera_sensor_header IN LISTS wt_bsp_camera_sensor_headers)
    get_filename_component(wt_bsp_camera_sensor_include_dir "${wt_bsp_camera_sensor_header}" DIRECTORY)
    list(APPEND WT_BSP_FEATURE_INCLUDE_DIRS "${wt_bsp_camera_sensor_include_dir}")
endforeach()

# ESP-IDF discovers component requirements before loading Kconfig.
if(IDF_TARGET STREQUAL "esp32s31")
    list(APPEND WT_BSP_FEATURE_PRIV_REQUIRES esp_driver_cam esp_driver_ppa esp_mm esp_pm)
endif()

if("camera" IN_LIST WT_BSP_BOARD_FEATURES AND CONFIG_WT_BSP_ENABLE_CAMERA)
    if(NOT CONFIG_WT_BSP_CAMERA_SENSOR_GC2145_VGA AND NOT CONFIG_WT_BSP_CAMERA_SENSOR_GC2145_720P)
        list(FILTER wt_bsp_camera_sensor_sources EXCLUDE REGEX
             "/(gc2145_sensor|wt_bsp_gc2145_capture|wt_bsp_camera_gc2145_native)[.]c$")
    endif()
    list(APPEND WT_BSP_FEATURE_SRCS
         "${CMAKE_CURRENT_LIST_DIR}/wt_bsp_camera.c"
         "${CMAKE_CURRENT_LIST_DIR}/interface/wt_bsp_camera_interface.c"
         "${CMAKE_CURRENT_LIST_DIR}/backends/wt_bsp_camera_dvp.c"
         ${wt_bsp_camera_sensor_sources})
    list(APPEND WT_BSP_FEATURE_LDFRAGMENTS "${CMAKE_CURRENT_LIST_DIR}/interface/linker.lf")
    # Extract only registered sensor objects. Pulling the entire BSP archive
    # would also extract descriptor sources exported by USB dependencies.
    foreach(wt_bsp_camera_sensor_source IN LISTS wt_bsp_camera_sensor_sources)
        file(READ "${wt_bsp_camera_sensor_source}" wt_bsp_camera_sensor_content)
        string(REGEX MATCHALL
               "(^|\n)[ \t]*WT_BSP_CAMERA_DRIVER_REGISTER[ \t\r\n]*\\([ \t\r\n]*[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*,"
               wt_bsp_camera_registrations "${wt_bsp_camera_sensor_content}")
        foreach(wt_bsp_camera_registration IN LISTS wt_bsp_camera_registrations)
            string(REGEX MATCH "\\([ \t\r\n]*([A-Za-z_][A-Za-z0-9_]*)[ \t\r\n]*,$"
                   wt_bsp_camera_registration_id "${wt_bsp_camera_registration}")
            # Disabled sensor sources have no anchor. GNU ld permits an unused
            # -u symbol to remain undefined; enabled anchors extract their object.
            list(APPEND WT_BSP_FEATURE_LINK_OPTIONS "-u wt_bsp_camera_driver_${CMAKE_MATCH_1}")
        endforeach()
    endforeach()
    list(APPEND WT_BSP_FEATURE_PRIV_REQUIRES
         esp_video esp_cam_sensor esp_driver_jpeg esp_psram freertos esp_timer)
endif()
