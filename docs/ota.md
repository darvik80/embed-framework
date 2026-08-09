# OTA

## Crearts IoT (`v1/me/o/upd`)

`CreartsOtaService` subscribes to `CreartsIotService::onOtaUpdate` / `onOtaCancel`.

Payload (HTTPS):

```json
{
  "version": "2.2.0",
  "module": "main",
  "size": 1048576,
  "url": "https://cdn.example.com/firmware.bin",
  "sha256": "…64 hex…",
  "force": false
}
```

Progress is published on `v1/me/o/p` (`0…100`, `-1` download fail, `-2` sha256 fail, `-3` flash fail, `-4` cancelled, `101` done).

## Firmware rollback

After a successful OTA the previous image stays in the other slot (`ota_0` ↔ `ota_1`).

| How | Effect |
|-----|--------|
| Web **Rollback firmware & reboot** | Boot the other slot |
| RPC `ota_rollback` `{ "confirm": true }` | Same |
| New image, MQTT not up in 90 s | App marks invalid + reboot |
| Crash / WDT / OOM **before init finishes** (reboot loop) | See below — web/MQTT never run |
| Hang (no panic) during init | Task WDT panic → same as crash loop |

### Crash before init (memory / panic loop)

Web UI and RPC cannot help: the new app never reaches HTTP or MQTT.

1. **Bootloader** (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` + `partitions_ota.csv`): after OTA the image is `ESP_OTA_IMG_PENDING_VERIFY`. Do **not** call `esp_ota_mark_app_valid` until MQTT is up. A panic/WDT reset while still pending → next bootloader pass boots the previous slot. Requires a **bootloader rebuild** (`idf.py bootloader-flash` or full flash), not only `app-flash`.
2. **App crash-loop counter** (`checkCrashLoopRollback()` at the start of `app_main`, after NVS): 3 boots of a still-pending image, or 3 panic/WDT resets in a row, → `esp_ota_mark_app_invalid_rollback_and_reboot` / other slot. Works even if bootloader rollback was left off in `sdkconfig`. Survives ESP32-S3 `POWERON` (counter in NVS, not only RTC).
3. **USB** `idf.py app-flash` of a known-good `.bin` if the other slot is empty (factory-only table or first flash).

Live `sdkconfig` in this tree still had rollback **off** until reconfigure:

```bash
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.ota" fullclean build flash
```

If the other slot is empty (only USB-flashed once), rollback is impossible — flash an older `.bin` with `idf.py app-flash`.

Lab HTTP (`http://…`) is accepted; HTTPS uses the CRT bundle. MQTT chunk stream is not implemented.

## Device credentials (`fctry`)

WiFi SSID/password and Crearts product/device/host/token live in NVS partition **`fctry`** (`0x7FC000`, 16 KB) — same offset in `partitions.csv` and `partitions_ota.csv`.

- First boot: seed from Kconfig (`sdkconfig`) and persist.
- Later boots / OTA / `idf.py flash`: NVS wins (Kconfig token may be empty in CI builds).
- `idf.py flash` rewrites default `nvs` (PHY cal) but **does not** flash `fctry`.
- Wipe / force-update identity (does **not** re-seed Kconfig):
  - Hold **BOOT** (GPIO 0) ~3 s **while running** (not at chip reset)
  - Press **EN/RST** 3× quickly (within 10 s)
  - RPC `factory_reset` with `{ "confirm": true }`
  - Config page **Factory reset credentials** (`http://192.168.4.1/` or STA IP)
- Reconfigure without wipe: RPC `config_portal`, then SoftAP form (prefilled).

## Problem

1. Default [`partitions.csv`](../partitions.csv) is **factory-only** — `esp_ota_get_next_update_partition()` returns `nullptr`.
2. Download + flash used to run **inside** the MQTT / embed EventLoop Slot path and blocked all Signal delivery for the whole transfer.

## Solution (implemented)

### 1. Dual partitions when OTA is required

Use [`partitions_ota.csv`](../partitions_ota.csv) (`otadata` + `ota_0` + `ota_1` + SPIFFS + `fctry`):

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
