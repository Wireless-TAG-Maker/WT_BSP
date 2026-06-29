set(WT_BSP_TOOLS_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(wt_bsp_apply_sdkconfig_defaults)
    set(wt_bsp_sdkconfig_defaults)
    set(wt_bsp_board_defaults "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.board")
    set(wt_bsp_board_kconfig "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.board.Kconfig")
    set(ENV{WT_BSP_BOARD_KCONFIG} "${wt_bsp_board_kconfig}")

    if(NOT DEFINED SDKCONFIG OR "${SDKCONFIG}" STREQUAL "")
        set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig" CACHE FILEPATH "ESP-IDF sdkconfig file" FORCE)
    endif()
    set(SDKCONFIG "${SDKCONFIG}" PARENT_SCOPE)

    if(DEFINED ENV{WT_BSP_BOARD} AND NOT "$ENV{WT_BSP_BOARD}" STREQUAL "")
        set(wt_bsp_python "${PYTHON}")
        if(NOT wt_bsp_python)
            find_package(Python3 COMPONENTS Interpreter REQUIRED)
            set(wt_bsp_python "${Python3_EXECUTABLE}")
        endif()

        execute_process(
            COMMAND "${wt_bsp_python}" "${WT_BSP_TOOLS_DIR}/wt_bsp_set_board.py"
                    --project-path "${CMAKE_CURRENT_LIST_DIR}"
                    --sdkconfig "${SDKCONFIG}"
                    --board "$ENV{WT_BSP_BOARD}"
                    --generate-only
                    --quiet
            RESULT_VARIABLE wt_bsp_set_board_result
            ERROR_VARIABLE wt_bsp_set_board_error
        )

        if(NOT wt_bsp_set_board_result EQUAL 0)
            message(FATAL_ERROR "Failed to select WT_BSP board '$ENV{WT_BSP_BOARD}': ${wt_bsp_set_board_error}")
        endif()
    elseif(EXISTS "${wt_bsp_board_defaults}")
        set(wt_bsp_python "${PYTHON}")
        if(NOT wt_bsp_python)
            find_package(Python3 COMPONENTS Interpreter REQUIRED)
            set(wt_bsp_python "${Python3_EXECUTABLE}")
        endif()

        execute_process(
            COMMAND "${wt_bsp_python}" "${WT_BSP_TOOLS_DIR}/wt_bsp_set_board.py"
                    --project-path "${CMAKE_CURRENT_LIST_DIR}"
                    --sdkconfig "${SDKCONFIG}"
                    --sync-target-from-board
                    --quiet
            RESULT_VARIABLE wt_bsp_sync_target_result
            ERROR_VARIABLE wt_bsp_sync_target_error
        )

        if(NOT wt_bsp_sync_target_result EQUAL 0)
            message(FATAL_ERROR "Failed to sync WT_BSP board target: ${wt_bsp_sync_target_error}")
        endif()
    endif()

    if(EXISTS "${wt_bsp_board_defaults}")
        file(STRINGS "${wt_bsp_board_defaults}" wt_bsp_idf_target_lines REGEX "^CONFIG_IDF_TARGET=\"[^\"]+\"$")
        if(wt_bsp_idf_target_lines)
            list(GET wt_bsp_idf_target_lines 0 wt_bsp_idf_target_line)
            string(REGEX REPLACE "^CONFIG_IDF_TARGET=\"([^\"]+)\".*$" "\\1" wt_bsp_idf_target "${wt_bsp_idf_target_line}")
            set(IDF_TARGET "${wt_bsp_idf_target}" CACHE STRING "ESP-IDF target selected by WT_BSP board" FORCE)
            set(IDF_TARGET "${wt_bsp_idf_target}" PARENT_SCOPE)
        endif()
    endif()

    if(DEFINED SDKCONFIG_DEFAULTS AND NOT "${SDKCONFIG_DEFAULTS}" STREQUAL "")
        list(APPEND wt_bsp_sdkconfig_defaults ${SDKCONFIG_DEFAULTS})
    elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
        list(APPEND wt_bsp_sdkconfig_defaults "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
    endif()

    if(EXISTS "${wt_bsp_board_defaults}")
        list(APPEND wt_bsp_sdkconfig_defaults "${wt_bsp_board_defaults}")
    endif()

    if(wt_bsp_sdkconfig_defaults)
        set(SDKCONFIG_DEFAULTS "${wt_bsp_sdkconfig_defaults}" PARENT_SCOPE)
    endif()
endfunction()
