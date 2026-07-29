# OTA

## Problem

1. Default [`partitions.csv`](../partitions.csv) is **factory-only** — `esp_ota_get_next_update_partition()` returns `nullptr`.
2. Download + flash used to run **inside** the MQTT / embed EventLoop Slot path and blocked all Signal delivery for the whole transfer.

## Solution (implemented)

### 1. Dual partitions when OTA is required

Use [`partitions_ota.csv`](../partitions_ota.csv) (`otadata` + `ota_0` + `ota_1` + SPIFFS):

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.ota" build
```

Or set in menuconfig: **Partition Table → Custom partition CSV** → `partitions_ota.csv`.

Keep factory-only `partitions.csv` for demos that do not need OTA.

### 2. Early reject without OTA slots

`OtaModule::scheduleOtaUpdate` checks for an update partition first and reports `FlashError` / logs instead of starting HTTP.

### 3. Dedicated FreeRTOS task

MQTT handlers only parse JSON and call `scheduleOtaUpdate()`.  
`performOtaUpdate()` runs on task `ota_update` so the embed EventLoop stays responsive.

Concurrent requests are ignored while `isUpdateInProgress()` is true.

```
MQTT Slot / EventLoop
        │
        ▼
 scheduleOtaUpdate() ──► xTaskCreate(ota_update)
                                │
                                ▼
                         performOtaUpdate()  (HTTP + flash)
                                │
                                ▼
                         esp_restart() on success
```
