idf_component_register(
    SRCS
        "src/credentials.cpp"
    INCLUDE_DIRS
        include
    REQUIRES
        embed
        embed_core
)
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu2x)
