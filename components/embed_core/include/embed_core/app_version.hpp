#pragma once

#include "esp_app_desc.h"

namespace embed {

/// Returns the firmware version string embedded in the app binary.
/// The version is set by the project-root `version.txt` (standard ESP-IDF mechanism).
inline const char* appVersion()
{
    const esp_app_desc_t* desc = esp_app_get_description();
    return desc->version;
}

} // namespace embed
