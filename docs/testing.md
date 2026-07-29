# Testing (Unity)

ESP-IDF Unity tests for header-only / framework primitives.

## Recommended: standalone test app

```bash
cd test_apps/embed_unity
idf.py set-target esp32s3
idf.py build flash monitor
```

`main/test_app_main.cpp` registers `TEST_CASE`s and runs `unity_run_all_tests()` from `app_main`.

Covered today:

- `embed::string` empty / truncate / assign
- `embed::Message` concept for POD + `embed::string`

## Component `test/` tree

Mirror of the same cases lives in `components/embed/test/` for the IDF unit-test-app workflow:

```bash
# From ESP-IDF (adjust EXTRA path to this repo's components/)
idf.py -C "$IDF_PATH/tools/unit-test-app" \
  -D EXTRA_COMPONENT_DIRS="<path-to-repo>/components" \
  -T embed \
  set-target esp32s3

idf.py -C "$IDF_PATH/tools/unit-test-app" build flash monitor
```

Prefer `test_apps/embed_unity` for day-to-day work in this repository.

## Adding tests

1. Prefer new cases in `test_apps/embed_unity/main/` (and optionally mirror under `components/embed/test/`).
2. Tag with `[embed][<area>]`.
3. Keep tests free of WiFi/camera hardware when possible.
4. C++20 + RTTI are enabled for the test app (needed by the framework headers).
