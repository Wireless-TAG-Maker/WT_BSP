file(GLOB board_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_LIST_DIR}/*")
list(SORT board_dirs)

set(WT_BSP_BOARD_SRCS)
set(WT_BSP_BOARD_INCLUDE_DIRS)
set(WT_BSP_BOARD_FEATURES)
set(WT_BSP_BOARD_PRIV_REQUIRES)

if(CONFIG_WT_BSP_ENABLE_USB_DEVICE_UVC AND CONFIG_WT_BSP_ENABLE_CSI)
    list(APPEND WT_BSP_BOARD_SRCS "${CMAKE_CURRENT_LIST_DIR}/board_usb_device_uvc.c")
    list(APPEND WT_BSP_BOARD_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}")
    list(APPEND WT_BSP_BOARD_PRIV_REQUIRES usb_device_uvc)
endif()

foreach(board_dir IN LISTS board_dirs)
    if(IS_DIRECTORY "${board_dir}" AND EXISTS "${board_dir}/CMakeLists.txt")
        include("${board_dir}/CMakeLists.txt")
    endif()
endforeach()
