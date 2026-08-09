#pragma once

#include "esp_err.h"

namespace embed {

struct FirmwareSlotInfo {
    char label[17]{};
    char version[33]{};
    char project[33]{};
    bool running = false;
    bool valid = false;
};

/// `running` = current app; `other` = opposite OTA slot (previous firmware if valid).
bool loadFirmwareSlots(FirmwareSlotInfo& running, FirmwareSlotInfo& other);

/// Boot the other OTA slot on next reset. Does not reboot.
/// ESP_ERR_NOT_FOUND = no previous image (only one slot flashed).
esp_err_t rollbackFirmware();

/// Call once after `NvsStore::initFlash()`, before TLS / services.
/// If the new OTA image is still pending-verify and this is the N-th boot
/// without MQTT confirm, or panic/WDT happened N times in a row, rolls back
/// to the other slot and restarts (does not return).
void checkCrashLoopRollback();

/// Current image works (MQTT up / `esp_ota_mark_app_valid`). Clears the
/// pending-verify boot counter so later reboots do not look like a failed OTA.
void noteFirmwareConfirmed();

/// Pending-verify image failed before it could run (NVS/TLS/init). Rolls back
/// immediately if another slot exists; otherwise logs and returns.
void failUnconfirmedFirmware(const char* why);

} // namespace embed
