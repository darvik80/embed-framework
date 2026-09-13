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
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_UTILS OFF CACHE BOOL "" FORCE)
set(ENABLE_CUSTOM_COMPILER_FLAGS OFF CACHE BOOL "" FORCE)
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

add_executable(test_tb_attributes
    test_tb_attributes.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard/src/attributes.cpp"
)
target_include_directories(test_tb_attributes PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard/include"
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
    "${unity_SOURCE_DIR}/src"
    "${cjson_SOURCE_DIR}"
)
target_link_libraries(test_tb_attributes PRIVATE unity cjson)

add_executable(test_cogitor_attributes
    test_cogitor_attributes.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../components/cogitor_iot/src/attributes.cpp"
)
target_include_directories(test_cogitor_attributes PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/cogitor_iot/include"
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
    "${unity_SOURCE_DIR}/src"
    "${cjson_SOURCE_DIR}"
)
target_link_libraries(test_cogitor_attributes PRIVATE unity cjson)

add_executable(test_cogitor_rpc
    test_cogitor_rpc.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../components/cogitor_iot/src/rpc_params.cpp"
)
target_include_directories(test_cogitor_rpc PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/cogitor_iot/include"
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
    "${unity_SOURCE_DIR}/src"
    "${cjson_SOURCE_DIR}"
)
target_link_libraries(test_cogitor_rpc PRIVATE unity cjson)

enable_testing()
add_test(NAME embed_primitives COMMAND test_embed_primitives)
add_test(NAME tb_telemetry COMMAND test_tb_telemetry)
add_test(NAME tb_attributes COMMAND test_tb_attributes)
add_test(NAME cogitor_attributes COMMAND test_cogitor_attributes)
add_test(NAME cogitor_rpc COMMAND test_cogitor_rpc)
