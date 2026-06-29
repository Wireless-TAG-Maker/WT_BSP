# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Add WT_BSP components to the build
# The WT_BSP components are located at ../../../../components
set(EXTRA_COMPONENT_DIRS "../../../../components")

include("../../../../components/wt_bsp/tools/wt_bsp_project.cmake")
wt_bsp_apply_sdkconfig_defaults()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_hosted_master_p4)
