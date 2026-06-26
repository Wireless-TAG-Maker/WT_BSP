include(${CMAKE_CURRENT_LIST_DIR}/boards/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/features/CMakeLists.txt)

set(WT_BSP_BASE_SRCS "${CMAKE_CURRENT_LIST_DIR}/src/wt_bsp.c")

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
                            REQUIRES esp_driver_gpio
                                     ${WT_BSP_FEATURE_REQUIRES}
                            PRIV_REQUIRES freertos
                                          ${WT_BSP_FEATURE_PRIV_REQUIRES}

                            )
