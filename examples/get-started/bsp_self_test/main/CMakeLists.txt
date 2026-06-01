idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES wt_bsp log esp_common esp_driver_gpio freertos)
