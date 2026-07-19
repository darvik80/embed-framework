# ESP-IDF project CMakeLists.txt
# The include() must come before project() for ESP-IDF build system integration
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(embed-framework)
