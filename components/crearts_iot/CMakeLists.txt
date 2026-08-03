idf_component_register(
    SRCS
        "src/credentials.cpp"
        "src/topics.cpp"
        "src/telemetry.cpp"
        "src/attributes.cpp"
        "src/crearts_iot_service.cpp"
        "src/metrics_telemetry_bridge.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        embed
        embed_core
        json
        log
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions)
