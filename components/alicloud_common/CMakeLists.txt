idf_component_register(
    SRCS
        "src/net_tools.c"
        "src/http_client.c"
    INCLUDE_DIRS
        "src"
    REQUIRES
        nvs_flash
        esp_event
        esp-tls
        mbedtls
)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu2x)
