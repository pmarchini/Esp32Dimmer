cmake_minimum_required(VERSION 3.16)

# Add the component directory to the component search path
set(EXTRA_COMPONENT_DIRS ../src/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp32_dimmer_tests)
