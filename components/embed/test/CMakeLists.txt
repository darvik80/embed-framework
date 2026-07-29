# Unity unit tests for the embed framework component.
# Build via the ESP-IDF unit-test-app, or use test_apps/embed_unity.
idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES unity embed
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PUBLIC -frtti)
