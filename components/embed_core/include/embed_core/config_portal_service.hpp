#pragma once

#include "embed/embed.hpp"

#include <memory>

namespace embed {

/// HTTP config UI for WiFi + Crearts MQTT.
///
/// SoftAP / portal: `http://192.168.4.1/`
/// STA (if `CONFIG_EMBED_CONFIG_HTTP_STA`): `http://<device-ip>/`
///
/// Save writes `fctry` and reboots. Factory reset wipes identity and reboots
/// into the portal.
class ConfigPortalService : public Service {
public:
    ConfigPortalService();
    ~ConfigPortalService() override;

    const char* serviceName() const override { return "ConfigPortalService"; }

    void start() override;
    void stop() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace embed
