file(GLOB board_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_LIST_DIR}/*")
list(SORT board_dirs)

set(WT_BSP_BOARD_SRCS)
set(WT_BSP_BOARD_INCLUDE_DIRS)

foreach(board_dir IN LISTS board_dirs)
    if(IS_DIRECTORY "${board_dir}" AND EXISTS "${board_dir}/CMakeLists.txt")
        include("${board_dir}/CMakeLists.txt")
    endif()
endforeach()
