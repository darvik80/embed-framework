#include "embed_core/firmware_slot.hpp"
#include "embed_core/device_settings.hpp"
#include "embed_core/nvs_store.hpp"

#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"

#include <cstring>

namespace embed {

static const char* TAG = "FwSlot";

static constexpr char kNs[] = "embed";
static constexpr char kVerKey[] = "otav";
static constexpr char kPendingKey[] = "otab";
static constexpr char kCrashKey[] = "crsh";
static constexpr uint8_t kFailLimit = 3;
static constexpr uint32_t kCrashClearUs = 15ULL * 1000 * 1000;
static constexpr uint32_t kCrashRtcMagic = 0x43525348; // 'CRSH'

struct CrashRtc {
    uint32_t magic;
    uint32_t count;
};

RTC_NOINIT_ATTR static CrashRtc s_crashRtc{};
static esp_timer_handle_t s_crashClearTimer = nullptr;

static bool isCrashReset(esp_reset_reason_t r)
{
    return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT ||
           r == ESP_RST_WDT;
}

static bool isPendingVerify()
{
    const esp_partition_t* run = esp_ota_get_running_partition();
    esp_ota_img_states_t st = ESP_OTA_IMG_UNDEFINED;
    if (!run || esp_ota_get_state_partition(run, &st) != ESP_OK) {
        return false;
    }
    return st == ESP_OTA_IMG_PENDING_VERIFY || st == ESP_OTA_IMG_NEW;
}

static const char* runningVersion()
{
    const esp_app_desc_t* d = esp_app_get_description();
    return d && d->version[0] ? d->version : "";
}

static void loadCounters(char* ver, size_t verLen, uint8_t& pending, uint8_t& crash)
{
    ver[0] = '\0';
    pending = 0;
    crash = 0;
    NvsStore store;
    if (store.open(kNs) != ESP_OK) {
        return;
    }
    store.getString(kVerKey, ver, verLen);
    store.getU8(kPendingKey, pending);
    store.getU8(kCrashKey, crash);
}

static void saveCounters(const char* ver, uint8_t pending, uint8_t crash)
{
    NvsStore store;
    if (store.open(kNs) != ESP_OK) {
        return;
    }
    store.setString(kVerKey, ver ? ver : "");
    if (pending == 0) {
        store.erase(kPendingKey);
    } else {
        store.setU8(kPendingKey, pending);
    }
    if (crash == 0) {
        store.erase(kCrashKey);
    } else {
        store.setU8(kCrashKey, crash);
    }
    store.commit();
}

static void zeroRtcCrash()
{
    s_crashRtc.magic = kCrashRtcMagic;
    s_crashRtc.count = 0;
}

static void clearCrashCounterTimer(void* /*arg*/)
{
    uint8_t pending = 0;
    uint8_t crash = 0;
    char ver[33]{};
    loadCounters(ver, sizeof(ver), pending, crash);
    if (crash != 0) {
        ESP_LOGI(TAG, "stable boot — clearing crash-loop counter");
        saveCounters(runningVersion(), pending, 0);
    }
    zeroRtcCrash();
    if (s_crashClearTimer) {
        esp_timer_delete(s_crashClearTimer);
        s_crashClearTimer = nullptr;
    }
}

static void armCrashClearTimer()
{
    if (s_crashClearTimer) {
        return;
    }
    const esp_timer_create_args_t args = {
        .callback = &clearCrashCounterTimer,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "fw_crash_clr",
    };
    if (esp_timer_create(&args, &s_crashClearTimer) == ESP_OK) {
        esp_timer_start_once(s_crashClearTimer, kCrashClearUs);
    }
}

static void doRollbackAndRestart(const char* why)
{
    ESP_LOGE(TAG, "%s — rolling back firmware", why);
    zeroRtcCrash();
    saveCounters(runningVersion(), 0, 0);
    if (esp_ota_mark_app_invalid_rollback_and_reboot() == ESP_OK) {
        return;
    }
    if (rollbackFirmware() == ESP_OK) {
        esp_restart();
    }
    ESP_LOGE(TAG, "rollback failed (no other OTA slot?)");
}

static void fillSlot(const esp_partition_t* part, FirmwareSlotInfo& out, bool running)
{
    out = {};
    out.running = running;
    if (!part) {
        return;
    }
    std::strncpy(out.label, part->label, sizeof(out.label) - 1);
    esp_app_desc_t desc{};
    if (esp_ota_get_partition_description(part, &desc) != ESP_OK) {
        return;
    }
    out.valid = true;
    std::strncpy(out.version, desc.version, sizeof(out.version) - 1);
    std::strncpy(out.project, desc.project_name, sizeof(out.project) - 1);
}

bool loadFirmwareSlots(FirmwareSlotInfo& running, FirmwareSlotInfo& other)
{
    running = {};
    other = {};
    const esp_partition_t* run = esp_ota_get_running_partition();
    fillSlot(run, running, true);
    if (!run) {
        return false;
    }
    const esp_partition_t* next = esp_ota_get_next_update_partition(run);
    if (next && next != run) {
        fillSlot(next, other, false);
    }
    return running.valid;
}

esp_err_t rollbackFirmware()
{
    const esp_partition_t* run = esp_ota_get_running_partition();
    if (!run) {
        return ESP_ERR_NOT_FOUND;
    }
    const esp_partition_t* prev = esp_ota_get_next_update_partition(run);
    if (!prev || prev == run) {
        ESP_LOGE(TAG, "no other OTA slot (use partitions_ota.csv)");
        return ESP_ERR_NOT_FOUND;
    }

    FirmwareSlotInfo other{};
    fillSlot(prev, other, false);
    if (!other.valid) {
        ESP_LOGE(TAG, "other slot %s has no firmware image", prev->label);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGW(TAG, "firmware rollback %s → %s (%s)", run->label, prev->label, other.version);
    const esp_err_t err = esp_ota_set_boot_partition(prev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set boot partition failed: %s", esp_err_to_name(err));
    }
    return err;
}

void checkCrashLoopRollback()
{
    const esp_reset_reason_t why = esp_reset_reason();
    const char* ver = runningVersion();

    uint8_t pending = 0;
    uint8_t crash = 0;
    char storedVer[33]{};
    loadCounters(storedVer, sizeof(storedVer), pending, crash);
    if (std::strcmp(storedVer, ver) != 0) {
        pending = 0;
        crash = 0;
    }

    if (s_crashRtc.magic != kCrashRtcMagic) {
        zeroRtcCrash();
    }

    if (isCrashReset(why)) {
        if (crash < 255) {
            ++crash;
        }
        if (s_crashRtc.count < 255) {
            ++s_crashRtc.count;
        }
        ESP_LOGW(TAG, "crash reset=%d nvs=%u rtc=%u", static_cast<int>(why), crash,
                 s_crashRtc.count);
    }

    // Config portal reboots must not look like a failed OTA.
    if (isPendingVerify() && !isConfigPortalRequested()) {
        if (pending < 255) {
            ++pending;
        }
        ESP_LOGW(TAG, "OTA pending verify boot %u/%u", pending, kFailLimit);
    }

    saveCounters(ver, pending, crash);

    if (isPendingVerify() && pending >= kFailLimit) {
        doRollbackAndRestart("OTA image did not finish init");
        return;
    }

    const uint32_t crashes = crash > s_crashRtc.count ? crash : s_crashRtc.count;
    if (crashes >= kFailLimit) {
        FirmwareSlotInfo running{};
        FirmwareSlotInfo other{};
        if (loadFirmwareSlots(running, other) && other.valid) {
            doRollbackAndRestart("panic/WDT crash-loop");
            return;
        }
        ESP_LOGE(TAG, "crash-loop but no previous firmware slot");
    }

    armCrashClearTimer();
}

void noteFirmwareConfirmed()
{
    zeroRtcCrash();
    saveCounters(runningVersion(), 0, 0);
}

void failUnconfirmedFirmware(const char* why)
{
    if (!isPendingVerify()) {
        return;
    }
    doRollbackAndRestart(why ? why : "unconfirmed firmware failed");
}

} // namespace embed
