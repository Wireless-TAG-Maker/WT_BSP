if(CONFIG_WT_BSP_BOARD_WT9932P4X_TINY)
    if(CONFIG_ESP32P4_SELECTS_REV_LESS_V3)
        message(FATAL_ERROR "WT9932P4X-TINY requires ESP32-P4 v3.x firmware; run idf.py set-board again")
    endif()
    list(APPEND WT_BSP_BOARD_INCLUDE_DIRS
         "${CMAKE_CURRENT_LIST_DIR}"
         "${CMAKE_CURRENT_LIST_DIR}/../WT9932P4-TINY")
    list(APPEND WT_BSP_BOARD_FEATURES board button rgb sdmmc dsi csi touch usb_device_cdc)
    list(APPEND WT_BSP_BOARD_SRCS
         "${CMAKE_CURRENT_LIST_DIR}/board.c"
         "${CMAKE_CURRENT_LIST_DIR}/../WT9932P4-TINY/board_common.c")
endif()
