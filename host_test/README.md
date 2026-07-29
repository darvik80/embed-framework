# Host tests (no ESP-IDF, no flash)

Unit tests for header-only `embed` primitives on the build machine.

## Run locally

```bash
cmake -S host_test -B host_test/build
cmake --build host_test/build
ctest --test-dir host_test/build --output-on-failure
# or:
./host_test/build/test_embed_primitives
```

Requirements: CMake ≥ 3.16, C++20 compiler, network once (FetchContent pulls [Unity](https://github.com/ThrowTheSwitch/Unity) v2.6.1).

## Scope

| Covered | Not covered (need device / mocks) |
|---------|-----------------------------------|
| `embed::string` | `EventLoop` / `Signal` / `Slot` |
| `embed::Message` concept | WiFi, MQTT, camera, OSS |

Device Unity app remains under `test_apps/embed_unity`.
