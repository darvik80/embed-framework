# PRD Diagrams — embed-framework

> Extracted from [prd.md](prd.md). Each diagram is self-contained and can be rendered by any Mermaid-compatible viewer.

---

# Component Dependency Layering

Full framework component hierarchy — from application wiring down through cloud SDKs, peripheral services, platform services, to the core framework primitives. Dependencies flow downward only.

```mermaid
%%{init: {
  "flowchart": {
    "htmlLabels": false,
    "curve": "linear",
    "nodeSpacing": 50,
    "rankSpacing": 70,
    "wrappingWidth": 200
  }
}}%%
flowchart TB
    subgraph App["main (product wiring)"]
        AppCode[app_main / RPC handlers]
    end

    subgraph Cloud["Cloud SDKs (pluggable)"]
        Cogitor["cogitor_iot"]
        Ali["alicloud_iot / alicloud_oss"]
        TB["thingsboard"]
    end

    subgraph Extra["embed_extra — peripherals"]
        Cam[CameraService]
        Mjpeg[MjpegService]
        Led[LedStripService]
    end

    subgraph Core["embed_core — platform services"]
        Wifi[WifiService]
        Mqtt[MqttService]
        Metrics[MetricsService]
        Nvs[NvsStore / device_settings / firmware_slot]
        Portal[ConfigPortalService]
    end

    subgraph Base["embed — core framework"]
        Svc[Service & ServiceRegistry]
        Loop[EventLoop]
        Sig[Signal / Slot]
        SM[StateMachine CRTP]
        Pod[Trivially-copyable containers]
        Crypto[Crypto SHA/MD5/HMAC]
    end

    AppCode --> Cogitor
    AppCode --> Ali
    AppCode --> TB
    AppCode --> Cam
    AppCode --> Mjpeg
    AppCode --> Led
    AppCode --> Wifi
    AppCode --> Mqtt
    AppCode --> Metrics
    AppCode --> Nvs
    AppCode --> Portal
    Cogitor --> Mqtt
    Cogitor --> Portal
    Cam --> Mjpeg
    Mqtt --> Wifi
    Metrics --> Nvs
    Wifi --> Nvs
    Portal --> Wifi
    Mqtt --> Svc
    Metrics --> Svc
    Wifi --> Svc
    Cam --> Svc
    Mjpeg --> Svc
    Led --> Svc
    Svc --> Loop
    Svc --> Sig
    Svc --> SM
    Sig --> Pod
    SM --> Pod
    Svc --> Crypto
    Base -. no upward dependencies .-> Base
```

---

# embed — Class Diagram

Core framework class relationships: Service, ServiceRegistry, EventLoop, Signal/Slot, ConnectionPool, StateMachine, and their interactions.

```mermaid
%%{init: {}}%%
classDiagram
    class Service {
        <<abstract>>
        +serviceName() const char*
        +start() virtual
        +stop() virtual
    }

    class ServiceRegistry {
        <<singleton>>
        -storage_ : ServiceStorage[EMBED_MAX_SERVICES]
        -services_ : Entry[EMBED_MAX_SERVICES]
        -mutex_ : SemaphoreHandle_t
        +createService~T~() T*
        +getService~T~() T*
        +hasService~T~() bool
        +startAll() void
        +stopAll() void
        +count() size_t
    }

    class EventLoop {
        <<singleton>>
        -initialized_ : bool
        +init() void
        +deinit() void
        +post~M~() esp_err_t
        +registerHandler() esp_err_t
    }

    class ConnectionPool {
        <<singleton>>
        -entries_ : Entry[EMBED_MAX_CONNECTIONS]
        +allocate() Connection
        +release() void
    }

    class Signal~M~ {
        -base_ : esp_event_base_t
        +emit(msg) void
        +eventBase() esp_event_base_t
    }

    class Slot~M~ {
        -callback_ : Callback
        -ctx_ : void*
        -connection_ : Connection
        +connect(signal) Connection&
        +disconnect() void
        +isConnected() bool
    }

    class Connection {
        -index_ : int
        +connected() bool
        +disconnect() void
    }

    class StateMachine~T,States~ {
        -currentState : variant~States*~
        -prevState : variant~States*~
        +handle(event) void
        +transitionTo~State~() void
        +getCurrentState() const
    }

    Service <|-- ServiceRegistry : manages
    Service <.. StateMachine : CRTP base
    Signal~M~ ..> EventLoop : posts via
    Slot~M~ ..> EventLoop : registers handler
    Connection ..> ConnectionPool : slot index
    Signal~M~ ..> Slot~M~ : typed contract (M)
    ServiceRegistry ..> Service : placement-new
```

---

# embed — Data Flow

In-process event bus: how messages flow from a producer task through the EventLoop to Slot callbacks and consumer state.

