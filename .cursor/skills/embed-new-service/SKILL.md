---
name: embed-new-service
description: >-
  Scaffold a new embed::Service in this ESP-IDF project (header, cpp, CMake,
  Kconfig, Message/Signal/Slot wiring). Use when the user asks to add a service,
  create XxxService, or wire a new feature into ServiceRegistry.
---

# Add a new Service

Follow [embed-framework](../embed-framework/SKILL.md). Prefer extending an existing component (`embed_core` / `embed_extra` / `alicloud_oss` / cloud) over a new component unless the feature is a separate dependency domain.

## Checklist

```
- [ ] Choose component (or create via embed-new-component)
- [ ] Define Message structs + static_assert(Message<...>)
- [ ] Declare XxxService : public embed::Service (+ StateMachine if lifecycle SM fits)
- [ ] Public Signal<> for outbound; private Slot<> + static handlers for inbound
- [ ] Implement start()/stop(); resolve peers only in start()
- [ ] sizeof(XxxService) fits EMBED_SERVICE_SIZE or move heavy state to heap
- [ ] Register SRCS in component CMakeLists.txt
- [ ] Add Kconfig options if tunable; use CONFIG_EMBED_* in code
- [ ] Wire createService<XxxService>(...) in main/main.cpp (commented if optional)
- [ ] Log with TAG; no blocking work in Slot callbacks
```

## Header skeleton

```cpp
#pragma once
#include "embed/embed.hpp"

namespace embed {  // or vendor::domain

struct FooReady {
    uint32_t id;
};
static_assert(embed::Message<FooReady>);

class FooService : public embed::Service {
public:
    FooService() = default;
    const char* serviceName() const override { return "FooService"; }
    void start() override;
    void stop() override;

    Signal<FooReady> onReady;

private:
    Slot<embed::WifiConnected> wifiConnectedSlot_{onWifiConnected, this};
    static void onWifiConnected(const embed::WifiConnected& msg, void* ctx);

    // members_
};

} // namespace
```

## start() pattern

```cpp
void FooService::start() {
    auto* wifi = ServiceRegistry::instance().getService<WifiService>();
    if (wifi)
        wifiConnectedSlot_.connect(wifi->onConnected);
    // init hardware / timers / tasks
}
```

## Data-plane note

If the service produces large buffers (frames, files): use a FreeRTOS queue to **one** consumer task; document who calls `esp_camera_fb_return` / `free`. Do **not** put owning `camera_fb_t*` in a multi-subscriber Signal.

## Credentials

Broker auth → implement `embed::MqttCredentials`, construct `static` in `app_main`, pass by reference to `MqttService`. See `PlainMqttCredentials` in `main/main.cpp` and `alicloud::iot::AlicloudCredentials`.
