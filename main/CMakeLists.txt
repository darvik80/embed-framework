idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES embed embed_core embed_extra alicloud_iot alicloud_oss thingsboard crearts_iot
             esp_driver_gpio esp_timer log freertos esp_app_format esp_system json
)

# Ensure libhal.a is linked after libmbedcrypto.a to resolve sha_hal_* symbols
# that mbedtls hardware SHA acceleration requires.
target_link_libraries(${COMPONENT_LIB} INTERFACE idf::hal)