```mermaid
%%{init: {
  "sequence": {
    "mirrorActors": false,
    "useMaxWidth": true,
    "wrap": true,
    "wrapPadding": 12
  }
}}%%
sequenceDiagram
    participant Producer as Producer task<br/>(e.g. MQTT worker)
    participant EvtLoop as EventLoop<br/>(embed_evt task)
    participant Slot as Slot callback<br/>(Service::start)
    participant Consumer as Consumer state

    Producer->>EvtLoop: esp_event_post(base, id, &msg, sizeof(M), 100ms)
    Note over EvtLoop: enqueue or timeout
    EvtLoop-->>Slot: dispatch on dedup'd task
    Slot->>Consumer: onStateChanged(TransitionTo<...>)  or direct call
    Consumer-->>Consumer: apply effect, may emit another Signal
```

---

# embed_core — Architecture

Platform services (WiFi, MQTT, Metrics, NVS, ConfigPortal) and their dependencies on embed primitives and ESP-IDF drivers.

```mermaid
%%{init: {
  "flowchart": {
    "htmlLabels": false,
    "curve": "linear",
    "nodeSpacing": 50,
    "rankSpacing": 70,
    "wrappingWidth": 200
  }
}}%%
flowchart LR
    subgraph embed_core
        W[WifiService]
        M[MqttService]
        Met[MetricsService]
        N[NvsStore]
        DS[device_settings]
        FS[firmware_slot]
        CP[ConfigPortalService]
    end

    subgraph embed
        SR[ServiceRegistry]
        EL[EventLoop]
        SIG[Signal/Slot]
        SM[StateMachine]
    end

    subgraph ESP-IDF
        WiFi[esp_wifi]
        MQTT[esp-mqtt]
        NVS[nvs_flash / nvs]
        SPIF[esp_spiffs]
        Timers[esp_timer]
    end

    W --> WiFi
    M --> MQTT
    N --> NVS
    Met --> SPIF
    Met --> Timers
    DS --> N
    FS --> ESP-IDF[esp_ota]

    W --> SR
    M --> SR
    Met --> SR
    CP --> SR

    W --> SM
    M --> SM
    W -->|onConnected| SIG
    M -->|onMessage| SIG
    Met -->|onMetricsCollected| SIG

    W -->|Slot<MqttConnected>| M
    N -->|Slot<WifiConnected>| W
```

---

# embed_core — Startup & Data Flow

Application startup sequence: NVS init, EventLoop init, service creation, WiFi connect, MQTT connect, metrics collection.

```mermaid
%%{init: {
  "sequence": {
    "mirrorActors": false,
    "useMaxWidth": true,
    "wrap": true,
    "wrapPadding": 12
  }
}}%%
sequenceDiagram
    participant App as app_main
    participant SR as ServiceRegistry
    participant Wifi as WifiService
    participant MQTT as MqttService
    participant M as MetricsService
    participant NVS as NVS fctry
    participant EvtLoop as embed_evt
    participant Net as WiFi / IP stack

    App->>NVS: NvsStore::initFlash()
    App->>EvtLoop: EventLoop::instance().init()
    App->>SR: createService<WifiService>()
    App->>SR: createService<MetricsService>()
    App->>SR: createService<MqttService>(creds)
    App->>SR: startAll()
    SR->>Wifi: start() (load SSID, init WiFi)
    Wifi->>NVS: loadWifiSettings
    Wifi->>Net: esp_wifi_start()
    Net-->>Wifi: WIFI_EVENT_STA_CONNECTED / IP acquired
    Wifi->>EvtLoop: emit WifiConnected{ip}
    EvtLoop-->>MQTT: Slot onWifiConnected fires
    MQTT->>Net: esp_mqtt_client_start()
    Net-->>MQTT: MQTT_EVENT_CONNECTED
    MQTT->>EvtLoop: emit MqttConnected{brokerUri}
    M->>EvtLoop: emit MetricsCollected (every 10 s)
```

---

# embed_extra — Architecture

Peripheral services (Camera, MJPEG, LED strip) and their dependencies on ESP-IDF drivers and the core framework.

```mermaid
%%{init: {
  "flowchart": {
    "htmlLabels": false,
    "curve": "linear",
    "nodeSpacing": 50,
    "rankSpacing": 70,
    "wrappingWidth": 200
  }
}}%%
flowchart LR
    subgraph embed_extra
        Cam[CameraService]
        Mjpeg[MjpegService]
        Led[LedStripService]
    end

    subgraph Drivers
        esp[esp32-camera]
        jpeg[esp_jpeg]
        http[esp_http_server]
        rmt[driver/rmt]
        led_strip[led_strip]
    end

    subgraph embed
        SR[ServiceRegistry]
        SIG[Signal/Slot]
    end

    Cam --> esp
    Cam --> jpeg
    Mjpeg --> http
    Mjpeg -->|Slot<CameraFrame>| Cam
    Led --> rmt
    Led --> led_strip

    Cam --> SR
    Mjpeg --> SR
    Led --> SR

    Cam -->|Signal<CameraFrame>| SIG
    Led -->|Signal<LedStripChanged>| SIG
```

---

# embed_extra — Camera & LED Data Flow

Camera capture loop (frame buffer → MJPEG stream) and LED strip RPC control flow.

