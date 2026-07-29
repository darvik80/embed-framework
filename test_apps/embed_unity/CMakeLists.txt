# Standalone Unity runner for embed framework primitives.
#
# From this directory (with ESP-IDF exported):
#   idf.py set-target esp32s3
#   idf.py build flash monitor
#
# On the serial console Unity prints PASS/FAIL for each TEST_CASE.

cmake_minimum_required(VERSION 3.16)

# Only build the test app + pulled-in dependencies (embed, unity).
set(COMPONENTS main)
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(embed_unity)
