list(APPEND WT_BSP_FEATURE_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")

if("sdmmc" IN_LIST WT_BSP_BOARD_FEATURES AND CONFIG_WT_BSP_ENABLE_SDMMC)
    list(APPEND WT_BSP_FEATURE_SRCS "${CMAKE_CURRENT_LIST_DIR}/wt_bsp_sdmmc.c")
    list(APPEND WT_BSP_FEATURE_REQUIRES fatfs sdmmc esp_hw_support esp_driver_spi esp_driver_sdspi)
endif()