```mermaid
%%{init: {
  "sequence": {
    "mirrorActors": false,
    "useMaxWidth": true,
    "wrap": true,
    "wrapPadding": 12
  }
}}%%
sequenceDiagram
    participant Cam as Camera driver
    participant CSvc as CameraService
    participant Mjpeg as MjpegService
    participant HTTP as Browser
    participant Led as LedStripService
    participant RPC as Cogitor RPC

    loop every CONFIG_EMBED_CAMERA_CAPTURE_INTERVAL_MS
        Cam->>CSvc: FB ready
        CSvc->>CSvc: build CameraFrame{data,len,seq,fb}
        CSvc->>Mjpeg: onFrame slot (queue)
        Mjpeg-->>HTTP: multipart/x-mixed-replace JPEG
        Mjpeg-->>CSvc: releaseCameraFrame (returns fb to driver)
    end

    RPC->>Led: rpc set_led {gpio, offset, length, r, g, b, on}
    Led->>Led: setRange + refresh
    Led-->>RPC: onChanged (or empty ack)
```

---

# cogitor_iot — Architecture

Cogitor IoT Platform SDK internals: credentials, topics, IotService, telemetry, attributes, RPC, OTA, NTP, device info, metrics bridge, and config portal.

```mermaid
%%{init: {
  "flowchart": {
    "htmlLabels": false,
    "curve": "linear",
    "nodeSpacing": 50,
    "rankSpacing": 70,
    "wrappingWidth": 200
  }
}}%%
flowchart LR
    subgraph cogitor_iot
        Creds[CogitorCredentials]
        Topics[Topics]
        Iot[IotService]
        Tel[TelemetryBuilder/Batch]
        Attr[AttributeBuilder/Request]
        RPC[RpcRegistry]
        OTA[OtaService]
        NTP[NtpService]
        Info[DeviceInfo]
        Bridge[MetricsTelemetryBridge]
        Log[log_service]
        Portal[ConfigPortalService]
    end

    subgraph embed_core
        Mqtt[MqttService]
        Wifi[WifiService]
        Metrics[MetricsService]
        Nvs[NvsStore / device_settings]
        FS[firmware_slot]
    end

    subgraph embed
        SR[ServiceRegistry]
        SIG[Signal/Slot]
    end

    Creds -->|implements| Mqtt
    Iot -->|Slot<MqttConnected>| Mqtt
    Iot -->|Slot<MqttMessageReceived>| Mqtt
    OTA -->|Slot<OtaUpdate>| Iot
    Bridge -->|Slot<MetricsCollected>| Metrics
    Info -->|Slot<MqttConnected>| Iot
    Portal --> Nvs
    Portal --> Wifi
    Iot --> Topics
    Iot --> Tel
    Iot --> Attr
    Iot --> RPC
    OTA --> FS
```

---

# cogitor_iot — Full Data Flow

End-to-end flow: credentials loading, service creation, MQTT connect, telemetry publish, RPC dispatch, OTA update with crash-loop guard.

```mermaid
%%{init: {
  "sequence": {
    "mirrorActors": false,
    "useMaxWidth": true,
    "wrap": true,
    "wrapPadding": 12
  }
}}%%
sequenceDiagram
    participant App as app_main
    participant Creds as CogitorCredentials
    participant Mqtt as MqttService
    participant Iot as IotService
    participant Met as MetricsService
    participant Bridge as MetricsTelemetryBridge
    participant Platform as Cogitor Platform
    participant OTA as OtaService
    participant FS as firmware_slot

    App->>Creds: loadOrSeedCredentials (NVS fctry + Kconfig)
    App->>Mqtt: createService<MqttService>(*creds)
    App->>Iot: createService<IotService>(*creds)
    App->>Met: createService<MetricsService>()
    App->>Bridge: createService<MetricsTelemetryBridge>()
    App->>OTA: createService<OtaService>()
    App->>FS: checkCrashLoopRollback()
    App->>App: startAll()

    Mqtt->>Platform: CONNECT (LWT on up/status)
    Platform-->>Mqtt: CONNACK
    Mqtt->>Iot: MqttConnected
    Iot->>Platform: SUBSCRIBE down/#
    Bridge->>Met: Slot<MetricsCollected>
    Met->>Bridge: MetricsCollected (every 10 s)
    Bridge->>Iot: publishTelemetry(json)
    Iot->>Platform: PUBLISH v1/t

    Platform->>Iot: v1/r/req {id,method,params}
    Iot->>Iot: dispatch via RpcRegistry
    Iot->>Platform: v1/r/res {id,code,message,data}

    Platform->>Iot: v1/o/upd {version,url,sha256}
    Iot->>OTA: OtaUpdate signal
    OTA->>OTA: download (HTTPS) → verify → flash → reboot
    OTA->>Platform: v1/o/p progress (0..100)
    Platform-->>Mqtt: CONNACK (next boot)
    Mqtt->>FS: noteFirmwareConfirmed() (first connect)
```
