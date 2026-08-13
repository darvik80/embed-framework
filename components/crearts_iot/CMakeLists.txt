idf_component_register(
    SRCS
        "src/credentials.cpp"
        "src/credential_store.cpp"
        "src/topics.cpp"
        "src/telemetry.cpp"
        "src/attributes.cpp"
        "src/rpc_params.cpp"
        "src/rpc_registry.cpp"
        "src/crearts_iot_service.cpp"
        "src/metrics_telemetry_bridge.cpp"
        "src/device_info_service.cpp"
        "src/ota_service.cpp"
        "src/ntp_service.cpp"
        "src/log_service.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        embed
        embed_core
        log
        app_update
        esp_http_client
        esp_app_format
        esp_hw_support
        mbedtls
        esp-tls
        cjson
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions)
