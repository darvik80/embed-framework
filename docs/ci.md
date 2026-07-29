# CI / CD (Gitea Actions)

Workflows: [`.gitea/workflows/ci.yml`](../.gitea/workflows/ci.yml) (mirror under `.github/workflows/`).

## Jobs

| Job | Needs | What |
|-----|--------|------|
| **`host-tests`** | cmake, g++, git | Builds & **runs** Unity in `host_test/` (no IDF, no flash) |
| `embed-unity` | Docker + `tools/ci/idf-docker.sh` | Builds `test_apps/embed_unity` for esp32s3 |
| `firmware` | same | Main firmware build |
| `firmware-ota` | same | OTA partition table build |

`host-tests` is the fast quality gate. IDF jobs run in parallel after it (`needs: host-tests`).

## Common Gitea failures

| Error | Fix |
|-------|-----|
| `exec: "node"` (126) | Don’t put `actions/checkout` inside `container: espressif/idf` |
| `Bind mount failed: '/workspace/...'` (125) | Use `tools/ci/idf-docker.sh` (`--volumes-from`) |
| `tools/ci/idf-docker.sh: No such file` | Commit & push `tools/ci/idf-docker.sh` |

## Local

```bash
# Host tests (CI gate)
cmake -S host_test -B host_test/build && cmake --build host_test/build
ctest --test-dir host_test/build --output-on-failure

# Device Unity app (build only)
cd test_apps/embed_unity && idf.py set-target esp32s3 && idf.py build

# Firmware
idf.py set-target esp32s3 && idf.py build
```
