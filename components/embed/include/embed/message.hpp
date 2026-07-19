#pragma once

#include "embed/config.hpp"
#include <type_traits>

namespace embed {

/// Concept: a type that can be safely passed through esp_event_loop.
/// Must be trivially copyable, standard layout, and fit within EMBED_MAX_EVENT_DATA_SIZE.
template<typename T>
concept Message = std::is_trivially_copyable_v<T>
    && std::is_standard_layout_v<T>
    && sizeof(T) <= EMBED_MAX_EVENT_DATA_SIZE;

} // namespace embed
