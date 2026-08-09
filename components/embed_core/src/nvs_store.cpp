#include "embed_core/nvs_store.hpp"

#include "esp_log.h"
#include "esp_partition.h"
#include "nvs_flash.h"

namespace embed {

static const char* TAG = "NvsStore";

namespace {

bool g_inited = false;
bool g_fctryOk = false;

esp_err_t initOne(const char* label)
{
    esp_err_t err = label ? nvs_flash_init_partition(label) : nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "%s NVS erase (no pages / version)", label ? label : "nvs");
        if (label) {
            nvs_flash_erase_partition(label);
            err = nvs_flash_init_partition(label);
        } else {
            nvs_flash_erase();
            err = nvs_flash_init();
        }
    }
    return err;
}

} // namespace

esp_err_t NvsStore::initFlash()
{
    if (g_inited) {
        return ESP_OK;
    }

    esp_err_t err = initOne(nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "default NVS init failed: %s", esp_err_to_name(err));
        return err;
    }

    const esp_partition_t* fctry = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, kFactoryPartition);
    if (fctry) {
        err = initOne(kFactoryPartition);
        if (err == ESP_OK) {
            g_fctryOk = true;
            ESP_LOGI(TAG, "fctry ready offset=0x%lx size=%lu",
                     static_cast<unsigned long>(fctry->address),
                     static_cast<unsigned long>(fctry->size));
        } else {
            ESP_LOGW(TAG, "fctry init failed (%s) — using default nvs",
                     esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "no '%s' partition — settings live in default nvs "
                      "(erased by idf.py flash)",
                 kFactoryPartition);
    }

    g_inited = true;
    return ESP_OK;
}

bool NvsStore::hasFactoryPartition()
{
    return g_fctryOk;
}

esp_err_t NvsStore::open(const char* ns, nvs_open_mode_t mode)
{
    close();
    if (!ns || ns[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (!g_inited) {
        esp_err_t err = initFlash();
        if (err != ESP_OK) {
            return err;
        }
    }

    if (g_fctryOk) {
        esp_err_t err = nvs_open_from_partition(kFactoryPartition, ns, mode, &handle_);
        if (err == ESP_OK) {
            partition_ = kFactoryPartition;
            return ESP_OK;
        }
        ESP_LOGW(TAG, "open %s/%s failed (%s), fallback default nvs",
                 kFactoryPartition, ns, esp_err_to_name(err));
    }

    esp_err_t err = nvs_open(ns, mode, &handle_);
    if (err == ESP_OK) {
        partition_ = "nvs";
    } else {
        ESP_LOGE(TAG, "open nvs/%s failed: %s", ns, esp_err_to_name(err));
        handle_ = 0;
    }
    return err;
}

void NvsStore::close()
{
    if (handle_ != 0) {
        nvs_close(handle_);
        handle_ = 0;
        partition_ = nullptr;
    }
}

bool NvsStore::getString(const char* key, char* buf, size_t bufLen) const
{
    if (handle_ == 0 || !key || !buf || bufLen == 0) {
        return false;
    }
    size_t len = bufLen;
    return nvs_get_str(handle_, key, buf, &len) == ESP_OK;
}

esp_err_t NvsStore::setString(const char* key, const char* value)
{
    if (handle_ == 0 || !key) {
        return ESP_ERR_INVALID_STATE;
    }
    return nvs_set_str(handle_, key, value ? value : "");
}

bool NvsStore::getU8(const char* key, uint8_t& out) const
{
    if (handle_ == 0 || !key) {
        return false;
    }
    return nvs_get_u8(handle_, key, &out) == ESP_OK;
}

esp_err_t NvsStore::setU8(const char* key, uint8_t value)
{
    if (handle_ == 0 || !key) {
        return ESP_ERR_INVALID_STATE;
    }
    return nvs_set_u8(handle_, key, value);
}

bool NvsStore::getU16(const char* key, uint16_t& out) const
{
    if (handle_ == 0 || !key) {
        return false;
    }
    return nvs_get_u16(handle_, key, &out) == ESP_OK;
}

esp_err_t NvsStore::setU16(const char* key, uint16_t value)
{
    if (handle_ == 0 || !key) {
        return ESP_ERR_INVALID_STATE;
    }
    return nvs_set_u16(handle_, key, value);
}

esp_err_t NvsStore::erase(const char* key)
{
    if (handle_ == 0 || !key) {
        return ESP_ERR_INVALID_STATE;
    }
    return nvs_erase_key(handle_, key);
}

esp_err_t NvsStore::eraseAll()
{
    if (handle_ == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return nvs_erase_all(handle_);
}

esp_err_t NvsStore::eraseFactoryPartition()
{
    esp_err_t err = initFlash();
    if (err != ESP_OK) {
        return err;
    }
    if (!g_fctryOk) {
        return ESP_ERR_NOT_FOUND;
    }
    err = nvs_flash_erase_partition(kFactoryPartition);
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
        return err;
    }
    err = nvs_flash_init_partition(kFactoryPartition);
    g_fctryOk = (err == ESP_OK);
    return err;
}

esp_err_t NvsStore::commit()
{
    if (handle_ == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return nvs_commit(handle_);
}

} // namespace embed
