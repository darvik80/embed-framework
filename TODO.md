# TODO — embed-framework

Prioritized from architecture review (2026-07-29). Skills: `.cursor/skills/embed-framework`, `embed-new-service`, `embed-new-component`.

## P0 — Correctness / safety

- [ ] **Camera frame ownership** — _(broader redesign deferred)_
- [x] **OssUploadService reclaim** — `releaseCameraFrame()` / `esp_camera_fb_return` when `fb != nullptr`.
- [x] **OTA partitions** — `partitions_ota.csv` + early reject on factory-only; see `docs/ota.md`.
- [x] **OTA off EventLoop** — `scheduleOtaUpdate()` → FreeRTOS task `ota_update`.
- [x] **OTA crash-loop rollback** — pending-verify + `checkCrashLoopRollback()` before TLS/services.

## P1 — Framework reliability

- [x] **Slot + ConnectionPool** — Unregister handler if pool allocate fails.
- [x] **EventLoop::post** — Bounded timeout (`EMBED_EVENT_POST_TIMEOUT_MS`); drop + log on failure.
- [x] **ConnectionPool locking** — Mutex when `EMBED_THREAD_SAFE=1`; EventLoop register/unregister uses mutex.
- [ ] **startAll lock scope** — Do not hold registry mutex across `Service::start()`.
- [ ] **EMBED_SERVICE_SIZE** — Remove unused `wifiConfig_` (or raise slot size).

## P2 — Feature completeness

- [ ] **Camera Kconfig / stop sync** — _(frozen with camera)_
- [x] **MQTT payload size** — `payload` capacity 1400; `EMBED_MAX_EVENT_DATA_SIZE` 1600; truncation logged.
- [ ] **ThingsBoard** — Access Token / Basic credentials + ThingsBoardService (telemetry, attributes, RPC). Client RPC / claim / protobuf TBD.
- [ ] **OssUploadService::stop** — _(frozen with camera)_
- [x] **Metrics storage** — SPIFFS `storage` + `esp_spiffs_info` when mounted.

## P3 — Structure / maintainability

- [ ] **Public headers** — Move `alicloud_common` / low-level OSS headers from `src/` to `include/`.
- [ ] **C++ dialect** — Unify on C++20 for new code.
- [x] **Dual MQTT reconnect** — esp-mqtt auto-reconnect disabled; SM + timer is sole policy.
- [x] **sdkconfig.defaults** — Site WiFi / Crearts token stay out of defaults; token empty in defaults, local `sdkconfig` gitignored.
- [x] **fctry NVS** — WiFi + Crearts identity persist across OTA / `idf.py flash`; Kconfig seeds first boot.
- [x] **Config portal** — SoftAP + HTTP WiFi/MQTT setup; GPIO/RPC factory reset does not re-seed Kconfig.
- [ ] **main.cpp** — Split demo vs product profile.
- [x] **Crearts IoT docs** — Root README, architecture, crearts_iot + deploy READMEs aligned with auth + Kconfig.
## P4 — Docs & tests

- [x] **README** + architecture / testing / OTA / CI docs + component READMEs for `embed` / `embed_core` / `crearts_iot`.
- [x] **Unity example** — `host_test/` (Linux, no flash) + `test_apps/embed_unity` + `components/embed/test/`.
- [x] **Gitea CI** — `host-tests` job runs Unity; firmware jobs use `tools/ci/idf-docker.sh`.
- [ ] Expand host coverage (types, StateMachine with mocks).
- [ ] Device smoke — WiFi/MQTT SM, OTA dry-run / Crearts connect.
