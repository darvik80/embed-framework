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

# ── embed types (pair, tuple, array, optional, variant) ─────────────────

add_executable(test_embed_types
    test_embed_types.cpp
)
target_include_directories(test_embed_types PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
)
target_link_libraries(test_embed_types PRIVATE unity)
add_test(NAME embed_types COMMAND test_embed_types)

# ── embed state machine ─────────────────────────────────────────────────

add_executable(test_embed_state_machine
    test_embed_state_machine.cpp
)
target_include_directories(test_embed_state_machine PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
)
target_link_libraries(test_embed_state_machine PRIVATE unity)
add_test(NAME embed_state_machine COMMAND test_embed_state_machine)

# ── ThingsBoard topics (full coverage) ──────────────────────────────────

add_executable(test_tb_topics
    test_tb_topics.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard/src/topics.cpp"
)
target_include_directories(test_tb_topics PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/thingsboard/include"
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
)
target_link_libraries(test_tb_topics PRIVATE unity)
add_test(NAME tb_topics COMMAND test_tb_topics)

# ── Alicloud MessageIdGenerator ─────────────────────────────────────────

add_executable(test_alicloud_msgid
    test_alicloud_msgid.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../components/alicloud_iot/src/message_id_generator.cpp"
)
target_include_directories(test_alicloud_msgid PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/alicloud_iot/include"
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed_core/include"
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
)
target_link_libraries(test_alicloud_msgid PRIVATE unity)
add_test(NAME alicloud_msgid COMMAND test_alicloud_msgid)

# ── Alicloud credentials (needs mbedtls SHA-256 on host) ───────────────

FetchContent_Declare(
    mbedtls
    GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
    GIT_TAG        v3.6.2
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(mbedtls)
if(NOT mbedtls_POPULATED)
    FetchContent_Populate(mbedtls)
endif()

add_library(mbedtls_sha256_host STATIC
    "${mbedtls_SOURCE_DIR}/library/sha256.c"
    "${mbedtls_SOURCE_DIR}/library/platform_util.c"
    "${mbedtls_SOURCE_DIR}/library/constant_time.c"
)
target_include_directories(mbedtls_sha256_host PUBLIC
    "${mbedtls_SOURCE_DIR}/include"
)
target_compile_definitions(mbedtls_sha256_host PRIVATE
    MBEDTLS_CONFIG_FILE="${CMAKE_CURRENT_LIST_DIR}/mbedtls_config_host.h"
)

add_executable(test_alicloud_credentials
    test_alicloud_credentials.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../components/alicloud_iot/src/alicloud_credentials.cpp"
)
target_include_directories(test_alicloud_credentials PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/alicloud_iot/include"
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed_core/include"
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
)
target_link_libraries(test_alicloud_credentials PRIVATE unity mbedtls_sha256_host)
add_test(NAME alicloud_credentials COMMAND test_alicloud_credentials)
