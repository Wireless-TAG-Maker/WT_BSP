cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "../../../components")

include("../../../components/wt_bsp/tools/wt_bsp_project.cmake")
wt_bsp_apply_sdkconfig_defaults()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(wt_factory_esp32p4)
