#pragma once

#include "esp_err.h"
#include "nvs.h"

#include <cstddef>
#include <cstdint>

namespace embed {

/// Persistent KV store.
///
/// Prefers partition label `fctry` (not part of the default `idf.py flash`
/// image set, so USB reflash / OTA keep device identity). Falls back to the
/// default `nvs` partition when `fctry` is absent.
///
/// Call `initFlash()` once before WiFi or any `open()`.
class NvsStore {
public:
    static constexpr char kFactoryPartition[] = "fctry";

    NvsStore() = default;
    ~NvsStore() { close(); }

    NvsStore(const NvsStore&) = delete;
    NvsStore& operator=(const NvsStore&) = delete;
    NvsStore(NvsStore&&) = delete;
    NvsStore& operator=(NvsStore&&) = delete;

    /// Init default NVS (WiFi PHY) + optional `fctry`. Idempotent.
    static esp_err_t initFlash();

    /// True after a successful `fctry` init (settings survive `idf.py flash`).
    static bool hasFactoryPartition();

    esp_err_t open(const char* ns, nvs_open_mode_t mode = NVS_READWRITE);
    void close();

    [[nodiscard]] bool isOpen() const { return handle_ != 0; }
    [[nodiscard]] const char* partition() const { return partition_ ? partition_ : ""; }

    bool getString(const char* key, char* buf, size_t bufLen) const;
    esp_err_t setString(const char* key, const char* value);

    bool getU8(const char* key, uint8_t& out) const;
    esp_err_t setU8(const char* key, uint8_t value);

    bool getU16(const char* key, uint16_t& out) const;
    esp_err_t setU16(const char* key, uint16_t value);

    esp_err_t erase(const char* key);
    esp_err_t eraseAll();
    esp_err_t commit();

    /// Wipe `fctry` and re-init. No-op / NOT_FOUND if the partition is missing.
    static esp_err_t eraseFactoryPartition();

private:
    nvs_handle_t handle_ = 0;
    const char* partition_ = nullptr;
};

} // namespace embed
