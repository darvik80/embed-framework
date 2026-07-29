# Testing

## Host Unity (no flash, no ESP-IDF) — preferred for CI

```bash
cmake -S host_test -B host_test/build
cmake --build host_test/build
ctest --test-dir host_test/build --output-on-failure
```

See [host_test/README.md](../host_test/README.md).

Covers `embed::string` and `embed::Message`.

## Device Unity app

```bash
cd test_apps/embed_unity
idf.py set-target esp32s3
idf.py build flash monitor
```

Same primitives on chip via ESP-IDF Unity (`TEST_CASE` / `unity_run_all_tests`).

## Component `test/` tree

`components/embed/test/` for the IDF unit-test-app (`idf.py -T embed`).

## Adding tests

| Kind | Where |
|------|--------|
| Pure logic / POD / string | `host_test/` (+ optional mirror in `test_apps/embed_unity`) |
| Needs FreeRTOS / esp_event | `test_apps/embed_unity` or component `test/` on device / `linux` target |
| Hardware | device smoke only |
