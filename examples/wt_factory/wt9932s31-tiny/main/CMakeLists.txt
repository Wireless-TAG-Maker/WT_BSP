set(factory_srcs "main.c" "factory_console.c")
if(CONFIG_WT_FACTORY_ENABLE_SD_RECORDING)
    list(APPEND factory_srcs "avi_writer.c")
endif()

idf_component_register(SRCS ${factory_srcs}
                       INCLUDE_DIRS "."
                       REQUIRES wt_bsp esp_timer fatfs)
