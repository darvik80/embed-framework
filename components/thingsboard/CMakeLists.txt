idf_component_register(
    SRCS
        "src/credentials.cpp"
        "src/topics.cpp"
        "src/telemetry.cpp"
        "src/thingsboard_service.cpp"
        "src/metrics_telemetry_bridge.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        embed
        embed_core
        json
        log
        esp_hw_support
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions)
