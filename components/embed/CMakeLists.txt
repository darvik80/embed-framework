idf_component_register(
    SRCS
        "src/registry.cpp"
        "src/event_loop.cpp"
        "src/signal_slot.cpp"
    INCLUDE_DIRS "include"
    REQUIRES "esp_event" "esp_system" "freertos"
    PRIV_REQUIRES ""
)

# C++20
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)

# Enable RTTI for dynamic_cast
target_compile_options(${COMPONENT_LIB} PUBLIC -frtti)

# Keep exceptions disabled (embedded)
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions)

# Configuration defaults are in include/embed/config.hpp (#ifndef guards).
# To override, either:
#   1. Define EMBED_XXX before including embed.hpp, or
#   2. Set CMake cache: -DEMBED_MAX_SERVICES=32
# If a CMake variable is set, pass it as a compile definition:
foreach(cfg_var IN ITEMS
    EMBED_MAX_SERVICES
    EMBED_MAX_CONNECTIONS
    EMBED_EVENT_QUEUE_SIZE
    EMBED_EVENT_TASK_PRIORITY
    EMBED_EVENT_TASK_STACK_SIZE
    EMBED_THREAD_SAFE
    EMBED_MAX_EVENT_DATA_SIZE
    EMBED_SERVICE_SIZE
    EMBED_MAX_CUSTOM_METRICS
)
    if(DEFINED ${cfg_var})
        target_compile_definitions(${COMPONENT_LIB} PUBLIC ${cfg_var}=${${cfg_var}})
    endif()
endforeach()
