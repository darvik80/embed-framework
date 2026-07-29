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
FetchContent_MakeAvailable(unity)

add_executable(test_embed_primitives
    test_embed_primitives.cpp
)

target_include_directories(test_embed_primitives PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../components/embed/include"
    "${unity_SOURCE_DIR}/src"
)

target_link_libraries(test_embed_primitives PRIVATE unity)

enable_testing()
add_test(NAME embed_primitives COMMAND test_embed_primitives)
