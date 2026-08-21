# embed-framework — Developer Manual

> **Version:** 1.0  
> **Target:** ESP32-S3 / ESP-IDF 5.x  
> **Last updated:** August 2026

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Architecture Overview](#2-architecture-overview)
3. [Getting Started](#3-getting-started)
4. [`embed` — Core Framework](#4-embed--core-framework)
   - [4.1 Trivially-Copyable Primitives](#41-trivially-copyable-primitives)
   - [4.2 `embed::string`](#42-embedstring)
   - [4.3 Message Concept](#43-message-concept)
   - [4.4 Service & ServiceRegistry](#44-service--serviceregistry)
   - [4.5 EventLoop](#45-eventloop)
   - [4.6 Signal & Slot](#46-signal--slot)
   - [4.7 Connection & ConnectionPool](#47-connection--connectionpool)
   - [4.8 StateMachine](#48-statemachine)
   - [4.9 Crypto](#49-crypto)
   - [4.10 Compile-Time Configuration](#410-compile-time-configuration)
5. [`embed_core` — Platform Services](#5-embed_core--platform-services)
   - [5.1 NvsStore](#51-nvsstore)
   - [5.2 Device Settings](#52-device-settings)
   - [5.3 WifiService](#53-wifiservice)
   - [5.4 MqttCredentials & MqttService](#54-mqttcredentials--mqttservice)
   - [5.5 MetricsService](#55-metricsservice)
   - [5.6 ConfigPortalService](#56-configportalservice)
   - [5.7 Firmware Slot & OTA Safety](#57-firmware-slot--ota-safety)
6. [`embed_extra` — Peripheral Services](#6-embed_extra--peripheral-services)
   - [6.1 LedStripService](#61-ledstripservice)
   - [6.2 CameraService](#62-cameraservice)
   - [6.3 MjpegService](#63-mjpegservice)
7. [`cogitor_iot` — Cogitor IoT Platform SDK](#7-cogitor_iot--cogitor-iot-platform-sdk)
   - [7.1 CogitorCredentials](#71-cogitorcredentials)
   - [7.2 Credential Store (NVS)](#72-credential-store-nvs)
   - [7.3 Topics & Topic Styles](#73-topics--topic-styles)
   - [7.4 IotService](#74-iotservice)
   - [7.5 Telemetry](#75-telemetry)
   - [7.6 Attributes](#76-attributes)
   - [7.7 RPC](#77-rpc)
   - [7.8 NTP](#78-ntp)
   - [7.9 OTA](#79-ota)
   - [7.10 DeviceInfo](#710-deviceinfo)
   - [7.11 MetricsTelemetryBridge](#711-metricstelemetrybridge)
8. [Integration with iot-platform-go](#8-integration-with-iot-platform-go)
9. [Complete Application Example](#9-complete-application-example)
10. [Kconfig Reference](#10-kconfig-reference)
11. [Appendix: MQTT Topic Map](#11-appendix-mqtt-topic-map)

---

## 1. Introduction

**embed-framework** is a C++20 service-oriented framework for ESP32 devices running ESP-IDF. It provides:

- A **zero-heap service registry** with placement-new slots
- A **typed pub/sub system** (Signal/Slot) built on `esp_event`
- A **CRTP state machine** for modeling service lifecycles
- **Trivially-copyable containers** (`string`, `array`, `optional`, `variant`, `pair`, `tuple`) safe for event-loop transport
- Ready-made services for **WiFi**, **MQTT**, **metrics**, **OTA**, **LED strips**, **camera**, and **Cogitor IoT Platform** integration

The framework is split into four ESP-IDF components:

| Component | Purpose |
|-----------|---------|
| `embed` | Core primitives — no ESP-IDF service dependencies |
| `embed_core` | WiFi, MQTT, NVS, metrics, config portal |
| `embed_extra` | Camera, MJPEG streaming, WS2812 LED strips |
| `cogitor_iot` | Cogitor IoT Platform device SDK (protocol v1) |

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────────┐
│ main (product wiring, Kconfig, RPC handlers)    │
├─────────────────────────────────────────────────┤
│ cogitor_iot  (IoT Platform SDK, protocol v1)    │
│ alicloud_*   (Alibaba Cloud IoT)                │
│ thingsboard  (ThingsBoard client)               │
├─────────────────────────────────────────────────┤
│ embed_extra  (camera, mjpeg, led strip)         │
│ embed_core   (wifi, mqtt, metrics, nvs, portal) │
├─────────────────────────────────────────────────┤
│ embed (registry, event loop, signal/slot, SM)   │
└─────────────────────────────────────────────────┘
```

**Dependencies flow downward only.** Cloud provider components implement `embed::MqttCredentials` and consume `MqttService` signals — they never become required by `embed` itself.

### Event paths

| Path | Loop | Use |
|------|------|-----|
| `Signal<M>` / `Slot<M>` | Custom `embed_evt` loop | Service-to-service POD messages |
| WiFi / IP events | Default ESP-IDF loop | Handled inside `WifiService`, translated to Signals |
| MQTT client events | MQTT task | Handled inside `MqttService`, translated to Signals |

### Ownership model

| Object | Owner |
|--------|-------|
| Service instances | `ServiceRegistry` pool (process lifetime) |
| MQTT credentials | Caller (`static` in `app_main`) |
| Slot connections | `Slot` → `Connection` → pool entry |
| Camera frame buffers | Camera driver (zero-copy) or heap |

---

## 3. Getting Started

### Prerequisites

- ESP-IDF 5.x installed, `IDF_PATH` set
- CMake 3.16+
- C++20 capable toolchain (GCC 12+ via ESP-IDF)

### Minimal Application

```cpp
#include "embed/embed.hpp"
#include "embed_core/wifi_service.hpp"
#include "embed_core/metrics_service.hpp"
#include "embed_core/nvs_store.hpp"
#include "esp_log.h"

extern "C" void app_main() {
    // 1. Initialize NVS (required before WiFi)
    embed::NvsStore::initFlash();

    // 2. Initialize the framework event loop
    embed::EventLoop::instance().init();

    // 3. Create services
    auto& registry = embed::ServiceRegistry::instance();
    registry.createService<embed::WifiService>();
    registry.createService<embed::MetricsService>();

    // 4. Start all services (calls start() in creation order)
    registry.startAll();

    ESP_LOGI("app", "Running with %zu services", registry.count());
}
```

### CMakeLists.txt (top-level)

```cmake
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my-iot-device)
```

### Component dependency

In your component's `idf_component.yml`:

```yaml
dependencies:
  embed:
    path: components/embed
  embed_core:
    path: components/embed_core
  cogitor_iot:
    path: components/cogitor_iot
```

---

## 4. `embed` — Core Framework

Include everything with a single header:

```cpp
#include "embed/embed.hpp"
```

Or include specific headers for minimal compilation:

```cpp
#include "embed/service.hpp"
#include "embed/signal.hpp"
```

### 4.1 Trivially-Copyable Primitives

All containers in `embed::` are **trivially copyable** — they can be safely memcpy'd through the event loop. This is enforced with `static_assert`.

#### `embed::pair<A, B>`

```cpp
embed::pair<int, float> p{42, 3.14f};
auto [x, y] = p;  // structured bindings supported
```

#### `embed::tuple<Types...>`

```cpp
embed::tuple<int, float, char> t{1, 2.0f, 'a'};
int& x = t.get<0>();
```

#### `embed::array<T, N>`

```cpp
embed::array<int, 4> arr;
arr.fill(0);
arr[0] = 42;
```

#### `embed::optional<T>`

```cpp
embed::optional<int> opt;          // empty
opt = 42;                          // has value
int v = opt.value_or(0);           // 42
opt.reset();                       // empty again
```

#### `embed::variant<Types...>`

```cpp
embed::variant<int, float, char> v;
v = 42;
bool isInt = v.holds_alternative<int>();  // true
v.visit([](auto& val) { /* dispatch */ });
```

### 4.2 `embed::string`

Fixed-capacity, null-terminated, trivially-copyable string. Suitable for event-loop messages.

```cpp
embed::string<63> name("hello");
name = "world";
name += "!";

bool found  = name.find('o') != embed::string<63>::npos;
bool prefix = name.starts_with("wo");
bool has    = name.contains('r');
```

Key properties:
- Internal buffer is `Capacity + 1` bytes (null terminator)
- Truncates silently if source exceeds capacity
- Supports cross-size copy: `embed::string<31>(longer_string)` truncates to 31 chars
- Constructible from `std::string`, `std::string_view`, `const char*`

### 4.3 Message Concept

A type can flow through the event loop if it satisfies `embed::Message<M>`:

```cpp
template<typename T>
concept Message = std::is_trivially_copyable_v<T>
    && std::is_standard_layout_v<T>
    && sizeof(T) <= EMBED_MAX_EVENT_DATA_SIZE;  // default 1600 bytes
```

Define your own messages:

```cpp
struct ButtonPressed {
    int gpio;
    bool level;
};
static_assert(embed::Message<ButtonPressed>);  // compile-time check
```

### 4.4 Service & ServiceRegistry

#### `embed::Service`

Base class for all services. Provides lifecycle hooks:

```cpp
class MyService : public embed::Service {
public:
    const char* serviceName() const override { return "MyService"; }
    void start() override { /* init hardware, connect signals */ }
    void stop()  override { /* release resources */ }
};
```

**Lifecycle:**
1. **Constructor** — called by `createService<T>()`
2. **`start()`** — called by `startAll()`. All services exist at this point; `getService<T>()` is safe.
3. **`stop()`** — called by `stopAll()` in reverse creation order.

#### `embed::ServiceRegistry`

Singleton, zero-heap service container. Services are placement-new'd into fixed-size slots.

```cpp
auto& registry = embed::ServiceRegistry::instance();

// Create services (constructor args forwarded)
auto* wifi    = registry.createService<embed::WifiService>();
auto* metrics = registry.createService<embed::MetricsService>();

// Retrieve by type (uses dynamic_cast — requires RTTI)
auto* wifi2 = registry.getService<embed::WifiService>();
assert(wifi == wifi2);

// Check existence
if (registry.hasService<embed::MqttService>()) { /* ... */ }

// Lifecycle
registry.startAll();  // calls start() in creation order
registry.stopAll();   // calls stop() in reverse order
```

**Pool limits:**
- `EMBED_MAX_SERVICES` (default 16) — max number of services
- `EMBED_SERVICE_SIZE` (default 512 bytes) — max size per service object
- If a service is too large, increase `EMBED_SERVICE_SIZE` via CMake:
  ```cmake
  target_compile_definitions(embed PUBLIC EMBED_SERVICE_SIZE=1024)
  ```
- Duplicate `createService<T>()` returns the existing instance

### 4.5 EventLoop

Wraps `esp_event` with a dedicated loop (not the system default).

```cpp
// Initialize once at startup
embed::EventLoop::instance().init();

// Post a message (from any task)
embed::EventLoop::instance().post<MyMessage>(base, id, msg);
```

The event loop runs on its own FreeRTOS task:
- **Stack:** `EMBED_EVENT_TASK_STACK_SIZE` (default 8192)
- **Priority:** `EMBED_EVENT_TASK_PRIORITY` (default 5)
- **Queue:** `EMBED_EVENT_QUEUE_SIZE` (default 32 entries)
- **Post timeout:** `EMBED_EVENT_POST_TIMEOUT_MS` (default 100 ms, then drop)

**Important:** Keep Slot handlers short. Blocking operations (HTTP, TLS, flash writes) must run on a dedicated task.

### 4.6 Signal & Slot

Type-safe publish/subscribe built on `EventLoop`.

#### Signal

A signal emits messages of type `M`:

```cpp
struct TemperatureReading {
    float celsius;
    uint32_t sensorId;
};
static_assert(embed::Message<TemperatureReading>);

class SensorService : public embed::Service {
public:
    embed::Signal<TemperatureReading> onTemperature;
    
    void readSensor() {
        TemperatureReading msg{.celsius = 22.5f, .sensorId = 1};
        onTemperature.emit(msg);
    }
};
```

#### Slot

A slot receives messages from a signal:

```cpp
class DisplayService : public embed::Service {
    embed::Slot<TemperatureReading> tempSlot_{onTemp, this};
    
    static void onTemp(const TemperatureReading& msg, void* ctx) {
        auto* self = static_cast<DisplayService*>(ctx);
        // Update display with msg.celsius
    }
    
    void start() override {
        auto* sensor = embed::ServiceRegistry::instance()
                           .getService<SensorService>();
        if (sensor) {
            tempSlot_.connect(sensor->onTemperature);
        }
    }
};
```

**Key points:**
- Callback signature: `void(const M& msg, void* ctx)`
- `ctx` is the user pointer passed to the `Slot` constructor
- Multiple slots can connect to the same signal
- When a `Slot` is destroyed, it auto-disconnects
- Slots are non-copyable, non-movable

### 4.7 Connection & ConnectionPool

`Connection` is an RAII handle returned by `Slot::connect()`:

```cpp
embed::Connection conn = slot.connect(signal);
if (conn.connected()) { /* subscription active */ }
conn.disconnect();  // or let it go out of scope
```

`ConnectionPool` manages up to `EMBED_MAX_CONNECTIONS` (default 64) simultaneous subscriptions. Thread-safe when `EMBED_THREAD_SAFE=1`.

### 4.8 StateMachine

CRTP-based hierarchical state machine. Each state declares valid transitions via `State<On<Event, TargetState>...>`.

```cpp
// 1. Define states
struct IdleState;
struct RunningState;
struct ErrorState;

// 2. Define events
struct StartEvent {};
struct StopEvent {};
struct FaultEvent {};

// 3. Define transitions
struct IdleState : embed::State<
    embed::On<StartEvent, RunningState>
> {};

struct RunningState : embed::State<
    embed::On<StopEvent, IdleState>,
    embed::On<FaultEvent, ErrorState>
> {};

struct ErrorState : embed::State<
    embed::On<StopEvent, IdleState>
> {};

// 4. Create the machine
class Motor : public embed::StateMachine<Motor, IdleState, RunningState, ErrorState> {
public:
    // Called after each transition
    void onStateChanged(const embed::TransitionTo<RunningState>&) {
        ESP_LOGI("Motor", "Now running");
    }
    void onStateChanged(const embed::TransitionTo<IdleState>&) {
        ESP_LOGI("Motor", "Now idle");
    }
    void onStateChanged(const embed::Nothing&) {
        // No transition (event not handled in current state)
    }
};

// 5. Use
Motor motor;
motor.handle(StartEvent{});   // Idle → Running
motor.handle(FaultEvent{});   // Running → Error
motor.handle(StopEvent{});    // Error → Idle
```

**Features:**
- `handle(event)` — dispatches to current state's `handle()` method
- `handle(event, ctx)` — passes extra context to `onStateChanged()`
- `getCurrentState()` / `getPrevState()` — `std::variant<States*...>`
- `transitionTo<State>()` — manual transition (used inside `execute()`)

### 4.9 Crypto

Incremental and one-shot hashing:

```cpp
#include "embed/crypto.hpp"

// One-shot SHA-256
uint8_t digest[32];
embed::crypto::sha256(data, len, digest);

// Incremental SHA-256
embed::crypto::Sha256 hasher;
hasher.update(chunk1, len1);
hasher.update(chunk2, len2);
hasher.finish(digest);

// HMAC-SHA256
uint8_t mac[32];
embed::crypto::hmacSha256(key, keyLen, msg, msgLen, mac);

// Incremental MD5 (for Alibaba OTA)
embed::crypto::Md5 md5;
md5.update(data, len);
uint8_t md5digest[16];
md5.finish(md5digest);
```

### 4.10 Compile-Time Configuration

All limits are overridable via CMake `target_compile_definitions`:

| Macro | Default | Purpose |
|-------|---------|---------|
| `EMBED_MAX_SERVICES` | 16 | Max services in registry |
| `EMBED_SERVICE_SIZE` | 512 | Max bytes per service object |
| `EMBED_MAX_CONNECTIONS` | 64 | Max signal-slot connections |
| `EMBED_EVENT_QUEUE_SIZE` | 32 | Event loop queue depth |
| `EMBED_EVENT_TASK_PRIORITY` | 5 | Event loop FreeRTOS priority |
| `EMBED_EVENT_TASK_STACK_SIZE` | 8192 | Event loop task stack |
| `EMBED_EVENT_POST_TIMEOUT_MS` | 100 | Post timeout (ms), -1 = block forever |
| `EMBED_THREAD_SAFE` | 1 | Enable mutex protection |
| `EMBED_MAX_EVENT_DATA_SIZE` | 1600 | Max message payload bytes |
| `EMBED_MAX_CUSTOM_METRICS` | 4 | Custom metric slots in MetricsService |

---

## 5. `embed_core` — Platform Services

```cpp
#include "embed_core/wifi_service.hpp"
#include "embed_core/mqtt_service.hpp"
#include "embed_core/metrics_service.hpp"
#include "embed_core/nvs_store.hpp"
#include "embed_core/device_settings.hpp"
#include "cogitor_iot/config_portal_service.hpp"
#include "embed_core/firmware_slot.hpp"
```

### 5.1 NvsStore

Persistent key-value store. Prefers the `fctry` partition (survives OTA and USB reflashing). Falls back to default `nvs` if `fctry` is absent.

```cpp
#include "embed_core/nvs_store.hpp"

// Initialize once at startup (before WiFi or any NVS access)
embed::NvsStore::initFlash();

// Check if factory partition exists
bool hasFactory = embed::NvsStore::hasFactoryPartition();

// Use
embed::NvsStore store;
store.open("my_ns", NVS_READWRITE);
store.setString("ssid", "MyNetwork");
store.setU16("port", 8883);

char buf[64];
store.getString("ssid", buf, sizeof(buf));

store.commit();
store.close();

// Factory reset: wipe fctry
embed::NvsStore::eraseFactoryPartition();
```

**Partition strategy:**
- `nvs` — default partition (WiFi PHY data, system)
- `fctry` — device identity (WiFi creds, MQTT token, product/device IDs). Not touched by `idf.py flash`.

### 5.2 Device Settings

WiFi settings live in embed_core; Cogitor MQTT identity in cogitor_iot.

```cpp
#include "embed_core/device_settings.hpp"

// Load WiFi settings
embed::WifiSettings wifi;
if (embed::loadWifiSettings(wifi)) {
    // wifi.ssid, wifi.password are populated
}

// Load Cogitor MQTT settings
cogitor::iot::CogitorSettings cogitor;
if (cogitor::iot::loadSettings(cogitor)) {
    // cogitor.product, cogitor.device, cogitor.host,
    // cogitor.token, cogitor.port, cogitor.useTls, cogitor.topicShort
}

// Save settings
embed::saveWifiSettings(wifi);
cogitor::iot::saveSettings(cogitor);

// Check if settings are complete
bool complete = cogitor::iot::settingsComplete(cogitor);

// Backup / restore
cogitor::iot::backupDeviceSettings();                    // active → backup NVS namespace
cogitor::iot::restoreDeviceSettingsBackup();             // backup → active

// Factory reset (wipe active, keep backup)
cogitor::iot::factoryResetSettings();

// Config portal detection
bool needs = embed::needsConfigPortal();    // true if portal flag set or no WiFi SSID
embed::setConfigPortalRequested(true);      // force portal on next boot

// Import/export credentials JSON
char jsonBuf[2048];
cogitor::iot::exportCredentialsJson(jsonBuf, sizeof(jsonBuf), true);  // include secrets
cogitor::iot::importCredentialsJson(jsonStr);      // parse + save + clear portal flag

// Reboot helpers
embed::scheduleReboot(1000);  // reboot in 1 second

// Factory reset via GPIO (BOOT button hold)
embed::startFactoryResetGpioWatch();
if (embed::factoryResetGpioHeld()) {
    cogitor::iot::factoryResetSettings();
}

// Factory reset via RST burst (EN/RST pressed N times quickly)
if (embed::checkRstBurstFactoryReset()) {
    cogitor::iot::factoryResetSettings();
}
```

**Credentials JSON format:**
```json
{
  "wifi": {
    "ssid": "MyNetwork",
    "password": "secret"
  },
  "cogitor": {
    "product": "home",
    "device": "esp32-s3",
    "host": "192.168.1.100",
    "token": "abc123",
    "port": 1883,
    "useTls": false,
    "topicShort": true
  }
}
```

### 5.3 WifiService

WiFi STA service with state machine and signal-slot integration.

**States:** `Idle → Scanning → Connecting → Connected ↔ Disconnected → Error`

```cpp
auto* wifi = registry.createService<embed::WifiService>();

// For config portal mode (SoftAP):
wifi->enableSoftAp();  // call BEFORE start()

// Signals
embed::Slot<embed::WifiConnected> wifiConnSlot_{onWifiConn, this};
embed::Slot<embed::WifiDisconnected> wifiDisconnSlot_{onWifiDisconn, this};

static void onWifiConn(const embed::WifiConnected& msg, void* ctx) {
    ESP_LOGI("app", "WiFi connected, IP: %s", msg.ip.c_str());
}

static void onWifiDisconn(const embed::WifiDisconnected& msg, void* ctx) {
    ESP_LOGW("app", "WiFi disconnected, reason=%u", msg.reason);
}

void start() override {
    auto* wifi = embed::ServiceRegistry::instance().getService<embed::WifiService>();
    wifiConnSlot_.connect(wifi->onConnected);
    wifiDisconnSlot_.connect(wifi->onDisconnected);
}
```

**Signals emitted:**

| Signal | When | Payload |
|--------|------|---------|
| `onConnected` | WiFi up + IP obtained | `WifiConnected{ ip: string<17> }` |
| `onDisconnected` | WiFi lost or error | `WifiDisconnected{ reason: uint8_t }` |

**SSID/Password resolution:**
1. NVS `fctry` (set by config portal or `saveWifiSettings()`)
2. Kconfig seed (`CONFIG_EMBED_WIFI_SSID` / `CONFIG_EMBED_WIFI_PASSWORD`) — only on first boot or after portal flag is cleared

**SoftAP mode:**
```cpp
wifi->enableSoftAp();
// SSID: "{CONFIG_EMBED_CONFIG_AP_SSID_PREFIX}-{MAC4}{MAC5}"
// e.g., "embed-A1B2"
registry.startAll();
// Config portal HTTP available at http://192.168.4.1/
```

### 5.4 MqttCredentials & MqttService

#### MqttCredentials (abstract interface)

```cpp
#include "embed_core/mqtt_credentials.hpp"

class MqttCredentials {
public:
    virtual const char* brokerUri() const = 0;    // "mqtt://host:1883" or "mqtts://..."
    virtual const char* clientId() const = 0;
    virtual const char* username() const = 0;
    virtual const char* password() const = 0;
    virtual const char* cert() const { return nullptr; }      // PEM CA cert
    virtual size_t certLen() const { return 0; }
    virtual const char* willTopic() const { return nullptr; }  // LWT topic
    virtual const char* willMessage() const { return nullptr; }
    virtual size_t willMessageLen() const { return 0; }
    virtual int willQos() const { return 1; }
    virtual bool willRetain() const { return true; }
};
```

Implement this for any MQTT broker (Cogitor, AWS IoT, Azure, plain MQTT).

#### MqttService

State machine-driven MQTT client. Connects when WiFi is up, disconnects when WiFi is down.

**States:** `Idle → Connecting → Connected ↔ Disconnected → Error`

```cpp
// Create a credentials object (must outlive MqttService)
static MyCredentials creds("mqtt://broker:1883", "client1", "user", "pass");
auto* mqtt = registry.createService<embed::MqttService>(creds);

// Signals
mqtt->onConnected       // Signal<MqttConnected>
mqtt->onDisconnected    // Signal<MqttDisconnected>
mqtt->onMessage         // Signal<MqttMessageReceived>

// Publish
mqtt->publish("my/topic", "{\"key\":\"value\"}", strlen(data), 1, false);

// Subscribe / unsubscribe
mqtt->subscribe("my/topic/down/#", 1);
mqtt->unsubscribe("my/topic/down/#");

// Check connection
bool connected = mqtt->isConnected();
```

**Signals emitted:**

| Signal | Payload | Description |
|--------|---------|-------------|
| `onConnected` | `MqttConnected{ brokerUri: string<127> }` | Broker session established |
| `onDisconnected` | `MqttDisconnected{ reason: uint8_t }` | Session lost |
| `onMessage` | `MqttMessageReceived{ topic: string<127>, payload: string<1400> }` | Incoming message |

**Important:**
- Payload capacity is `embed::string<1400>` — longer messages are truncated with a warning log
- Reconnect is driven by the state machine + timer (`CONFIG_EMBED_MQTT_RECONNECT_INTERVAL_MS`)
- esp-mqtt auto-reconnect is **disabled** — the state machine handles it

### 5.5 MetricsService

Periodic system metrics collection with custom metric registration.

```cpp
auto* metrics = registry.createService<embed::MetricsService>();

// Register a custom metric
static float getBatteryLevel(float& out, void* ctx) {
    out = 87.5f;  // read from ADC
    return 0;
}
metrics->registerCustomMetric("battery", getBatteryLevel, nullptr);

// Subscribe to metrics
embed::Slot<embed::MetricsCollected> metricsSlot_{onMetrics, this};

static void onMetrics(const embed::MetricsCollected& m, void* ctx) {
    ESP_LOGI("metrics",
        "cpu=%u%% heap=%u dram=%u/%u psram=%u uptime=%us wifi_rssi=%d",
        m.cpuUsagePercent, m.freeHeap, m.freeDram, m.totalDram,
        m.freePsram, m.uptimeSeconds, m.wifiRssi);
    
    // Custom metrics
    for (uint8_t i = 0; i < m.customMetricsCount; i++) {
        ESP_LOGI("metrics", "  %s = %.2f",
            m.customMetrics[i].name.c_str(), m.customMetrics[i].value);
    }
}

void start() override {
    auto* metrics = embed::ServiceRegistry::instance()
                        .getService<embed::MetricsService>();
    metricsSlot_.connect(metrics->onMetricsCollected);
}
```

**`MetricsCollected` fields:**

| Field | Type | Description |
|-------|------|-------------|
| `timestamp` | `int64_t` | Unix ms |
| `cpuUsagePercent` | `uint8_t` | CPU load 0–100 |
| `freeHeap` | `uint32_t` | Free heap (all 8-bit, internal + PSRAM) |
| `minFreeHeap` | `uint32_t` | Low watermark |
| `largestFreeBlock` | `uint32_t` | Largest contiguous block |
| `freeDram` | `uint32_t` | Internal DRAM free |
| `minFreeDram` | `uint32_t` | DRAM low watermark |
| `largestFreeDramBlock` | `uint32_t` | Largest DRAM block |
| `totalDram` | `uint32_t` | Total DRAM |
| `freePsram` | `uint32_t` | PSRAM free |
| `minFreePsram` | `uint32_t` | PSRAM low watermark |
| `uptimeSeconds` | `uint32_t` | Uptime |
| `storageTotalBytes` | `uint64_t` | SPIFFS total |
| `storageUsedBytes` | `uint64_t` | SPIFFS used |
| `wifiRssi` | `int8_t` | WiFi signal strength |
| `wifiConnected` | `bool` | WiFi link state |
| `wifiIp` | `string<17>` | IP address |
| `customMetricsCount` | `uint8_t` | Number of custom metrics |
| `customMetrics` | `array<CustomMetricEntry, EMBED_MAX_CUSTOM_METRICS>` | Name + value pairs |

Collection interval: `CONFIG_EMBED_METRICS_INTERVAL_MS` (default 10000 ms).

### 5.6 ConfigPortalService

HTTP setup UI for WiFi + Cogitor MQTT (`cogitor_iot::ConfigPortalService`).

```cpp
registry.createService<cogitor::iot::ConfigPortalService>();
```

**Endpoints:**
- SoftAP: `http://192.168.4.1/` (always available when portal is active)
- STA (optional): `http://<device-ip>/` (when `CONFIG_EMBED_CONFIG_HTTP_STA=y`)

**Features:**
- Web form to enter WiFi SSID/password and Cogitor MQTT settings
- JSON import/export (`GET /credentials.json`)
- Save writes to `fctry` and reboots
- Factory reset button in UI (wipes identity, reboots into portal)

### 5.7 Firmware Slot & OTA Safety

```cpp
#include "embed_core/firmware_slot.hpp"

// Load info about running and other OTA slot
embed::FirmwareSlotInfo running, other;
embed::loadFirmwareSlots(running, other);
ESP_LOGI("ota", "Running: %s v%s (valid=%d)",
    running.label, running.version, running.valid);

// Crash-loop protection — call early in app_main
embed::checkCrashLoopRollback();
// If the new OTA image has booted N times without MQTT confirm,
// or panic/WDT happened N times, rolls back to other slot + restarts.

// Mark firmware as confirmed (MQTT connected / app works)
embed::noteFirmwareConfirmed();

// Rollback to previous slot on next reset
embed::rollbackFirmware();

// Fail unconfirmed firmware immediately (rolls back if possible)
embed::failUnconfirmedFirmware("TLS init failed");
```

---

## 6. `embed_extra` — Peripheral Services

```cpp
#include "embed_extra/led_strip_service.hpp"
#include "embed_extra/camera_service.hpp"
#include "embed_extra/mjpeg_service.hpp"
```

### 6.1 LedStripService

Multiple WS2812/SK6812 strips via RMT. Bind GPIO at runtime.

```cpp
auto* leds = registry.createService<embed::LedStripService>();

// In start() or from an RPC handler:
leds->attach(48, 16);        // 16 LEDs on GPIO 48
leds->attach(21, 8, 64);     // 8 LEDs on GPIO 21, brightness 64

// Control
leds->setRange(48, 0, 4, 255, 0, 0);    // LEDs 0-3: red
leds->setPixel(48, 5, 0, 255, 0);        // LED 5: green
leds->refresh(48);                        // push to hardware
leds->fill(21, 0, 0, 255);               // all blue
leds->clear(48);                          // all off
leds->setBrightness(48, 128);             // 50% brightness

// Query
bool has = leds->attached(48);
uint16_t count = leds->ledCount(48);
uint8_t strips = leds->stripCount();

// List all strips
embed::LedStripInfo infos[8];
uint8_t n = leds->list(infos, 8);

// Detach
leds->detach(21);
leds->detachAll();

// Signal: emitted on every successful setRange/fill/clear
embed::Slot<embed::LedStripChanged> ledSlot_{onLedChanged, this};
```

**Kconfig:**
- `CONFIG_EMBED_LED_STRIP` — enable service (default `y`)
- `CONFIG_EMBED_LED_STRIP_MAX` — max concurrent strips (default 4, limited by RMT channels)
- `CONFIG_EMBED_LED_STRIP_DEFAULT_BRIGHTNESS` — default brightness (0–255)

### 6.2 CameraService

JPEG frame capture from ESP32-CAM boards.

```cpp
auto* cam = registry.createService<embed::CameraService>();

// Subscribe to frames
embed::Slot<embed::CameraFrame> frameSlot_{onFrame, this};

static void onFrame(const embed::CameraFrame& frame, void* ctx) {
    // frame.data points to JPEG bytes (zero-copy from camera driver)
    // frame.len is the byte count
    // frame.seq is a monotonic sequence number
    
    // IMPORTANT: release the frame when done
    embed::releaseCameraFrame(const_cast<embed::CameraFrame&>(frame));
}

void start() override {
    auto* cam = embed::ServiceRegistry::instance()
                    .getService<embed::CameraService>();
    frameSlot_.connect(cam->onFrame);
}
```

**Kconfig:**
- `CONFIG_EMBED_CAMERA_BOARD` — `FREENOVE_S3` or `AITHINKER`
- `CONFIG_EMBED_CAMERA_XCLK_FREQ_HZ` — XCLK frequency (default 10 MHz)
- `CONFIG_EMBED_CAMERA_JPEG_QUALITY` — 1 (best) to 63 (worst), default 10
- `CONFIG_EMBED_CAMERA_FB_COUNT` — DMA frame buffers (default 2, double-buffered)
- `CONFIG_EMBED_CAMERA_CAPTURE_INTERVAL_MS` — capture interval (default 50 ms ≈ 20 fps)

### 6.3 MjpegService

Streams MJPEG over HTTP by subscribing to `CameraService`.

```cpp
auto* cam   = registry.createService<embed::CameraService>();
auto* mjpeg = registry.createService<embed::MjpegService>();
registry.startAll();
// Connect browser to http://<device-ip>/stream
```

**Kconfig:**
- `CONFIG_EMBED_MJPEG_PORT` — HTTP server port (default 80)
- `CONFIG_EMBED_MJPEG_QUEUE_DEPTH` — frame buffer depth (default 2)
- `CONFIG_EMBED_MJPEG_STACK_SIZE` — HTTP task stack (default 8192)

---

## 7. `cogitor_iot` — Cogitor IoT Platform SDK

Device-side SDK for the [Cogitor IoT Platform](#8-integration-with-iot-platform-go) (protocol v1). Implements the full MQTT spec: telemetry, attributes, RPC, NTP, OTA, events, logs.

```cpp
#include "cogitor_iot/cogitor_iot.hpp"
```

### 7.1 CogitorCredentials

MQTT credentials for the Cogitor platform. Implements `embed::MqttCredentials`.

**Auth model:**
```
client_id = username = {product_id}.{device_id}
password  = <access_token>
```

**LWT (Last Will and Testament):**
- Topic: `v1/s` (short) or `iot/v1/{product}/{device}/up/status` (full)
- Payload: `{"online":false,"ts":0,"reason":"lwt"}`
- Retained, QoS 1

```cpp
#include "cogitor_iot/cogitor_iot.hpp"

// Production: access token from dashboard
static auto creds = cogitor::iot::CogitorCredentials::createAccessToken(
    "home",                    // product_id
    "esp32-s3-001",           // device_id
    "192.168.1.100",          // broker host
    "dashboard-issued-token",  // access token
    cogitor::iot::TopicStyle::Short,  // or Full
    false,                     // useTls
    0                          // port (0 = default 1883/8883)
);

// Lab: explicit username/password
static auto creds = cogitor::iot::CogitorCredentials::createBasic(
    "home", "device1", "broker.local",
    "myuser", "mypass",
    cogitor::iot::TopicStyle::Short
);

if (!creds || !creds->isValid()) {
    ESP_LOGE("app", "Invalid Cogitor credentials");
    return;
}

// Use with MqttService
registry.createService<embed::MqttService>(*creds);
```

### 7.2 Credential Store (NVS)

Load credentials from NVS `fctry`, seeding from Kconfig on first boot:

```cpp
static auto creds = cogitor::iot::loadOrSeedCredentials(
    CONFIG_EMBED_COGITOR_IOT_PRODUCT_ID,
    CONFIG_EMBED_COGITOR_IOT_DEVICE_ID,
    CONFIG_EMBED_COGITOR_IOT_HOST,
    CONFIG_EMBED_COGITOR_IOT_ACCESS_TOKEN,
#ifdef CONFIG_EMBED_COGITOR_IOT_TOPIC_SHORT
    cogitor::iot::TopicStyle::Short,
#else
    cogitor::iot::TopicStyle::Full,
#endif
#ifdef CONFIG_EMBED_COGITOR_IOT_USE_TLS
    true,
#else
    false,
#endif
    static_cast<uint16_t>(CONFIG_EMBED_COGITOR_IOT_PORT)
);
```

**Behavior:**
1. If NVS `fctry` has complete settings → load from NVS (survives OTA/flash)
2. Otherwise → seed from Kconfig values, persist to NVS
3. Returns `nullopt` if neither is complete

**Do not hardcode the access token in source.** Use `menuconfig` or the config portal.

### 7.3 Topics & Topic Styles

Two topic styles, same payloads:

| Style | Example | When to use |
|-------|---------|-------------|
| **Short** | `v1/t`, `v1/a`, `v1/r/req` | Constrained devices, less flash |
| **Full** | `iot/v1/home/esp32-s3/up/telemetry/data` | Gateways, multi-tenant, self-describing |

The `Topics` class builds all topic strings:

```cpp
cogitor::iot::Topics topics("home", "esp32-s3", cogitor::iot::TopicStyle::Short);

std::string t = topics.telemetryPublish();       // "v1/me/t" (short) or full path
std::string r = topics.rpcRequestSubscribe();     // "v1/me/r/req"
std::string d = topics.downstreamSubscribe();     // "v1/me/#"
```

**Topic constants** are in `cogitor::iot::topic_str`:

```cpp
namespace cogitor::iot::topic_str {
    namespace dir   { kUp, kDown }
    namespace cap   { kStatus, kTelemetry, kEvents, kAttributes, kRpc, kNtp, kOta, kLogs }
    namespace op    { kData, kPost, kReport, kRequest, kResponse, kUpdate, ... }
    namespace short_topic { kStatus, kTelemetry, kRpcRequest, ... }
    namespace full_suffix { kStatus, kTelemetry, kRpcRequest, ... }
}
```

### 7.4 IotService

Central service for Cogitor IoT protocol. Subscribes to downstream topics on MQTT connect, dispatches incoming messages to signals.

```cpp
auto* iot = registry.createService<cogitor::iot::IotService>(*creds);
```

**Signals emitted:**

| Signal | Payload | Trigger |
|--------|---------|---------|
| `onAttributeUpdate` | `AttributeUpdate{ payload: string<767> }` | Desired attribute push from platform |
| `onAttributeResponse` | `AttributeResponse{ requestId, payload }` | Response to attribute request |
| `onRpcRequest` | `RpcRequest{ requestId, method, params }` | RPC call from platform |
| `onNtpResponse` | `NtpResponse{ requestId, deviceSendTime, serverRecvTime, serverSendTime }` | NTP reply |
| `onOtaUpdate` | `OtaUpdate{ payload: string<1400> }` | OTA notification from platform |
| `onOtaCancel` | `OtaCancel{ payload: string<255> }` | OTA cancelled by platform |

**Publish methods:**

```cpp
// Telemetry
iot->publishTelemetry(R"({"temperature":22.5})");
iot->publishTelemetry(builder);   // TelemetryBuilder
iot->publishTelemetry(batch);     // TelemetryBatch

// Events
iot->publishEvents(R"({"ts":1705312200000,"type":"alarm","code":"TEMP_HIGH"})");

// Attributes (reported)
iot->publishAttributes(R"({"firmwareVersion":"2.1.0"})");
iot->publishAttributes(builder);  // AttributeBuilder

// Request attributes from platform
uint32_t reqId;
cogitor::iot::AttributeRequestBuilder req;
req.addDesired("targetTemperature").addReported("firmwareVersion");
iot->requestAttributes(req, reqId);

// RPC response
iot->respondRpc(requestId, 0, "ok", R"({"rebooting":true})");

// NTP
int64_t sendTimeMs = esp_timer_get_time() / 1000;
uint32_t ntpReqId;
iot->requestNtp(ntpReqId, sendTimeMs);

// OTA version report
iot->publishOtaVersion("2.1.0", "main");

// OTA query (ask platform if update available)
iot->publishOtaQuery("main", "2.1.0");

// OTA progress
iot->publishOtaProgress("main", 50, "Downloading firmware");

// Device logs
iot->publishLogs(R"([{"ts":1705312200000,"level":"ERROR","module":"wifi","message":"lost"}])");
```

### 7.5 Telemetry

#### TelemetryBuilder

Builds telemetry JSON in three formats:

```cpp
// 1. Simple KV (server stamps receive time)
cogitor::iot::TelemetryBuilder builder;
builder.add("temperature", 22.5)
       .add("humidity", 61)
       .add("cpuUsage", 45);
std::string json = builder.build();
// → {"temperature":22.5,"humidity":61,"cpuUsage":45}

// 2. Client timestamp
builder.timestampMs(1705312200000);
std::string json = builder.build();
// → {"ts":1705312200000,"values":{"temperature":22.5,...}}

// 3. Batch (multiple timestamped entries)
cogitor::iot::TelemetryBatch batch;
cogitor::iot::TelemetryBuilder b1;
b1.timestampMs(1705312200000).add("temperature", 22.5);
cogitor::iot::TelemetryBuilder b2;
b2.timestampMs(1705312201000).add("temperature", 23.0);
batch.add(std::move(b1));
batch.add(std::move(b2));
std::string json = batch.build();
// → [{"ts":...,"values":{...}},{"ts":...,"values":{...}}]
```

**Supported value types:** `double`, `int64_t`, `int`, `bool`, `const char*`, `std::string_view`, raw JSON.

#### Publish

```cpp
iot->publishTelemetry(builder);   // auto-builds JSON
iot->publishTelemetry(batch);     // batch format
iot->publishTelemetry(rawJson);   // pre-built string_view
```

### 7.6 Attributes

#### AttributeBuilder (reported)

```cpp
cogitor::iot::AttributeBuilder builder;
builder.add("firmwareVersion", "2.1.0")
       .add("model", "ESP32-S3")
       .add("serialNumber", "SN-4A21F");
iot->publishAttributes(builder);
// → {"firmwareVersion":"2.1.0","model":"ESP32-S3","serialNumber":"SN-4A21F"}
```

#### AttributeRequestBuilder (request from platform)

```cpp
cogitor::iot::AttributeRequestBuilder req;
req.addReported("firmwareVersion")
   .addDesired("targetTemperature")
   .addDesired("enabled");
uint32_t reqId;
iot->requestAttributes(req, reqId);
// → {"id":7,"reported":["firmwareVersion"],"desired":["targetTemperature","enabled"]}
```

#### Handling attribute updates (desired push from platform)

```cpp
embed::Slot<cogitor::iot::AttributeUpdate> attrSlot_{onAttrUpd, this};

static void onAttrUpd(const cogitor::iot::AttributeUpdate& upd, void* ctx) {
    // upd.payload is flat JSON of changed desired keys
    double targetTemp;
    if (cogitor::iot::attributeGetNumber(upd.payload, "targetTemperature", targetTemp)) {
        // Apply new target temperature
    }
    bool enabled;
    if (cogitor::iot::attributeGetBool(upd.payload, "enabled", enabled)) {
        // Enable/disable device
    }
}
```

#### Parsing attribute responses

```cpp
auto values = cogitor::iot::parseAttributeResponse(payload);
// values.reportedJson → {"firmwareVersion":"2.1.0",...}
// values.desiredJson  → {"targetTemperature":25,...}
```

### 7.7 RPC

#### Registering RPC methods

```cpp
auto* iot = embed::ServiceRegistry::instance()
                .getService<cogitor::iot::IotService>();

// Define parameters
static constexpr cogitor::iot::RpcParamDef kEcho[] = {
    cogitor::iot::rpcStr("msg")
};

static constexpr cogitor::iot::RpcParamDef kSetLed[] = {
    cogitor::iot::rpcInt("gpio"),
    cogitor::iot::rpcInt("offset"),
    cogitor::iot::rpcInt("length"),
    cogitor::iot::rpcInt("r", false, 255),      // optional, default 255
    cogitor::iot::rpcInt("g", false, 255),
    cogitor::iot::rpcInt("b", false, 255),
    cogitor::iot::rpcBool("on", false, true),    // optional, default true
};

// Handler signature
static void onEcho(cogitor::iot::IotService& iot,
                   uint32_t requestId,
                   const cogitor::iot::RpcParams& params,
                   void* ctx)
{
    std::string msg;
    if (!params.get("msg", msg)) {
        iot.respondRpc(requestId, 400, "missing params.msg");
        return;
    }
    // Success: code=0
    iot.respondRpc(requestId, 0, "ok", R"({"msg":")" + msg + R"("})");
}

// Register
iot->rpc().add("echo", kEcho, onEcho, nullptr, "Echo a string");
iot->rpc().add("set_led", kSetLed, onSetLed, nullptr, "Set LED range color");

// No-params method
iot->rpc().add("reboot", onReboot, nullptr, "Reboot device");
```

#### Parameter helpers

```cpp
// Factory functions for parameter definitions
cogitor::iot::rpcInt("name")                    // required int
cogitor::iot::rpcInt("name", false, 42)         // optional int, default 42
cogitor::iot::rpcBool("name")                   // required bool
cogitor::iot::rpcBool("name", false, true)       // optional bool, default true
cogitor::iot::rpcStr("name")                    // required string
cogitor::iot::rpcStr("name", false, "default")   // optional string, default
cogitor::iot::rpcNum("name")                    // required number (double)
cogitor::iot::rpcNum("name", false, 3.14)        // optional number, default
```

#### RpcParams (inside handler)

```cpp
int gpio;
params.get("gpio", gpio);           // returns false if missing

int brightness = params.getInt("brightness", 128);  // with fallback
bool enabled   = params.getBool("enabled", true);
std::string name = params.getString("name", "default");
```

#### Built-in: `rpc-list`

Every device automatically supports `rpc-list` — returns the full method catalog:

```json
{ "id": 1, "method": "rpc-list", "params": {} }
```

Response:
```json
{
  "id": 1, "code": 0, "message": "ok",
  "data": [
    { "method": "rpc-list", "params": {}, "required": [] },
    { "method": "echo", "params": { "msg": { "type": "string", "required": true } }, "required": ["msg"] },
    { "method": "set_led", "description": "Set LED range color",
      "params": {
        "gpio": { "type": "int", "required": true },
        "r": { "type": "int", "required": false, "default": 255 }
      },
      "required": ["gpio", "offset", "length"]
    }
  ]
}
```

The dashboard RPC console calls `rpc-list` to auto-generate forms.

#### RPC status codes

| Code | Meaning |
|------|---------|
| `0` | Success |
| `400` | Bad parameters |
| `404` | Unknown method |
| `408` | Handler timeout |
| `409` | Busy / conflicting state |
| `500` | Internal error |
| `501` | Not implemented |

Max registered methods: `RpcRegistry::kMaxMethods` = 24.

### 7.8 NTP

Request time synchronization with the platform:

```cpp
int64_t deviceSendTimeMs = esp_timer_get_time() / 1000;
uint32_t requestId;
iot->requestNtp(requestId, deviceSendTimeMs);

// Handle response
embed::Slot<cogitor::iot::NtpResponse> ntpSlot_{onNtp, this};

static void onNtp(const cogitor::iot::NtpResponse& resp, void* ctx) {
    int64_t deviceRecvTimeMs = esp_timer_get_time() / 1000;
    int64_t rtt = deviceRecvTimeMs - resp.deviceSendTime;
    int64_t serverTime = resp.serverSendTime + rtt / 2;
    // Use serverTime to set system clock
}
```

### 7.9 OTA

`OtaService` handles firmware updates from the platform automatically.

```cpp
registry.createService<cogitor::iot::OtaService>();
```

**Flow:**
1. Platform publishes OTA notification on `v1/me/o/upd`
2. `OtaService` parses firmware JSON (version, url, sha256, size)
3. Downloads firmware on a dedicated FreeRTOS task
4. Verifies SHA-256 digest
5. Flashes to OTA partition
6. Reports progress via `v1/me/o/p` (0–100%, negative = error)
7. Reboots into new image
8. On successful MQTT connect: confirms firmware (`noteFirmwareConfirmed()`)

**Progress step codes:**

| Step | Meaning |
|------|---------|
| `0`–`100` | Download/install percent |
| `-1` | Download failed |
| `-2` | Checksum mismatch |
| `-3` | Flash failed |
| `-4` | Cancelled |
| `101` | Success |

**Safety:**
- Crash-loop rollback: if new firmware fails to connect MQTT after N boots, auto-rolls back
- `checkCrashLoopRollback()` — call early in `app_main`
- `noteFirmwareConfirmed()` — called automatically when MQTT connects

**OTA notification JSON (from platform):**
```json
{
  "version": "2.2.0",
  "module": "main",
  "size": 1048576,
  "url": "https://cdn.example.com/firmware/v2.2.0.bin",
  "sha256": "e3b0c44298fc1c149afbf4c8996fb924..."
}
```

### 7.10 DeviceInfo

Reports static **reported** attributes on each MQTT connect and requests **desired** attributes:

```cpp
registry.createService<cogitor::iot::DeviceInfo>();
```

On MQTT connect, `DeviceInfo` publishes:
- Firmware version, chip model, free heap, WiFi MAC, etc.

And requests all desired attributes from the platform.

### 7.11 MetricsTelemetryBridge

Forwards `MetricsService` data to Cogitor IoT telemetry automatically:

```cpp
registry.createService<cogitor::iot::MetricsTelemetryBridge>();
```

On each `MetricsCollected` signal, builds and publishes telemetry JSON:

```json
{
  "cpuUsage": 45,
  "freeHeap": 125000,
  "freeDram": 80000,
  "totalDram": 327680,
  "freePsram": 4194304,
  "uptimeSeconds": 3600,
  "wifiRssi": -65,
  "wifiConnected": true
}
```

---

## 8. Integration with iot-platform-go

The [Cogitor IoT Platform](https://github.com/your-org/iot-platform-go) (`iot-platform-go`) is a self-hosted, single-binary IoT backend with embedded MQTT broker, SQLite, and React dashboard.

### Platform Setup

```yaml
# docker-compose.yml
services:
  platform:
    build: .
    ports:
      - "8080:8080"   # HTTP + Dashboard
      - "1883:1883"   # MQTT
    volumes:
      - platform_data:/data
    environment:
      - PUBLIC_BASE_URL=http://<host-ip>:8080
      - JWT_SECRET=<change-me>
```

### Device Provisioning

1. **Create a Product** in the dashboard (defines schema: RPC methods, telemetry metrics, attributes)
2. **Register a Device** → dashboard shows a one-time access token
3. Copy the token to firmware via `idf.py menuconfig` → `CONFIG_EMBED_COGITOR_IOT_ACCESS_TOKEN`
4. Or use the **Config Portal** (SoftAP + HTTP) to enter credentials on-device

### MQTT Connection

```
Broker:    <platform-host>:1883
Username:  {product_id}.{device_id}
Password:  <access_token>
Client ID: {product_id}.{device_id}
```

The platform auto-detects topic style (short vs full) from the first device packet.

### Data Flow

```
Device → Platform:
  Telemetry:   v1/t  →  stored in SQLite, WebSocket to dashboard, rules bus
  Attributes:  v1/a  →  stored as reported attributes
  RPC reply:   v1/r/res  →  correlated by "id", shown in RPC console
  OTA progress: v1/o/p  →  OTA job status update

Platform → Device:
  Desired attrs: v1/a/upd  →  pushed when user edits in dashboard
  RPC request:   v1/r/req  →  from dashboard RPC console or automation
  OTA update:    v1/o/upd  →  when new firmware uploaded + notify
  NTP response:  v1/n/res  →  time sync
```

### Node-RED Automation

The platform publishes normalized events on `platform/v1/events/{product}/{device}/...`.  
Node-RED sends commands on `platform/v1/commands/{product}/{device}/...`.

Example Node-RED flow: when temperature > 80, send RPC `set_led` to turn LED red:

```
[platform event] → [filter: telemetry.temperature > 80] → [command: rpc/set_led]
```

### Auto-OTA

Enable Auto-OTA on the product in the dashboard. When new firmware is uploaded:
1. Platform pushes OTA notification to all online devices of that product
2. Device downloads, verifies, flashes, reboots
3. On reconnect, platform marks device as updated

### MCP (AI Integration)

The platform exposes an MCP endpoint at `/mcp`. Any MCP-compatible AI client (Cursor, Claude, etc.) can:
- List devices and products
- Read telemetry
- Invoke RPC methods
- Set desired attributes

---

## 9. Complete Application Example

Full `app_main` wiring for a Cogitor IoT device with WiFi, MQTT, metrics, OTA, LED strip, and custom RPC:

```cpp
#include "esp_crt_bundle.h"
#include "embed/embed.hpp"
#include "embed_core/wifi_service.hpp"
#include "embed_core/metrics_service.hpp"
#include "embed_core/mqtt_service.hpp"
#include "embed_core/nvs_store.hpp"
#include "embed_core/device_settings.hpp"
#include "embed_core/firmware_slot.hpp"
#include "cogitor_iot/config_portal_service.hpp"
#include "cogitor_iot/cogitor_iot.hpp"
#include "embed_extra/led_strip_service.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "sdkconfig.h"

#include <optional>

// ── Custom RPC handlers ────────────────────────────────────────────────

class MyRpcHandlers : public embed::Service {
public:
    const char* serviceName() const override { return "MyRpc"; }

    void start() override {
        auto* iot = embed::ServiceRegistry::instance()
                        .getService<cogitor::iot::IotService>();
        if (!iot) return;

        using namespace cogitor::iot;

        static constexpr RpcParamDef kSetBrightness[] = {
            rpcInt("gpio"), rpcInt("brightness", false, 128)
        };

        iot->rpc().add("set_brightness", kSetBrightness,
            [](IotService& iot, uint32_t id, const RpcParams& p, void*) {
                int gpio = p.getInt("gpio", -1);
                int bri  = p.getInt("brightness", 128);
                auto* leds = embed::ServiceRegistry::instance()
                                 .getService<embed::LedStripService>();
                if (!leds || !leds->attached(gpio)) {
                    iot.respondRpc(id, 404, "strip not found");
                    return;
                }
                leds->setBrightness(gpio, static_cast<uint8_t>(bri));
                iot.respondRpc(id, 0, "ok");
            }, nullptr, "Set strip brightness");
    }
};

// ── Monitor (optional logging service) ─────────────────────────────────

class MonitorService : public embed::Service {
public:
    const char* serviceName() const override { return "Monitor"; }

    void start() override {
        auto& reg = embed::ServiceRegistry::instance();
        if (auto* wifi = reg.getService<embed::WifiService>()) {
            wifiConnSlot_.connect(wifi->onConnected);
            wifiDisconnSlot_.connect(wifi->onDisconnected);
        }
        if (auto* mqtt = reg.getService<embed::MqttService>()) {
            mqttConnSlot_.connect(mqtt->onConnected);
        }
    }

private:
    static void onWifiConn(const embed::WifiConnected& m, void*) {
        ESP_LOGI("Monitor", "WiFi UP, IP=%s", m.ip.c_str());
    }
    static void onWifiDisconn(const embed::WifiDisconnected& m, void*) {
        ESP_LOGW("Monitor", "WiFi DOWN reason=%u", m.reason);
    }
    static void onMqttConn(const embed::MqttConnected& m, void*) {
        ESP_LOGI("Monitor", "MQTT → %s", m.brokerUri.c_str());
    }

    embed::Slot<embed::WifiConnected> wifiConnSlot_{onWifiConn, this};
    embed::Slot<embed::WifiDisconnected> wifiDisconnSlot_{onWifiDisconn, this};
    embed::Slot<embed::MqttConnected> mqttConnSlot_{onMqttConn, this};
};

// ── app_main ───────────────────────────────────────────────────────────

extern "C" void app_main() {
    ESP_LOGI("app", "Starting IoT device");

    // Factory reset detection
    embed::startFactoryResetGpioWatch();
    embed::NvsStore::initFlash();

    if (embed::checkRstBurstFactoryReset() || embed::factoryResetGpioHeld()) {
        ESP_LOGW("app", "Factory reset!");
        cogitor::iot::factoryResetSettings();
    }

    embed::checkCrashLoopRollback();
    esp_tls_init_global_ca_store();
    esp_crt_bundle_attach(NULL);
    embed::EventLoop::instance().init();

    // Determine topic style from Kconfig
    const auto topicStyle =
#ifdef CONFIG_EMBED_COGITOR_IOT_TOPIC_SHORT
        cogitor::iot::TopicStyle::Short;
#else
        cogitor::iot::TopicStyle::Full;
#endif

    // Load or seed Cogitor credentials from NVS
    const bool portal = embed::needsConfigPortal();
    static std::optional<cogitor::iot::CogitorCredentials> creds;
    if (!portal) {
        creds = cogitor::iot::loadOrSeedCredentials(
            CONFIG_EMBED_COGITOR_IOT_PRODUCT_ID,
            CONFIG_EMBED_COGITOR_IOT_DEVICE_ID,
            CONFIG_EMBED_COGITOR_IOT_HOST,
            CONFIG_EMBED_COGITOR_IOT_ACCESS_TOKEN,
            topicStyle,
#ifdef CONFIG_EMBED_COGITOR_IOT_USE_TLS
            true,
#else
            false,
#endif
            static_cast<uint16_t>(CONFIG_EMBED_COGITOR_IOT_PORT));
    }

    // Build service graph
    auto& registry = embed::ServiceRegistry::instance();

    auto* wifi = registry.createService<embed::WifiService>();
    if (portal) wifi->enableSoftAp();

    registry.createService<embed::MetricsService>();
    registry.createService<cogitor::iot::ConfigPortalService>();

    const bool mqttOk = !portal && creds && creds->isValid();
    if (mqttOk) {
        registry.createService<embed::MqttService>(*creds);
        registry.createService<cogitor::iot::IotService>(*creds);
        registry.createService<cogitor::iot::MetricsTelemetryBridge>();
        registry.createService<cogitor::iot::OtaService>();
        registry.createService<cogitor::iot::DeviceInfo>();
        registry.createService<MyRpcHandlers>();
    }

#ifdef CONFIG_EMBED_LED_STRIP
    registry.createService<embed::LedStripService>();
#endif

    registry.createService<MonitorService>();
    registry.startAll();

    if (portal) {
        ESP_LOGW("app", "Config portal AP=%s → http://192.168.4.1/", wifi->apSsid());
    } else if (mqttOk) {
        ESP_LOGI("app", "Running → %.*s.%.*s",
            (int)creds->productId().size(), creds->productId().data(),
            (int)creds->deviceId().size(), creds->deviceId().data());
    }

    // Confirm firmware is working (prevents crash-loop rollback)
    if (mqttOk) {
        embed::noteFirmwareConfirmed();
    }

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
```

---

## 10. Kconfig Reference

### WiFi (`embed_core`)

| Symbol | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_EMBED_WIFI_SSID` | string | `"MyNetwork"` | WiFi SSID |
| `CONFIG_EMBED_WIFI_PASSWORD` | string | `""` | WiFi password (empty = open) |
| `CONFIG_EMBED_WIFI_MAX_RETRY` | int | 5 | Max reconnection attempts |

### Config Portal (`embed_core`)

| Symbol | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_EMBED_CONFIG_HTTP_STA` | bool | y | Serve config UI on STA IP |
| `CONFIG_EMBED_CONFIG_HTTP_PORT` | int | 80 | Config HTTP port |
| `CONFIG_EMBED_CONFIG_AP_SSID_PREFIX` | string | `"embed"` | SoftAP SSID prefix |
| `CONFIG_EMBED_CONFIG_AP_PASSWORD` | string | `""` | SoftAP WPA2 password |
| `CONFIG_EMBED_CONFIG_RESET_GPIO` | int | 0 | Factory reset GPIO (-1 = disable) |
| `CONFIG_EMBED_CONFIG_RESET_HOLD_MS` | int | 3000 | Hold time for factory reset |
| `CONFIG_EMBED_CONFIG_RST_BURST_COUNT` | int | 3 | EN/RST presses for factory reset |

### MQTT (`embed_core`)

| Symbol | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_EMBED_MQTT_BROKER_URI` | string | `"mqtt://broker.example.com:1883"` | Broker URI |
| `CONFIG_EMBED_MQTT_CLIENT_ID` | string | `"esp32s3-device"` | Client ID |
| `CONFIG_EMBED_MQTT_USERNAME` | string | `""` | MQTT username |
| `CONFIG_EMBED_MQTT_PASSWORD` | string | `""` | MQTT password |
| `CONFIG_EMBED_MQTT_MAX_RETRY` | int | 5 | Max reconnect attempts |
| `CONFIG_EMBED_MQTT_RECONNECT_INTERVAL_MS` | int | 5000 | Reconnect interval |
| `CONFIG_EMBED_MQTT_KEEPALIVE` | int | 120 | Keepalive (seconds) |

### Cogitor IoT (`cogitor_iot`)

| Symbol | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_EMBED_COGITOR_IOT_PRODUCT_ID` | string | `"home"` | Product ID |
| `CONFIG_EMBED_COGITOR_IOT_DEVICE_ID` | string | `"esp32-s3"` | Device ID |
| `CONFIG_EMBED_COGITOR_IOT_HOST` | string | `"192.168.1.100"` | Broker host (LAN IP) |
| `CONFIG_EMBED_COGITOR_IOT_ACCESS_TOKEN` | string | `""` | Access token (MQTT password) |
| `CONFIG_EMBED_COGITOR_IOT_USE_TLS` | bool | n | Use TLS (mqtts) |
| `CONFIG_EMBED_COGITOR_IOT_PORT` | int | 0 | Port (0 = default) |
| `CONFIG_EMBED_COGITOR_IOT_TOPIC_SHORT` | bool | n | Use short topic style |

### Metrics (`embed_core`)

| Symbol | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_EMBED_METRICS_INTERVAL_MS` | int | 10000 | Collection interval |
| `CONFIG_EMBED_METRICS_ENABLE_STORAGE` | bool | y | Collect SPIFFS metrics |
| `CONFIG_EMBED_METRICS_ENABLE_CPU` | bool | y | CPU load (needs `FREERTOS_GENERATE_RUN_TIME_STATS`) |

### LED Strip (`embed_extra`)

| Symbol | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_EMBED_LED_STRIP` | bool | y | Enable LedStripService |
| `CONFIG_EMBED_LED_STRIP_MAX` | int | 4 | Max concurrent strips |
| `CONFIG_EMBED_LED_STRIP_DEFAULT_BRIGHTNESS` | int | 32 | Default brightness |

### Camera & MJPEG (`embed_extra`)

| Symbol | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_EMBED_CAMERA_BOARD` | choice | `FREENOVE_S3` | Camera pinout |
| `CONFIG_EMBED_CAMERA_XCLK_FREQ_HZ` | int | 10000000 | XCLK frequency |
| `CONFIG_EMBED_CAMERA_JPEG_QUALITY` | int | 10 | JPEG quality (1–63) |
| `CONFIG_EMBED_CAMERA_FB_COUNT` | int | 2 | DMA frame buffers |
| `CONFIG_EMBED_CAMERA_CAPTURE_INTERVAL_MS` | int | 50 | Capture interval |
| `CONFIG_EMBED_MJPEG_PORT` | int | 80 | MJPEG HTTP port |
| `CONFIG_EMBED_MJPEG_QUEUE_DEPTH` | int | 2 | Frame queue depth |
| `CONFIG_EMBED_MJPEG_STACK_SIZE` | int | 8192 | HTTP task stack |

---

## 11. Appendix: MQTT Topic Map

Device: `product_id=home`, `device_id=esp32-s3`  
Client ID: `home.esp32-s3`

| Capability | Full Topic | Short Topic |
|------------|------------|-------------|
| Status (LWT) | `iot/v1/home/esp32-s3/up/status` | `v1/s` |
| Telemetry | `…/up/telemetry/data` | `v1/t` |
| Events | `…/up/events/post` | `v1/e` |
| Attr report | `…/up/attributes/report` | `v1/a` |
| Attr request | `…/up/attributes/request` | `v1/a/req` |
| Attr response | `…/down/attributes/response` | `v1/a/res` |
| Attr update | `…/down/attributes/update` | `v1/a/upd` |
| RPC request | `…/down/rpc/request` | `v1/r/req` |
| RPC response | `…/up/rpc/response` | `v1/r/res` |
| Client RPC req | `…/up/rpc/request` | `v1/r/creq` |
| Client RPC res | `…/down/rpc/response` | `v1/r/cres` |
| NTP req | `…/up/ntp/request` | `v1/n/req` |
| NTP res | `…/down/ntp/response` | `v1/n/res` |
| OTA version | `…/up/ota/version` | `v1/o/ver` |
| OTA query | `…/up/ota/query` | `v1/o/q` |
| OTA update | `…/down/ota/update` | `v1/o/upd` |
| OTA cancel | `…/down/ota/cancel` | `v1/o/can` |
| OTA progress | `…/up/ota/progress` | `v1/o/p` |
| Logs | `…/up/logs/report` | `v1/l` |
| Device sub | `…/down/#` | `v1/#` |

**Correlation:** All request/response pairs carry `"id"` in the JSON body (uint32). Topics are fixed strings.
