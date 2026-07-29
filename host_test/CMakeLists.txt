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
FetchContent_MakeAvailable(unity cjson)

add_executable(test_embed_primitives
    test_embed_primitives.cpp
)
target_include_directories(test_embed_primitives PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
    "${unity_SOURCE_DIR}/src"
)
target_link_libraries(test_embed_primitives PRIVATE unity)

add_executable(test_tb_telemetry
    test_tb_telemetry.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard/src/telemetry.cpp"
)
target_include_directories(test_tb_telemetry PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard/include"
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
    "${unity_SOURCE_DIR}/src"
    "${cjson_SOURCE_DIR}"
)
target_link_libraries(test_tb_telemetry PRIVATE unity cjson)

enable_testing()
add_test(NAME embed_primitives COMMAND test_embed_primitives)
add_test(NAME tb_telemetry COMMAND test_tb_telemetry)
