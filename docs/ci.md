# CI / CD (Gitea Actions)

Workflows use **GitHub Actions–compatible** YAML for [Gitea Actions](https://docs.gitea.com/usage/actions/overview) (`act_runner`) and GitHub.

| Path | Purpose |
|------|---------|
| [`.gitea/workflows/ci.yml`](../.gitea/workflows/ci.yml) | Preferred for Gitea |
| [`.github/workflows/ci.yml`](../.github/workflows/ci.yml) | Mirror for GitHub |

## Why not `container: espressif/idf`?

`actions/checkout` and `actions/upload-artifact` run with **Node.js**. The official IDF image has no `node`, so Gitea fails with:

```text
exec: "node": executable file not found in $PATH  (exit 126)
```

**Fix used here:** job runs on the host runner; IDF builds use `docker run … espressif/idf:…`.

## Jobs

| Job | What |
|-----|------|
| `unity-tests` | Builds `test_apps/embed_unity` |
| `firmware` | Main firmware (`partitions.csv`) |
| `firmware-ota` | `sdkconfig.defaults` + `sdkconfig.defaults.ota` |

Device flash is not part of CI.

## Gitea setup

1. Enable **Actions** on the repo.
2. Register [`act_runner`](https://docs.gitea.com/usage/actions/act-runner) with **Docker** (docker.sock / privileged as needed).
3. Runner label must match `runs-on` (`ubuntu-latest`).
4. Runner must be able to pull `espressif/idf:v5.5.1`.

If `docker run` fails with permission errors, grant the runner user access to the Docker socket.

## Local equivalents

```bash
# Unity
cd test_apps/embed_unity && idf.py set-target esp32s3 && idf.py build

# Firmware (factory)
idf.py set-target esp32s3 && idf.py build

# Firmware (OTA)
idf.py set-target esp32s3
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.ota" build
```
