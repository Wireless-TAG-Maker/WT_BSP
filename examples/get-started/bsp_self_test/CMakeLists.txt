# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED IDF_TARGET)
    set(IDF_TARGET esp32p4)
endif()
set(EXTRA_COMPONENT_DIRS "../../../components/wt_bsp")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bsp_self_test)
