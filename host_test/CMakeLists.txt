cmake_minimum_required(VERSION 3.16)
project(embed_host_tests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.1
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(
    cjson
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG        v1.7.18
    GIT_SHALLOW    TRUE
)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_UTILS OFF CACHE BOOL "" FORCE)
# Fetch sources only — cJSON's own CMake adds MSVC /Za which conflicts with /std:c11.
FetchContent_GetProperties(unity)
if(NOT unity_POPULATED)
    FetchContent_Populate(unity)
endif()
FetchContent_GetProperties(cjson)
if(NOT cjson_POPULATED)
    FetchContent_Populate(cjson)
endif()

add_library(unity STATIC "${unity_SOURCE_DIR}/src/unity.c")
target_include_directories(unity PUBLIC "${unity_SOURCE_DIR}/src")

add_library(cjson_host STATIC "${cjson_SOURCE_DIR}/cJSON.c")
target_include_directories(cjson_host PUBLIC "${cjson_SOURCE_DIR}")

add_executable(test_embed_primitives
    test_embed_primitives.cpp
)
target_include_directories(test_embed_primitives PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
)
target_link_libraries(test_embed_primitives PRIVATE unity)

add_executable(test_tb_telemetry
    test_tb_telemetry.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard/src/telemetry.cpp"
)
target_include_directories(test_tb_telemetry PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard/include"
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
)
target_link_libraries(test_tb_telemetry PRIVATE unity cjson_host)

add_executable(test_tb_attributes
    test_tb_attributes.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard/src/attributes.cpp"
)
target_include_directories(test_tb_attributes PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard/include"
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
)
target_link_libraries(test_tb_attributes PRIVATE unity cjson_host)

add_executable(test_tb_rpc
    test_tb_rpc.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard/src/rpc.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard/src/topics.cpp"
)
target_include_directories(test_tb_rpc PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard/include"
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
)
target_link_libraries(test_tb_rpc PRIVATE unity cjson_host)

enable_testing()
add_test(NAME embed_primitives COMMAND test_embed_primitives)
add_test(NAME tb_telemetry COMMAND test_tb_telemetry)
add_test(NAME tb_attributes COMMAND test_tb_attributes)
add_test(NAME tb_rpc COMMAND test_tb_rpc)
