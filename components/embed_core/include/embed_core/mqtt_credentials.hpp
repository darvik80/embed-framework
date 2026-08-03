#pragma once

#include <cstddef>

namespace embed {

/// Abstract interface for MQTT connection credentials.
///
/// Different broker providers (Alibaba Cloud, AWS IoT, Azure, plain MQTT)
/// implement this interface to provide connection parameters.
///
/// Lifetime: Credential objects must outlive the MqttService that uses them.
/// Typically they are created as static globals before the service registry.
///
/// Usage:
///   class MyCredentials : public embed::MqttCredentials { ... };
///   static MyCredentials creds("mqtt://broker:1883", "client1", "user", "pass");
///   registry.createService<embed::MqttService>(creds);
class MqttCredentials {
public:
    virtual ~MqttCredentials() = default;

    /// Broker URI (e.g. "mqtt://broker.example.com:1883" or "mqtts://...").
    virtual const char* brokerUri() const = 0;

    /// MQTT client identifier.
    virtual const char* clientId() const = 0;

    /// MQTT username for authentication.
    virtual const char* username() const = 0;

    /// MQTT password for authentication.
    virtual const char* password() const = 0;

    /// Optional CA certificate for TLS (PEM format, null-terminated).
    /// Return nullptr if TLS is not needed.
    virtual const char* cert() const { return nullptr; }

    /// Length of the certificate buffer including the null terminator.
    /// Used by mbedtls_x509_crt_parse which requires buflen to include '\0'.
    /// Default: 0 (let MqttService fall back to strlen(cert()) + 1).
    virtual size_t certLen() const { return 0; }

    /// Optional Last Will topic. nullptr = no LWT.
    virtual const char* willTopic() const { return nullptr; }

    /// Optional Last Will payload (not necessarily null-terminated if willMessageLen > 0).
    virtual const char* willMessage() const { return nullptr; }

    /// Will payload length in bytes. 0 = use strlen(willMessage()).
    virtual size_t willMessageLen() const { return 0; }

    virtual int willQos() const { return 1; }
    virtual bool willRetain() const { return true; }
};

} // namespace embed
