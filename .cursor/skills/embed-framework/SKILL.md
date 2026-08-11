---
name: embed-framework
description: >-
  Coding conventions for the embed-framework ESP-IDF C++ project (ServiceRegistry,
  Signal/Slot, StateMachine, trivially-copyable Messages). Use when adding or
  editing services, messages, components, or when the user mentions embed::,
  Slot, Signal, ServiceRegistry, or ESP-IDF components in this repo.
---

# embed-framework conventions

ESP32-S3 · ESP-IDF ≥5.5 · C++20 framework (`-fno-exceptions`, RTTI in `embed`). Default PSRAM is **octal** (`N8R8` / Freenove CAM); quad vs missing PSRAM must not use `SPIRAM_BOOT_INIT` without `SPIRAM_IGNORE_NOTFOUND` or `cpu_start` aborts before `app_main`.

## Layering

```
main/                  # app wiring only (create services, credentials from Kconfig)
components/embed/      # framework primitives — no product logic
components/embed_core/ # WiFi, MQTT, Metrics
components/embed_extra/# Camera, MJPEG, WS2812 LED strip
components/crearts_iot/# Crearts IoT Platform device SDK (preferred cloud path)
components/alicloud_*/ # cloud providers (C + thin C++ wrappers OK; OSS + OssUploadService)
components/<vendor>/   # other providers implementing embed::MqttCredentials etc.
```

Do not put product/demo services in framework components. Demo services live in `main/`.
Secrets (WiFi password, Crearts access token) → NVS `fctry` (seeded from `sdkconfig` / menuconfig on first boot), not source literals. Factory reset / config portal: `factoryResetSettings()`, `ConfigPortalService`, RPC `factory_reset` / `config_portal` / `import_credentials`. JSON creds: `importCredentialsJson` / `GET /credentials.json`. OTA rollback: keep image `PENDING_VERIFY` until MQTT; `checkCrashLoopRollback()` early in `app_main` (bootloader rollback + 3 failed boots / panic-WDT).

## Service pattern

1. Inherit `embed::Service`, override `serviceName()`, `start()`, `stop()`.
2. Registry pool: object size ≤ `EMBED_SERVICE_SIZE` (default 512). Large state → heap (`unique_ptr`) like `AlicloudService`.
3. Create peers only in `start()` via `ServiceRegistry::getService<T>()` — all services exist by then.
4. Non-copyable; members trailing `_`; public `Signal<M>` for outbound events; private `Slot<M>` for inbound.
5. Slot callbacks are `static void(const M&, void* ctx)` with `this` as ctx.

```cpp
class FooService : public embed::Service {
public:
    const char* serviceName() const override { return "FooService"; }
    void start() override;
    void stop() override;
    Signal<FooDone> onDone;
private:
    Slot<WifiConnected> wifiSlot_{onWifi, this};
    static void onWifi(const WifiConnected& msg, void* ctx);
};
```

## Messages (Signal/Slot)

- Must satisfy `embed::Message`: trivially copyable, standard layout, `sizeof ≤ EMBED_MAX_EVENT_DATA_SIZE` (1024).
- Always `static_assert(embed::Message<T>);` next to the struct.
- Prefer POD / `embed::string<N>` — **no** owning pointers, `std::string`, or buffers in Messages.
- Signals are for control-plane events. Bulk data (camera frames, downloads) → FreeRTOS queue / dedicated task with explicit ownership.
- `Signal::emit` may drop under backpressure (`EMBED_EVENT_POST_TIMEOUT_MS`, default 100 ms).

## Ownership rules

| Resource | Rule |
|----------|------|
| Services | Placement-new in `ServiceRegistry`; lifetime = app |
| Credentials | `static` / outlives `MqttService` |
| Slot connections | RAII via `Connection`; disconnect on destroy |
| Camera FB | Never transfer sole ownership via Signal to N subscribers; single consumer or queue + `esp_camera_fb_return` |
| EventLoop handlers | Keep short; no blocking HTTP/TLS/OTA on the embed event task. Stack is `EMBED_EVENT_TASK_STACK_SIZE` (8192) |

## State machines

Use CRTP `StateMachine<Owner, States...>` with `State<On<Event, NextState>...>` and `onStateChanged(const TransitionTo<S>&)` like `WifiService` / `MqttService`.

## Error handling & logging

- Prefer `esp_err_t` + early return + `ESP_LOGE(TAG, ...)`.
- Avoid `ESP_ERROR_CHECK` in recoverable paths (aborts).
- One `static constexpr char TAG[]` (or string literal TAG) per translation unit.
- Kconfig: `CONFIG_EMBED_*` in component `Kconfig.projbuild`; wire into code (do not hardcode values that already have Kconfig).

## CMake / component layout

```
components/foo/
  CMakeLists.txt          # idf_component_register SRCS INCLUDE_DIRS REQUIRES
  idf_component.yml       # dependencies, description
  Kconfig.projbuild       # optional CONFIG_EMBED_*
  include/foo/*.hpp       # public API only
  src/*.cpp               # private
```

- Public headers under `include/<component>/`, not `src/`.
- `REQUIRES embed` (and peers) for anything using the framework.
- Match existing C++20 for framework/services; do not bump dialect casually.

## Anti-patterns (do not introduce)

- Owned heap/PSRAM pointers inside `Signal` messages with unclear reclaim.
- Blocking work (OTA download, OSS put, long TLS) inside Slot trampoline / EventLoop.
- `portMAX_DELAY` on event post without backpressure strategy (use `EMBED_EVENT_POST_TIMEOUT_MS`).
- Incomplete stubs left in `components/` (compile-break or remove).
- Secrets committed as real defaults in `sdkconfig.defaults` beyond placeholders.
- Competing MQTT reconnect (esp-mqtt auto-reconnect + custom timer) — keep SM-only policy.

## Reference files

- Lifecycle: `components/embed/include/embed/service.hpp`
- Limits: `components/embed/include/embed/config.hpp`
- Example SM + signals: `components/embed_core/include/embed_core/wifi_service.hpp`
- Hybrid pool/heap: `components/alicloud_iot/include/alicloud_iot/alicloud_service.hpp`
- App wiring: `main/main.cpp`
- Docs: `README.md`, `docs/architecture.md`, `docs/testing.md`
- Unity example: `test_apps/embed_unity/`
