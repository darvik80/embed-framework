#pragma once

#include "crearts_iot/credentials.hpp"

#include <optional>
#include <string_view>

namespace crearts::iot {

/// Load device MQTT identity from NVS (`fctry` / `nvs`, namespace `crearts`).
///
/// If NVS has a token + ids + host, those win (survives OTA and USB flash).
/// Otherwise seed from the Kconfig values and persist. Returns nullopt if
/// neither NVS nor the seed is complete.
std::optional<CreartsCredentials> loadOrSeedCredentials(
    std::string_view seedProductId,
    std::string_view seedDeviceId,
    std::string_view seedHost,
    std::string_view seedAccessToken,
    TopicStyle seedStyle,
    bool seedUseTls,
    uint16_t seedPort);

} // namespace crearts::iot
