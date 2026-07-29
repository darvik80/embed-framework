---
name: embed-new-component
description: >-
  Create a new ESP-IDF component for embed-framework (CMakeLists, idf_component.yml,
  include/src layout, Kconfig). Use when adding a new components/* package, cloud
  provider, or splitting a feature into its own IDF component.
---

# Add a new ESP-IDF component

Match existing components under `components/`. Read [embed-framework](../embed-framework/SKILL.md) for layering.

## Layout

```
components/<name>/
  CMakeLists.txt
  idf_component.yml
  Kconfig.projbuild          # optional
  include/<name>/...hpp      # public
  src/...cpp / ...c          # private
  certs/                     # optional TLS material
```

## CMakeLists.txt

```cmake
idf_component_register(
    SRCS
        "src/foo_service.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        embed
        embed_core
    PRIV_REQUIRES
        log
        freertos
)
```

- Framework users: `REQUIRES embed` (and `embed_core` if using WiFi/MQTT types).
- Keep IDF drivers in `PRIV_REQUIRES` when not part of the public API.
- Do not put public headers in `src/` (avoid `alicloud_common` legacy pattern for new code).

## idf_component.yml

```yaml
dependencies:
  idf:
    version: '>=5.5'
description: One-line purpose
version: 1.0.0
```

Add managed deps (e.g. `espressif/esp32-camera`) only when needed; pin compatible ranges like `embed_extra`.

## Kconfig.projbuild

Prefix options `EMBED_<AREA>_…` → `CONFIG_EMBED_…`. Defaults must be safe placeholders (empty secrets, guest SSIDs only if documented as demo).

## Namespaces

| Layer | Namespace |
|-------|-----------|
| Framework / core / extra services | `embed::` |
| Alibaba | `alicloud::iot` / OSS types as existing |
| Other vendors | `<vendor>::iot` or `<vendor>::` |

## Wire into the app

1. `main/CMakeLists.txt` — add component to `REQUIRES` if not auto-discovered via dependency chain.
2. `main/main.cpp` — `#include`, `registry.createService<...>()`.
3. Prefer optional features commented with a one-line reason (see existing camera/OSS comments).

## C vs C++

- New services: C++ headers/impl in the style of `embed_core`.
- Low-level protocol clients may be C with a thin `*Service` C++ wrapper (`alicloud_oss` pattern).
- Stick to C++20 for new C++ unless the file already uses another dialect.
