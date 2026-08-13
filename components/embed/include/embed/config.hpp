#pragma once

// Compile-time configuration for the embed framework.
// Override any of these before including embed.hpp, or via CMake:
//   target_compile_definitions(... PUBLIC EMBED_MAX_SERVICES=32)

#ifndef EMBED_MAX_SERVICES
#define EMBED_MAX_SERVICES 16
#endif

#ifndef EMBED_MAX_CONNECTIONS
#define EMBED_MAX_CONNECTIONS 64
#endif

// Max wait when posting to the embed event queue (ms).
// Prevents producers from blocking forever if the queue is full.
// Set to -1 to restore portMAX_DELAY (not recommended).
#ifndef EMBED_EVENT_POST_TIMEOUT_MS
#define EMBED_EVENT_POST_TIMEOUT_MS 100
#endif

#ifndef EMBED_THREAD_SAFE
#define EMBED_THREAD_SAFE 1
#endif

// Maximum size of a message payload in bytes.
// Messages larger than this will fail the Message concept.
// Increase if your messages (e.g. MetricsCollected, MqttMessageReceived) need more.
#ifndef EMBED_MAX_EVENT_DATA_SIZE
#define EMBED_MAX_EVENT_DATA_SIZE 1600
#endif

// Maximum size of a service object in bytes.
// Each service is stored in a fixed-size slot — increase this
// if your service classes (including their StateMachine, wifi_config_t, etc.)
// are larger than the default.
// Total memory used: EMBED_MAX_SERVICES * EMBED_SERVICE_SIZE
#ifndef EMBED_SERVICE_SIZE
#define EMBED_SERVICE_SIZE 512
#endif

// Maximum number of custom metrics in MetricsService.
// Each entry is ~36 bytes (embed::string<31> name + float value).
#ifndef EMBED_MAX_CUSTOM_METRICS
#define EMBED_MAX_CUSTOM_METRICS 4
#endif
