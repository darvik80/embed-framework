idf_component_register(
    SRCS "test_app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES unity embed
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -frtti)
