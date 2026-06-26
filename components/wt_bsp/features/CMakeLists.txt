file(GLOB feature_dirs LIST_DIRECTORIES true "${CMAKE_CURRENT_LIST_DIR}/*")
list(SORT feature_dirs)

set(WT_BSP_FEATURE_SRCS)
set(WT_BSP_FEATURE_INCLUDE_DIRS)
set(WT_BSP_FEATURE_REQUIRES)
set(WT_BSP_FEATURE_PRIV_REQUIRES)

foreach(feature_dir IN LISTS feature_dirs)
    if(IS_DIRECTORY "${feature_dir}" AND EXISTS "${feature_dir}/CMakeLists.txt")
        include("${feature_dir}/CMakeLists.txt")
    endif()
endforeach()
